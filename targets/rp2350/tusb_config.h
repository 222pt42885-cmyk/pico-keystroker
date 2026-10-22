/*
 * tusb_config.h (rp2350) — shares the RP2040 TinyUSB config.
 * The config is chip-agnostic (single HID keyboard); the CFG_TUSB_MCU
 * fallback inside selects OPT_MCU_RP2350 when PICO_RP2350 is defined.
 */
#include "../rp2040/tusb_config.h"
