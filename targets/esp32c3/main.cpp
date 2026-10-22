/* main.cpp — ESP32-C3 Super Mini STUB.
 *
 * *** THIS TARGET CANNOT BE A USB KEYBOARD. ***
 * The ESP32-C3 has NO USB-OTG peripheral. Its only USB hardware is a
 * fixed-function USB-Serial/JTAG controller (CDC serial + JTAG debug);
 * it cannot enumerate as HID (or any other USB class). For real USB-HID
 * use an ESP32-S2/S3 board (they have USB-OTG and work with TinyUSB HID
 * device mode); for wireless use a BLE-HID fork. See README.md here.
 *
 * What this stub does: Arduino-style setup()/loop() that uses the REAL
 * src/common core APIs (config_parser, button state machine, typing
 * engine — the same code the HID targets use) but routes the HAL's HID
 * output to the USB-Serial console (Serial) instead of a keyboard
 * report. Press BOOT (GPIO9) to log what WOULD have been typed. Useful
 * to validate the state machine on real hardware; it does NOT type
 * into the host.
 *
 * The hal_* functions below are the whole HAL implementation for this
 * target (there is no hal_port.c here): console-logging versions only.
 */
#include <Arduino.h>

#include "hal.h"
#include "config_parser.h"
#include "hid_keymap.h"
#include "button.h"
#include "typing_engine.h"

#define BOOT_BUTTON_PIN 9 /* Super Mini BOOT button, active-low */

/* --- HAL stub: console-logging only. Claims HID-ready but types nothing
 * into the host; hal_hid_send() prints the report to Serial. --- */

void hal_init(void) { /* Arduino core already init'd clocks/USB-CDC. */ }

bool hal_is_mounted(void) { return true; } /* CDC console, not HID. */
bool hal_hid_ready(void) { return true; }

bool hal_hid_send(uint8_t modifier, const uint8_t keycode[6]) {
    Serial.printf("[stub-hid] mod=0x%02x keys=%02x %02x %02x %02x %02x %02x\n",
                  modifier, keycode[0], keycode[1], keycode[2],
                  keycode[3], keycode[4], keycode[5]);
    return true;
}

void hal_delay_ms(uint32_t ms) { delay(ms); }

bool hal_button_read(void) {
    return digitalRead(BOOT_BUTTON_PIN) == LOW;
}

/* --- Shared-core state (real APIs, same as the HID targets). --- */

static keystroker_config_t g_cfg;
static button_t g_button;

/* Fallback config: same keys as tools/embed.py output / config/config.ini.
 * (No payload.h wiring on this stub; payload is a fixed demo string.) */
static const char kFallbackConfig[] =
    "WRITE_MODE=button\n"
    "COUNTDOWN_MS=5000\n"
    "HID_KEYBOARD_NAME=Arboard-2040-pico-keystroker\n"
    "LANGUAGE=en_US\n"
    "TYPING_DELAY_MS=8\n"
    "DOUBLE_CLICK_MS=400\n"
    "DEBOUNCE_MS=30\n"
    "BUTTON_PIN=9\n";

static const char kDemoPayload[] = "Pico Keystroker by Arda Kilicdagi\n"
                                       "https://github.com/Ardakilic/pico-keystroker\n";

/* typing_engine callback: log one press/release action to the console. */
static void log_action(const hid_action_t *action, void *user) {
    const typing_engine_t *eng = (const typing_engine_t *)user;
    uint8_t keys[6] = { 0, 0, 0, 0, 0, 0 };

    if (action == NULL) {
        return;
    }
    if (!action->is_release) {
        keys[0] = action->keycode;
    }
    hal_hid_send(action->modifier, keys);
    hal_delay_ms(typing_engine_delay_ms(eng));
}

static void type_all(void) {
    typing_engine_config_t tec;
    typing_engine_t eng;
    uint32_t actions;

    tec.language = g_cfg.language;
    tec.typing_delay_ms = g_cfg.typing_delay_ms;
    typing_engine_init(&eng, &tec);
    actions = typing_engine_type_string(&eng, kDemoPayload, log_action, &eng);
    Serial.printf("[stub] typed=%lu skipped=%lu actions=%lu (console only)\n",
                  (unsigned long)eng.chars_typed,
                  (unsigned long)eng.chars_skipped,
                  (unsigned long)actions);
}

void setup() {
    Serial.begin(115200);
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    config_set_defaults(&g_cfg);
    if (config_parse_string(kFallbackConfig, &g_cfg) != CFG_OK ||
        config_validate(&g_cfg) != CFG_OK) {
        config_set_defaults(&g_cfg);
    }
    button_init(&g_button, g_cfg.debounce_ms, g_cfg.double_click_ms);

    Serial.println("[stub] ESP32-C3 HID stub ready (console only, no USB-HID).");
    Serial.println("[stub] Press BOOT (GPIO9) to log what would be typed.");
}

void loop() {
    /* WRITE_MODE_BUTTON only: time mode makes no sense without HID. */
    if (button_update(&g_button, hal_button_read(), millis()) ==
        BUTTON_EVENT_PRESS) {
        type_all();
    }
    hal_delay_ms(1);
}
