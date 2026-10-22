#pragma once
/*
 * tusb_config.h — minimal TinyUSB device configuration.
 * Single HID keyboard interface; every other device class disabled.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* MCU selection: the Pico SDK normally supplies this itself; keep a defensive
 * fallback so the file also parses in SDK-less lint environments. */
#ifndef CFG_TUSB_MCU
#if defined(PICO_RP2350)
#define CFG_TUSB_MCU OPT_MCU_RP2350
#else
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

#define CFG_TUD_ENABLED 1

/* One keyboard. Everything else off. */
#define CFG_TUD_HID    1
#define CFG_TUD_CDC    0
#define CFG_TUD_MSC    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_HID_EP_BUFSIZE 16
#define CFG_TUD_ENDPOINT0_SIZE 64

#ifdef __cplusplus
}
#endif
