/* main.cpp — ESP32-S3 USB-HID keystroker firmware.
 *
 * Arduino setup()/loop() around the shared src/common core, mirroring
 * targets/rp2040/main.c:
 *   1. setup(): hal_init() (button pull-ups + UART0 Serial), load config
 *      (keystroker_config_text via config_set_defaults() +
 *      config_parse_string() + config_validate(); on any failure keep
 *      compiled-in defaults), button_init(), then Keyboard.begin() +
 *      USB.manufacturerName()/productName() + USB.begin() (strings must
 *      be set before USB.begin() — they are USB descriptors fixed at
 *      enumeration time).
 *   2. loop(): tick() runs one super-loop iteration —
 *      - WRITE_MODE_TIME:   after countdown_ms elapses since the mount
 *        edge (hal_is_mounted() rising edge), type the embedded input
 *        exactly once.
 *      - WRITE_MODE_BUTTON: poll hal_button_read() through button_update()
 *        (debounce + double-click suppression inside src/common/button.c),
 *        type the input once per BUTTON_EVENT_PRESS.
 *   3. Typing: src/common/typing_engine.c turns the payload into
 *      press/release actions (hid_keycode_for_ascii per LANGUAGE,
 *      '\n' -> ENTER, unmapped chars skipped); each action goes out via
 *      hal_hid_send() with typing_delay_ms between actions.
 *
 * Embedded payload: tools/embed.py generates payload.h with
 *   static const unsigned char keystroker_payload_text[] (+ _len);
 *   static const unsigned char keystroker_config_text[]  (+ _len);
 * Copy generated/payload.h next to this file (or add -I generated) so
 * __has_include finds it; otherwise the compiled-in fallback below
 * (same symbols, same layout) is used.
 *
 * Tick source: millis() (Arduino). Unsigned math stays wrap-safe.
 * TinyUSB device work runs in the Arduino-ESP32 background task, so this
 * file never pumps USB directly.
 *
 * NOTE on naming: the runtime config field HID_KEYBOARD_NAME is
 * parsed/validated but CANNOT rename the USB device after enumeration —
 * what the host sees comes from the HID_KEYBOARD_NAME compile-time macro
 * (board_config.h) passed to USB.productName() in setup(). Rebuild to
 * change it.
 */

#include <Arduino.h>

#include "USB.h"
#include "USBHIDKeyboard.h"

#include "board_config.h"
#include "hal.h"
#include "config_parser.h"
#include "hid_keymap.h"
#include "button.h"
#include "typing_engine.h"

/* HAL owns the Keyboard object (see hal_port.cpp). */
extern USBHIDKeyboard Keyboard;

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
static uint32_t s_mount_ms = 0;
static bool s_was_mounted = false;
static bool s_typed_once = false;

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
  /* Wait (bounded) for the host to accept the report. */
  t0 = millis();
  while (!hal_hid_ready()) {
    if ((uint32_t)(millis() - t0) > 2000u) {
      return; /* Host went away; drop the keystroke. */
    }
  }
  hal_hid_send(action->modifier, keys);
  hal_delay_ms(typing_engine_delay_ms(eng));
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

/* One super-loop iteration (called from loop()). */
static void tick(void) {
  bool mounted = hal_is_mounted();
  uint32_t now = millis();

  if (mounted && !s_was_mounted) {
    s_mount_ms = now; /* Countdown starts at mount, per spec. */
  }
  s_was_mounted = mounted;
  if (!mounted) {
    return;
  }

  if (g_cfg.write_mode == WRITE_MODE_TIME) {
    if (!s_typed_once && (uint32_t)(now - s_mount_ms) >= g_cfg.countdown_ms) {
      s_typed_once = true;
      type_all();
    }
  } else {
    /* WRITE_MODE_BUTTON (also the compiled-in default). */
    if (button_update(&g_button, hal_button_read(), now) ==
        BUTTON_EVENT_PRESS) {
      type_all();
    }
  }
}

void setup(void) {
  const char *cfg_text;

  hal_init();

  config_set_defaults(&g_cfg);
  cfg_text = config_text();
  if (cfg_text == NULL ||
      config_parse_string(cfg_text, &g_cfg) != CFG_OK ||
      config_validate(&g_cfg) != CFG_OK) {
    config_set_defaults(&g_cfg);
  }
  button_init(&g_button, g_cfg.debounce_ms, g_cfg.double_click_ms);

  /* HID first, strings second, USB.begin() last: descriptors are fixed
   * at enumeration time, so names must be set before the stack starts. */
  Keyboard.begin();
  USB.manufacturerName(HID_KEYBOARD_MANUFACTURER);
  USB.productName(HID_KEYBOARD_NAME);
  USB.begin();
}

void loop(void) {
  tick();
  hal_delay_ms(1);
}
