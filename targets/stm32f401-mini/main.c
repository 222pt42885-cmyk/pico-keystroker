/*
 * main.c — STM32 Mini F401 keystroker firmware (STM32F401CCU6).
 *
 * Baremetal super-loop modeled on targets/rp2040/main.c, reusing
 * src/common fully. Only HAL calls plus the shared core live here;
 * USB pumping happens in the OTG_FS interrupt (see hal_port.c), so
 * this file never touches TinyUSB/tusb.h or the Pico SDK directly.
 *
 * Flow: hal_init() -> load embedded config (defaults + parse + validate,
 * fall back to defaults on any failure) -> init button_t with
 * (debounce_ms, double_click_ms) -> loop:
 *   - WRITE_MODE_TIME:   type the embedded payload once, countdown_ms
 *     after the hal_is_mounted() rising edge (mount = USB enumerated).
 *   - WRITE_MODE_BUTTON: feed hal_button_read() through button_update();
 *     type the payload once per BUTTON_EVENT_PRESS (double-click
 *     suppression lives in src/common/button.c; DROPPED is ignored).
 * Typing: typing_engine_type_string() turns the payload into press/release
 * actions (hid_keymap per LANGUAGE, '\n' -> ENTER, unmapped chars skipped);
 * each action goes out via hal_hid_send() with typing_delay_ms spacing.
 *
 * Millisecond tick: HAL_GetTick() (SysTick, running after hal_init() ->
 * HAL_Init()). On non-ARM/doc builds (plain host gcc syntax check) a
 * loop-counter fallback keeps this file compiling; timing accuracy there
 * does not matter because target files never build on the host.
 *
 * Embedded payload: tools/embed.py generates payload.h with
 *   static const unsigned char keystroker_payload_text[] (+ _len);
 *   static const unsigned char keystroker_config_text[]  (+ _len);
 * Copy generated/payload.h next to this file (or add -I generated) so
 * __has_include finds it; otherwise the compiled-in fallback below is used.
 *
 * Build/flash (this directory, after `ln -s ../../src/common common`):
 *   pio run                              # .pio/build/.../firmware.{bin,hex,dfu}
 *   pio run -t upload                    # DFU via USB (BOOT0=1 + reset),
 *                                        # or upload_protocol = stlink
 * Artifacts are .bin/.hex/.dfu — NOT .uf2 (UF2 is Pi-bootrom-only).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#if defined(STM32F401xx) || defined(STM32F4xx) || defined(USE_HAL_DRIVER)
/* Real target: SysTick millisecond counter from STM32 HAL. */
#include "stm32f4xx_hal.h"
static uint32_t now_ms(void) {
    return HAL_GetTick();
}
#else
/* Host/doc fallback: advance 1 ms per poll so the logic still compiles
 * and reads correctly outside the ARM toolchain. */
static uint32_t now_ms(void) {
    static uint32_t t = 0;
    return t++;
}
#endif

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
    t0 = now_ms();
    while (!hal_hid_ready()) {
        if ((uint32_t)(now_ms() - t0) > 2000u) {
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

int main(void) {
    const char *cfg_text;
    uint32_t mount_ms = 0;
    uint32_t now;
    bool mounted;
    bool was_mounted = false;
    bool typed_once = false;

    hal_init();

    config_set_defaults(&g_cfg);
    cfg_text = config_text();
    if (cfg_text == NULL ||
        config_parse_string(cfg_text, &g_cfg) != CFG_OK ||
        config_validate(&g_cfg) != CFG_OK) {
        config_set_defaults(&g_cfg);
    }
    button_init(&g_button, g_cfg.debounce_ms, g_cfg.double_click_ms);

    for (;;) {
        /* USB device work runs in the OTG_FS interrupt (hal_port.c). */
        mounted = hal_is_mounted();
        now = now_ms();
        if (mounted && !was_mounted) {
            mount_ms = now; /* Countdown starts at mount. */
        }
        was_mounted = mounted;
        if (!mounted) {
            hal_delay_ms(1);
            continue;
        }

        if (g_cfg.write_mode == WRITE_MODE_TIME) {
            if (!typed_once && (uint32_t)(now - mount_ms) >= g_cfg.countdown_ms) {
                typed_once = true;
                type_all();
            }
        } else {
            /* WRITE_MODE_BUTTON (also the compiled-in default). */
            if (button_update(&g_button, hal_button_read(), now) ==
                BUTTON_EVENT_PRESS) {
                type_all();
            }
        }
        hal_delay_ms(1);
    }
}
