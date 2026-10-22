/*
 * main.c — pico-keystroker firmware entry point (RP2040/RP2350).
 *
 * Barebones TinyUSB HID keyboard, no QMK. Flow:
 *   1. hal_init() (button GPIO with pull-up, tusb_init()), LED init.
 *   2. Load config: keystroker_config_text via config_set_defaults() +
 *      config_parse_string() + config_validate(); on any failure keep
 *      compiled-in defaults.
 *   3. Loop: tud_task(); LED mirrors mount state.
 *      - WRITE_MODE_TIME:   after countdown_ms elapses since mount, type the
 *        embedded input exactly once.
 *      - WRITE_MODE_BUTTON: poll hal_button_read() through button_update()
 *        (debounce + double-click suppression inside src/common/button.c),
 *        type the input once per BUTTON_EVENT_PRESS.
 *   4. Typing: src/common/typing_engine.c turns the payload into press/release
 *      actions (hid_keycode_for_ascii per LANGUAGE, '\n' -> ENTER, unmapped
 *      chars skipped); each action goes out via hal_hid_send() with
 *      typing_delay_ms between actions.
 *
 * Embedded payload: tools/embed.py generates generated/payload.h with
 *   static const unsigned char keystroker_payload_text[] (+ _len);
 *   static const unsigned char keystroker_config_text[]  (+ _len);
 * Both arrays are NUL-terminated (lens exclude the NUL). The header is
 * #included only if present; otherwise compiled-in fallback arrays below
 * (same symbol names, same layout) are used.
 *
 * Sibling-owned interfaces used here (src/common and tools dirs: read-only):
 *   hal.h            hal_init/hal_is_mounted/hal_hid_ready/hal_hid_send/
 *                    hal_delay_ms/hal_button_read. NOTE: no hal_millis() in
 *                    this HAL, so time comes from the SDK directly (now_ms()).
 *   config_parser.h  keystroker_config_t (write_mode is write_mode_t enum,
 *                    language is lang_t); config_set_defaults/
 *                    config_parse_string/config_validate (CFG_OK == 0).
 *   hid_keymap.h     hid_keycode_for_ascii(char, lang_t, *kc, *mod); used
 *                    indirectly through typing_engine.
 *   button.h         button_t state + button_init(b, debounce_ms, ignore_ms)
 *                    + button_update(b, raw, now_ms); ignore_ms comes from
 *                    config.double_click_ms. Double-click suppression lives
 *                    in the module (BUTTON_EVENT_DROPPED is ignored here).
 *   typing_engine.h  typing_engine_init/type_string with hid_action_cb_t.
 * config.button_pin is validated but the firmware uses the board default pin
 * (hal_init() owns GPIO); choose it at build time with
 * -DKEYSTROKER_BUTTON_PIN=....
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "board_config.h"
#include "hal.h"
#include "config_parser.h"
#include "hid_keymap.h"
#include "button.h"
#include "typing_engine.h"

#if defined(__has_include)
#if __has_include("payload.h")
#include "payload.h"
#define KEYSTROKER_HAVE_PAYLOAD 1
#endif
#endif

#ifndef KEYSTROKER_HAVE_PAYLOAD
/* Fallback payload: same symbols/layout as tools/embed.py output. */
static const unsigned char keystroker_payload_text[] = "Pico Keystroker by Arda Kilicdagi\n"
                                                   "https://github.com/Ardakilic/pico-keystroker\n";
static const size_t keystroker_payload_text_len =
    sizeof(keystroker_payload_text) - 1u;
static const unsigned char keystroker_config_text[] =
    "WRITE_MODE=button\n"
    "COUNTDOWN_MS=5000\n"
    "HID_KEYBOARD_NAME=Arboard-2040-pico-keystroker\n"
    "LANGUAGE=en_US\n"
    "TYPING_DELAY_MS=8\n"
    "DOUBLE_CLICK_MS=400\n"
    "DEBOUNCE_MS=30\n"
    "BUTTON_PIN=15\n";
static const size_t keystroker_config_text_len =
    sizeof(keystroker_config_text) - 1u;
#endif

static keystroker_config_t g_cfg;
static button_t g_button;

