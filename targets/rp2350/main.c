/*
 * main.c (rp2350) — shares the RP2040 implementation verbatim.
 *
 * Per-board differences (PICO_BOARD=pico2, PICO_PLATFORM=rp2350, USB PID,
 * product string, pins) come from this dir's board_config.h plus the CMake
 * compile definitions; the firmware logic is chip-independent (pico-sdk GPIO
 * + TinyUSB + src/common), so no fork of main.c is needed.
 */
#include "../rp2040/main.c"
