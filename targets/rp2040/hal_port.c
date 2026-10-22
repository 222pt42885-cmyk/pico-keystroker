/*
 * hal_port.c — Raspberry Pi Pico implementation of src/common/hal.h.
 *
 * Mapping:
 *   hal_init()        GPIO button (input, pull-up) + tusb_init()
 *   hal_is_mounted()  tud_mounted()
 *   hal_hid_ready()   tud_mounted() && tud_hid_ready()
 *   hal_hid_send()    tud_hid_keyboard_report() (report ID 0: our HID report
 *                     descriptor has no report ID)
 *   hal_delay_ms()    sleep_ms()
 *   hal_button_read() pressed == GPIO low (pull-up); debounce lives in
 *                     src/common/button.c, consumed by main.c
 *
 * NOTE: this HAL has no millisecond clock API; time comes from the SDK
 * directly in main.c (to_ms_since_boot).
 */
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "hal.h"
#include "board_config.h"

void hal_init(void) {
  gpio_init((unsigned int)KEYSTROKER_BUTTON_PIN);
  gpio_set_dir((unsigned int)KEYSTROKER_BUTTON_PIN, GPIO_IN);
  gpio_pull_up((unsigned int)KEYSTROKER_BUTTON_PIN);
  tusb_init();
}

bool hal_is_mounted(void) {
  return tud_mounted();
}

bool hal_hid_ready(void) {
  return tud_mounted() && tud_hid_ready();
}

bool hal_hid_send(uint8_t modifier, const uint8_t keycode[6]) {
  uint8_t kc[6];

  if (!hal_hid_ready()) {
    return false;
  }
  memcpy(kc, keycode, sizeof(kc));
  return tud_hid_keyboard_report(0, modifier, kc);
}

void hal_delay_ms(uint32_t ms) {
  sleep_ms(ms);
}

bool hal_button_read(void) {
  /* Pull-up: idle high, pressed low. */
  return !gpio_get((unsigned int)KEYSTROKER_BUTTON_PIN);
}
