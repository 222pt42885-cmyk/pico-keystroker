#pragma once
/* Board: STM32 Mini F401 (STM32F401CCU6, 256 KiB flash / 64 KiB RAM, HSE 25 MHz).
 * Ref: https://stm32-base.org/boards/STM32F401CCU6-STM32-Mini-F401.html
 *
 * On-board UI: user LED on PC13 (sink: LOW = on). No on-board user button
 * (only NRST + BOOT0/BOOT1 DIP switches), so the trigger is an EXTERNAL
 * button on PA0 (internal pull-up, active-low: wire button between PA0-GND).
 * USB device: PA11 (D-) / PA12 (D+). SWD: PA13 / PA14.
 * WARNING: +5V pins tie directly to USB +5V (no protection). Never power
 * the board from USB and an external supply at the same time.
 */
#include "stm32f4xx_hal.h"

#define BOARD_NAME        "stm32f401-mini"

#define LED_GPIO_PORT     GPIOC
#define LED_GPIO_PIN      GPIO_PIN_13
#define LED_ACTIVE_LOW    1

#define BUTTON_GPIO_PORT  GPIOA
#define BUTTON_GPIO_PIN   GPIO_PIN_0
#define BUTTON_ACTIVE_LOW 1 /* external button to GND, internal pull-up */