/* Milliseconds since boot. The HAL has no clock API, so the SDK is used
 * directly (RP2040/RP2350 portable). Unsigned math stays wrap-safe. */
static uint32_t now_ms(void) {
  return to_ms_since_boot(get_absolute_time());
}

#if KEYSTROKER_LED_PIN >= 0
static void led_init(void) {
  gpio_init((unsigned int)KEYSTROKER_LED_PIN);
  gpio_set_dir((unsigned int)KEYSTROKER_LED_PIN, GPIO_OUT);
  gpio_put((unsigned int)KEYSTROKER_LED_PIN, 0);
}

static void led_set(bool on) {
  gpio_put((unsigned int)KEYSTROKER_LED_PIN, on ? 1u : 0u);
}
#else
static void led_init(void) {
}

static void led_set(bool on) {
  (void)on;
}
#endif

/* Cooperative delay that keeps the USB device task running. */
static void delay_pump(uint32_t ms) {
  uint32_t end = now_ms() + ms;
  while ((uint32_t)(now_ms() - end) < 0x80000000u) {
    tud_task();
    sleep_ms(1);
  }
}

static void ack_blink(void) {
  int i;
  for (i = 0; i < 3; i++) {
    led_set(true);
    delay_pump(60);
    led_set(false);
    delay_pump(60);
  }
}

/* typing_engine callback: push one press/release action to the host. */
static void emit_action(const hid_action_t *action, void *user) {
  const typing_engine_t *eng = (const typing_engine_t *)user;
  uint8_t keys[6] = { 0, 0, 0, 0, 0, 0 };
  uint32_t t0;

  if (action == NULL) {
    return;
  }
  if (!action->is_release) {
    keys[0] = action->keycode;
  }
  t0 = now_ms();
  while (!hal_hid_ready()) {
    tud_task();
    if ((uint32_t)(now_ms() - t0) > 2000u) {
      return; /* Host went away; drop the keystroke. */
    }
  }
  hal_hid_send(action->modifier, keys);
  delay_pump(typing_engine_delay_ms(eng));
}

static void type_all(void) {
  typing_engine_config_t tec;
  typing_engine_t eng;

  if (keystroker_payload_text_len == 0 ||
      keystroker_payload_text[keystroker_payload_text_len] != '\0') {
    return; /* Malformed generator output; type nothing. */
  }
  tec.language = g_cfg.language;
  tec.typing_delay_ms = g_cfg.typing_delay_ms;
  typing_engine_init(&eng, &tec);
  typing_engine_type_string(&eng, (const char *)keystroker_payload_text,
                            emit_action, &eng);
}

/* Embedded config text, or NULL when the generator output is malformed. */
static const char *config_text(void) {
  if (keystroker_config_text_len == 0 ||
      keystroker_config_text[keystroker_config_text_len] != '\0') {
    return NULL;
  }
  return (const char *)keystroker_config_text;
}

int main(void) {
  const char *cfg_text;
  uint32_t mount_ms = 0;
  uint32_t now;
  bool mounted;
  bool was_mounted = false;
  bool typed_once = false;

  hal_init();
  led_init();

  config_set_defaults(&g_cfg);
  cfg_text = config_text();
  if (cfg_text == NULL ||
      config_parse_string(cfg_text, &g_cfg) != CFG_OK ||
      config_validate(&g_cfg) != CFG_OK) {
    config_set_defaults(&g_cfg);
  }
  button_init(&g_button, g_cfg.debounce_ms, g_cfg.double_click_ms);

  while (true) {
    tud_task();
    mounted = hal_is_mounted();
    led_set(mounted);
    now = now_ms();
    if (mounted && !was_mounted) {
      mount_ms = now; /* Countdown starts at mount, per spec. */
    }
    was_mounted = mounted;
    if (!mounted) {
      continue;
    }

    if (g_cfg.write_mode == WRITE_MODE_TIME) {
      if (!typed_once && (uint32_t)(now - mount_ms) >= g_cfg.countdown_ms) {
        typed_once = true;
        type_all();
        ack_blink();
      }
    } else {
      /* WRITE_MODE_BUTTON (also the compiled-in default). */
      if (button_update(&g_button, hal_button_read(), now) ==
          BUTTON_EVENT_PRESS) {
        type_all();
        ack_blink();
      }
    }
  }
}
