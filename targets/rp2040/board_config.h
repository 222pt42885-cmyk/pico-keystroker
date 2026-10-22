#pragma once
/*
 * board_config.h — RP2040 (Raspberry Pi Pico) hardware defaults.
 *
 * Every macro is guarded by #ifndef so a build can override it with
 * -DKEYSTROKER_FOO=... (the rp2040-zero build does exactly this).
 * Runtime config (config.txt) BUTTON_PIN=-1 means "use this board default".
 */

#ifndef KEYSTROKER_BUTTON_PIN
/* GPIO with pull-up; pressed == low. Default matches CFG_DEFAULT_BUTTON_PIN. */
#define KEYSTROKER_BUTTON_PIN 15
#endif

#ifndef KEYSTROKER_LED_PIN
/* Onboard LED. The SDK defines PICO_DEFAULT_LED_PIN per PICO_BOARD. */
#ifdef PICO_DEFAULT_LED_PIN
#define KEYSTROKER_LED_PIN PICO_DEFAULT_LED_PIN
#else
#define KEYSTROKER_LED_PIN 25
#endif
#endif

#ifndef KEYSTROKER_WS2812_PIN
/* -1: no addressable LED on this board. */
#define KEYSTROKER_WS2812_PIN (-1)
#endif

#ifndef KEYSTROKER_USB_VID
/* 0xCAFE: private/local-use VID for this project (not a registered USB-IF ID;
 * fine for personal firmware, must not ship as a commercial product). */
#define KEYSTROKER_USB_VID 0xCAFE
#endif
#ifndef KEYSTROKER_USB_PID
#define KEYSTROKER_USB_PID 0x4001
#endif

#ifndef HID_KEYBOARD_NAME
/* USB product string. NOTE: fixed at enumeration time (compile time); the
 * runtime config field hid_name is parsed/validated but cannot rename an
 * already-enumerated USB device. Rebuild with -DHID_KEYBOARD_NAME="..." to
 * change what the host sees. */
#define HID_KEYBOARD_NAME "Arboard-2040-pico-keystroker"
#endif

#ifndef KEYSTROKER_USB_SERIAL
#define KEYSTROKER_USB_SERIAL "PICOKEY0001"
#endif
