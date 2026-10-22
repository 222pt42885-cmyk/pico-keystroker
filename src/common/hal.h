#ifndef KEYSTROKER_HAL_H
#define KEYSTROKER_HAL_H

/*
 * Hardware Abstraction Layer interface.
 *
 * The host-testable common core (config parser, HID keymap, typing engine,
 * button state machine) depends ONLY on this interface -- never on TinyUSB,
 * the Pico SDK, or any board headers. Firmware targets (see targets/)
 * provide the real implementation; host tests provide fakes/stubs.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One-time platform init (clocks, USB stack, GPIO direction, ...). */
void hal_init(void);

/* True once the USB device is mounted by the host. */
bool hal_is_mounted(void);

/* True when the HID interface is ready to accept a report. */
bool hal_hid_ready(void);

/*
 * Send one HID boot-protocol keyboard report.
 *   modifier : bitmap of modifier keys (see HID_MOD_* in hid_keymap.h)
 *   keycode  : up to 6 simultaneously pressed usage IDs (unused slots = 0)
 * Returns true if the report was accepted, false if it must be retried.
 */
bool hal_hid_send(uint8_t modifier, const uint8_t keycode[6]);

/* Blocking millisecond delay. */
void hal_delay_ms(uint32_t ms);

/* Sample the trigger button: true = pressed (active high after any
 * board-level inversion is applied by the HAL implementation). */
bool hal_button_read(void);

#ifdef __cplusplus
}
#endif

#endif /* KEYSTROKER_HAL_H */
