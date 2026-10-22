#pragma once
/*
 * usb_descriptors.h — TinyUSB descriptors live in usb_descriptors.c.
 * Re-exported here for debuggers and host-side tests.
 */
#include <stdint.h>

extern uint8_t const desc_hid_report[];
extern uint8_t const desc_configuration[];
