/*
 * rp2040_zero.h — custom PICO_BOARD header for RP2040-Zero style boards
 * (Waveshare RP2040-Zero and compatibles: RP2040 + USB-C in a mini footprint).
 *
 * Use with:
 *   cmake -DPICO_BOARD=rp2040_zero \
 *         -DPICO_BOARD_HEADER_DIRS=<repo>/targets/rp2040-zero ...
 *
 * Pinout diff vs. the Raspberry Pi Pico (this is the whole point of the file):
 *   - NO regular LED and NO debug LED; board status is a WS2812 RGB LED on
 *     GPIO16. There is nothing for KEYSTROKER_LED_PIN to drive, so firmware
 *     builds for this board pass -DKEYSTROKER_LED_PIN=-1 (LED code no-ops).
 *   - WS2812 data pin: GPIO16 (define PICO_DEFAULT_WS2812_PIN below for
 *     ws2812 drivers; this firmware does not bit-bang it yet).
 *   - User button default: GPIO15 (exposed on the edge header; GPIO14 kept as
 *     the alternate). Firmware passes -DKEYSTROKER_BUTTON_PIN=15.
 *   - Same RP2040 silicon, same 2 MB flash, same USB device port: everything
 *     else (clocks, UARTs, USB) inherits from boards/pico.h.
 */
#ifndef _BOARDS_RP2040_ZERO_H
#define _BOARDS_RP2040_ZERO_H

/* Inherit all RP2040/Pico defaults (flash size, XOSC, UART pins, ...). */
#include "boards/pico.h"

/* This board has no discrete LED; make that explicit so code referencing the
 * Pico default does not light the wrong pin. */
#ifdef PICO_DEFAULT_LED_PIN
#undef PICO_DEFAULT_LED_PIN
#endif

/* WS2812 addressable RGB LED data pin (not driven by this firmware yet). */
#ifndef PICO_DEFAULT_WS2812_PIN
#define PICO_DEFAULT_WS2812_PIN 16
#endif

/* Suggested user-button GPIO for this board (wired to GND when pressed,
 * firmware enables the internal pull-up). */
#ifndef RP2040_ZERO_BUTTON_PIN
#define RP2040_ZERO_BUTTON_PIN 15
#endif

/* USB product differentiation is done in firmware (board_config.h defaults +
 * -D overrides), not here, so VID/PID stay out of the board header. */

#endif
