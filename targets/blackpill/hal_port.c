/* hal_port.c — STM32F401CCU6 HAL port (Mini F401 and Black Pill;
 * per-board differences live in board_config.h).
 *
 * USB stack: TinyUSB device, synopsys STM32 FS driver (OTG_FS in FS mode
 * on PA11/PA12). The OTG_FS interrupt is owned by the Adafruit TinyUSB
 * core (do NOT define OTG_FS_IRQHandler here — a duplicate definition
 * fails the link); deferred stack work is pumped via tud_task() from
 * main.c's loop. You must add TinyUSB's HID descriptors (copy the TinyUSB
 * hid_app `usb_descriptors.c` example and adjust VID/PID/strings).
 * Alternative (same hal_* surface): STM32Cube USB Device Library
 * (USBD_HID): swap tud_* calls for USBD_HID_SendReport + checks on
 * hUsbDeviceFS state. See targets/README.md.
 *
 * Implements the HAL contract from src/common/hal.h:
 *   hal_init, hal_is_mounted, hal_hid_ready, hal_hid_send,
 *   hal_delay_ms, hal_button_read.
 */
#include <stdint.h>
#include <stdbool.h>

#include "hal.h"
#include "board_config.h"
#include "tusb.h"

void hal_init(void)
{
    HAL_Init();
    /* Expect 84 MHz sysclk from the 25 MHz HSE (set up by the Arduino core
     * or Cube SystemClock_Config; leave the default if already correct). */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = { 0 };

    /* LED (sink type: default off = HIGH). */
    g.Pin = LED_GPIO_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &g);
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);

    /* Trigger button: input with pull-up, active-low. */
    g.Pin = BUTTON_GPIO_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &g);

    tud_init(0); /* TinyUSB device stack (descriptors in usb_descriptors.c). */
}

/* NOTE: no OTG_FS_IRQHandler here. With framework=arduino + the Adafruit
 * TinyUSB library, the core's own Adafruit_TinyUSB_stm32.cpp already
 * defines OTG_FS_IRQHandler (it forwards to the TinyUSB ISR pump), so
 * defining it again here fails the link with "multiple definition of
 * OTG_FS_IRQHandler". Rely on the core's handler; TinyUSB deferred work
 * is serviced by the tud_task() poll in main.c's loop. */

bool hal_is_mounted(void)
{
    return tud_mounted();
}

bool hal_hid_ready(void)
{
    return tud_hid_ready();
}

bool hal_hid_send(uint8_t modifier, const uint8_t keycode[6])
{
    if (!tud_mounted() || !tud_hid_ready()) {
        return false;
    }
    return tud_hid_keyboard_report(0, modifier, keycode);
}

void hal_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

bool hal_button_read(void)
{
    GPIO_PinState s = HAL_GPIO_ReadPin(BUTTON_GPIO_PORT, BUTTON_GPIO_PIN);
#if BUTTON_ACTIVE_LOW
    return s == GPIO_PIN_RESET;
#else
    return s == GPIO_PIN_SET;
#endif
}
