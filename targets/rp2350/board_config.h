#pragma once
/*
 * board_config.h — RP2350 (Raspberry Pi Pico 2) hardware defaults.
 *
 * Same layout as the Pico (LED on GPIO25), so values mirror the rp2040 board
 * config except the USB PID / product string. Every macro is guarded by
 * #ifndef so -DKEYSTROKER_FOO=... overrides still work.
 */

#ifndef KEYSTROKER_BUTTON_PIN
/* GPIO with pull-up; pressed == low. Default matches CFG_DEFAULT_BUTTON_PIN. */
#define KEYSTROKER_BUTTON_PIN 15
#endif

#ifndef KEYSTROKER_LED_PIN
#ifdef PICO_DEFAULT_LED_PIN
#define KEYSTROKER_LED_PIN PICO_DEFAULT_LED_PIN
#else
#define KEYSTROKER_LED_PIN 25
#endif
#endif

#ifndef KEYSTROKER_WS2812_PIN
#define KEYSTROKER_WS2812_PIN (-1)
#endif

#ifndef KEYSTROKER_USB_VID
#define KEYSTROKER_USB_VID 0xCAFE
#endif
#ifndef KEYSTROKER_USB_PID
#define KEYSTROKER_USB_PID 0x4003
#endif

#ifndef HID_KEYBOARD_NAME
#define HID_KEYBOARD_NAME "Arboard-2350-pico-keystroker"
#endif

#ifndef KEYSTROKER_USB_SERIAL
#define KEYSTROKER_USB_SERIAL "PICOKEY0001"
#endif
