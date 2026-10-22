#pragma once
/*
 * board_config.h — ESP32-S3 (Zero / DevKitC-1) hardware defaults.
 *
 * Every macro is guarded by #ifndef so a build can override it with
 * -DBOARD_FOO=... Runtime config BUTTON_PIN is parsed/validated but the
 * firmware uses the board default pin (hal_init() owns GPIO).
 *
 * TRIGGER: the BOOT button on GPIO0, active-low (internal pull-up, wire
 * nothing — just press BOOT). An optional external button to GND on
 * EXT_BUTTON_PIN is OR-ed in by hal_button_read().
 *
 * USB: native USB-OTG peripheral on GPIO19 (D-) / GPIO20 (D+); connect
 * the board's USB-OTG port (not a UART-only port, if the board has both).
 * Requires ARDUINO_USB_MODE=0 (USB-OTG/TinyUSB, see platformio.ini);
 * CDC-On-Boot is OFF so `Serial` stays on UART0 (TX GPIO43 / RX GPIO44)
 * for debug output instead of USB-CDC.
 *
 * NOTE: the onboard WS2812 RGB LED is NOT a plain GPIO LED — it needs
 * the NeoPixel protocol and is deliberately NOT used as a status LED
 * here (no extra dependency, no timing hacks).
 */

#ifndef TRIGGER_BUTTON_PIN
/* BOOT button, active-low with internal pull-up. */
#define TRIGGER_BUTTON_PIN 0
#endif

#ifndef EXT_BUTTON_PIN
/* Optional external trigger button to GND (internal pull-up). */
/* Set to -1 to disable. */
#define EXT_BUTTON_PIN 4
#endif

#ifndef HID_KEYBOARD_NAME
/* USB product string. NOTE: fixed at enumeration time (compile time); the
 * runtime config field HID_KEYBOARD_NAME is parsed/validated but cannot
 * rename an already-enumerated USB device. Rebuild with
 * -DHID_KEYBOARD_NAME="..." to change what the host sees. */
#define HID_KEYBOARD_NAME "Arboard-2040-pico-keystroker"
#endif

#ifndef HID_KEYBOARD_MANUFACTURER
#define HID_KEYBOARD_MANUFACTURER "Arboard"
#endif
