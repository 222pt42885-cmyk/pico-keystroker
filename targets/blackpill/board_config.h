#pragma once
/* Board: WeAct Black Pill V1.2 (STM32F401CCU6 or F411CE, 256 KiB flash /
 * 64 KiB RAM, HSE 25 MHz). This port targets the F401CC variant.
 * Ref: https://stm32-base.org/boards/STM32F401CCU6-WeAct-Black-Pill-V1.2.html
 * (F411CE is footprint-compatible; same pins below apply.)
 *
 * On-board UI: user LED on PC13 (sink: LOW = on). No generic user button
 * (only NRST + BOOT0 — BOOT0 is boot-strapping, do NOT use as trigger),
 * so the trigger is an EXTERNAL button on PA0 (internal pull-up,
 * active-low: wire button between PA0-GND).
 * USB device (USB-C): PA11 (D-) / PA12 (D+). SWD: PA13 / PA14.
 * U3 footprint: SPI flash/EEPROM on PA4-PA7 (unused by this firmware).
 * WARNING: +5V pins tie directly to USB +5V (no protection). Never power
 * the board from USB and an external supply at the same time.
 */
#include "stm32f4xx_hal.h"

#define BOARD_NAME        "blackpill-f401cc"

#define LED_GPIO_PORT     GPIOC
#define LED_GPIO_PIN      GPIO_PIN_13
#define LED_ACTIVE_LOW    1

#define BUTTON_GPIO_PORT  GPIOA
#define BUTTON_GPIO_PIN   GPIO_PIN_0
#define BUTTON_ACTIVE_LOW 1 /* external button to GND, internal pull-up */
