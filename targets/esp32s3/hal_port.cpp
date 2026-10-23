/* hal_port.cpp — ESP32-S3 HAL implementation (Arduino-ESP32 USB-OTG).
 *
 * Implements the 6-function C HAL from src/common/hal.h over the
 * Arduino-ESP32 USB stack:
 *   - USBHIDKeyboard `Keyboard` (libraries/USB/src/USBHIDKeyboard.h):
 *     KeyReport { modifiers, reserved, keys[6] } + sendReport(&r),
 *     which packs a hid_keyboard_report_t and pushes it with the
 *     keyboard report ID.
 *   - USB device strings via USB.manufacturerName()/productName()
 *     (called in main.cpp setup(), before USB.begin()).
 *   - Mount/ready via the TinyUSB device API: tud_mounted() and
 *     tud_hid_n_ready(0) — the same calls Arduino's own USBHID::ready()
 *     is built on (libraries/USB/src/USBHID.cpp). ESPUSB exposes no
 *     mounted() and USBHIDKeyboard exposes no ready() in any
 *     Arduino-ESP32 core generation (2.x or 3.x), so TinyUSB is called
 *     directly; its device API is stable across both.
 *
 * Requires ARDUINO_USB_MODE=0 (USB-OTG/TinyUSB device mode, forced in
 * platformio.ini). In HW-CDC mode (==1) TinyUSB device is absent and
 * HID cannot enumerate, so fail the build loudly instead of shipping a
 * dead keyboard.
 */

#include <string.h>

#include <Arduino.h>

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "esp32-hal-tinyusb.h" /* tud_mounted() / tud_hid_n_ready() */

#include "board_config.h"
#include "hal.h"

#if defined(ARDUINO_USB_MODE) && (ARDUINO_USB_MODE != 0)
#error "pico-keystroker ESP32-S3 needs USB-OTG (TinyUSB) device mode: build with -DARDUINO_USB_MODE=0 -DARDUINO_USB_CDC_ON_BOOT=0"
#endif

/* Owned here; main.cpp references it via `extern USBHIDKeyboard Keyboard;`
 * to call Keyboard.begin() during setup(). */
USBHIDKeyboard Keyboard;

void hal_init(void) {
  pinMode(TRIGGER_BUTTON_PIN, INPUT_PULLUP);
#if defined(EXT_BUTTON_PIN) && (EXT_BUTTON_PIN >= 0)
  if (EXT_BUTTON_PIN != TRIGGER_BUTTON_PIN) {
    pinMode(EXT_BUTTON_PIN, INPUT_PULLUP);
  }
#endif
  /* CDC-On-Boot is OFF (see platformio.ini), so Serial is UART0
   * (TX GPIO43 / RX GPIO44) — debug console only, not USB. */
  Serial.begin(115200);
}

/* TinyUSB device state, called directly: ESPUSB has no mounted() and
 * USBHIDKeyboard (which owns its USBHID privately) has no ready() in the
 * Arduino-ESP32 core, so these mirror USBHID::ready()'s implementation
 * (`return tud_hid_n_ready(0);`). False when unmounted/not-ready. */
bool hal_is_mounted(void) {
  return tud_mounted();
}

bool hal_hid_ready(void) {
  return tud_mounted() && tud_hid_n_ready(0);
}

bool hal_hid_send(uint8_t modifier, const uint8_t keycode[6]) {
  KeyReport report;

  if (keycode == NULL) {
    return false;
  }
  if (!hal_hid_ready()) {
    return false;
  }
  report.modifiers = modifier;
  report.reserved = 0;
  memcpy(report.keys, keycode, sizeof(report.keys));
  /* void in this core revision: readiness was checked above. */
  Keyboard.sendReport(&report);
  return true;
}

void hal_delay_ms(uint32_t ms) {
  delay(ms);
}

bool hal_button_read(void) {
  /* Active-low buttons with internal pull-ups: pressed == LOW. */
  if (digitalRead(TRIGGER_BUTTON_PIN) == LOW) {
    return true;
  }
#if defined(EXT_BUTTON_PIN) && (EXT_BUTTON_PIN >= 0)
  if (EXT_BUTTON_PIN != TRIGGER_BUTTON_PIN &&
      digitalRead(EXT_BUTTON_PIN) == LOW) {
    return true;
  }
#endif
  return false;
}
