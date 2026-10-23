# pico-keystroker — Raspberry Pi Pico-family firmware (RP2040 / RP2350).
#
# Standalone project build (default target: rp2040 on a Raspberry Pi Pico):
#   cmake -S . -B build -DPICO_SDK_PATH=/path/to/pico-sdk
#   cmake --build build
#
# Select a different board target (one configure == one board):
#   cmake -S . -B build-zero  -DKEYSTROKER_TARGET=rp2040-zero -DPICO_SDK_PATH=...
#   cmake -S . -B build-pico2 -DKEYSTROKER_TARGET=rp2350      -DPICO_SDK_PATH=...
#
# Meanings:
#   KEYSTROKER_TARGET=rp2040      -> -DPICO_BOARD=pico
#   KEYSTROKER_TARGET=rp2040-zero -> -DPICO_BOARD=rp2040_zero (custom header in
#                                    targets/rp2040-zero, shares rp2040 sources)
#   KEYSTROKER_TARGET=rp2350      -> -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350
# An explicit -DPICO_BOARD=... always wins over KEYSTROKER_TARGET.
#
# SDK sourcing: PICO_SDK_PATH env var or -DPICO_SDK_PATH=..., or FetchContent:
#   cmake -S . -B build -DPICO_SDK_FETCH_FROM_GIT=ON

cmake_minimum_required(VERSION 3.13...3.27)

# --- Board selection (must precede pico_sdk_init) -----------------------------
if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

set(KEYSTROKER_TARGET "rp2040" CACHE STRING "Firmware target: rp2040 | rp2040-zero | rp2350")
set_property(CACHE KEYSTROKER_TARGET PROPERTY STRINGS rp2040 rp2040-zero rp2350)

if(NOT PICO_BOARD)
  if(KEYSTROKER_TARGET STREQUAL "rp2350")
    set(PICO_BOARD "pico2")
  elseif(KEYSTROKER_TARGET STREQUAL "rp2040-zero")
    set(PICO_BOARD "rp2040_zero")
    set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/targets/rp2040-zero"
        CACHE PATH "Custom PICO_BOARD header dir (rp2040_zero.h)")
  else()
    set(PICO_BOARD "pico")
  endif()
endif()
if(KEYSTROKER_TARGET STREQUAL "rp2350" AND NOT PICO_PLATFORM)
  set(PICO_PLATFORM "rp2350")
endif()

# --- SDK guard: never hard-fail, so a plain `cmake -S . -B build` still parses
# --- in environments (CI lint, sibling agents) without the SDK checked out. ---
if(NOT PICO_SDK_FETCH_FROM_GIT AND NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
  message(STATUS "pico-keystroker: Raspberry Pi Pico SDK not found "
                 "(PICO_SDK_PATH='${PICO_SDK_PATH}').")
  message(STATUS "  Set PICO_SDK_PATH (env or -D), or configure with "
                 "-DPICO_SDK_FETCH_FROM_GIT=ON to fetch it.")
  message(STATUS "  Skipping firmware targets (configure-only mode).")
  return()
endif()

# Must be included before project().
include("${CMAKE_CURRENT_SOURCE_DIR}/pico_sdk_import.cmake")

project(pico-keystroker C CXX ASM)

# SDK 2.1.1 requires C11 (its public headers use bare static_assert, a parse
# error under -std=c99/gnu99). src/common stays C99-clean, so the host unit
# tests' stricter flags are unaffected.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

set(KEYSTROKER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SHARED_DIR "${KEYSTROKER_ROOT}/targets/rp2040")
set(TARGET_NAME "pico-keystroker")

# Per-target overrides (rp2040 defaults already live in board_config.h).
set(TARGET_DEFS "")
set(TARGET_INCDIRS "${SHARED_DIR}")
if(KEYSTROKER_TARGET STREQUAL "rp2350")
  list(APPEND TARGET_DEFS
    KEYSTROKER_USB_PID=0x4003
    HID_KEYBOARD_NAME="Arboard-2350-pico-keystroker")
elseif(KEYSTROKER_TARGET STREQUAL "rp2040-zero")
  list(APPEND TARGET_DEFS
    KEYSTROKER_USB_PID=0x4002
    HID_KEYBOARD_NAME="Arboard-Zero-pico-keystroker"
    KEYSTROKER_BUTTON_PIN=15
    KEYSTROKER_WS2812_PIN=16)
endif()

# --- Payload embed step (configure/build time) --------------------------------
# tools/embed.py (owned by a sibling agent) generates generated/payload.h with
#   extern const char embedded_input[]; extern const char embedded_config[];
# It must emit a self-contained payload.h. If the script or the input files are
# missing, a fallback payload.h with safe defaults is generated instead, so the
# firmware always builds. main.c also #includes payload.h only if present and
# otherwise falls back to compiled-in defaults (belt and suspenders).
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")
set(KEYSTROKER_INPUT "${KEYSTROKER_ROOT}/config/input.txt"
    CACHE FILEPATH "Text payload file to type")
set(KEYSTROKER_CONFIG "${KEYSTROKER_ROOT}/config/config.ini"
    CACHE FILEPATH "Keystroker INI config file to embed")
set(EMBED_SCRIPT "${KEYSTROKER_ROOT}/tools/embed.py")
# Optional extra embed outputs (e.g. a payload.c with symbol definitions).
set(KEYSTROKER_EMBED_EXTRA_SOURCES "" CACHE STRING "Extra embed-generated sources")
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND AND EXISTS "${EMBED_SCRIPT}" AND EXISTS "${KEYSTROKER_INPUT}")
  add_custom_command(
    OUTPUT "${GEN_DIR}/payload.h"
    COMMAND "${Python3_EXECUTABLE}" "${EMBED_SCRIPT}"
            --input "${KEYSTROKER_INPUT}"
            --config "${KEYSTROKER_CONFIG}"
            --output "${GEN_DIR}/payload.h"
    DEPENDS "${KEYSTROKER_INPUT}" "${KEYSTROKER_CONFIG}" "${EMBED_SCRIPT}"
    COMMENT "Embedding keystroke payload (tools/embed.py)")
else()
  file(GENERATE OUTPUT "${GEN_DIR}/payload.h" CONTENT [=[#pragma once
/* Fallback payload: tools/embed.py or the input files were not available at
 * configure time. Same symbols/layout as tools/embed.py output; safe defaults.
 * Override via -DKEYSTROKER_INPUT=... -DKEYSTROKER_CONFIG=... */
#include <stddef.h>
#include <stdint.h>
static const unsigned char keystroker_payload_text[] = "Pico Keystroker by Arda Kilicdagi\nhttps://github.com/Ardakilic/pico-keystroker\n";
static const size_t keystroker_payload_text_len = sizeof(keystroker_payload_text) - 1u;
static const unsigned char keystroker_config_text[] =
    "WRITE_MODE=button\n"
    "COUNTDOWN_MS=5000\n"
    "HID_KEYBOARD_NAME=Arboard-2040-pico-keystroker\n"
    "LANGUAGE=en_US\n"
    "TYPING_DELAY_MS=8\n"
    "DOUBLE_CLICK_MS=400\n"
    "DEBOUNCE_MS=30\n"
    "BUTTON_PIN=15\n";
static const size_t keystroker_config_text_len = sizeof(keystroker_config_text) - 1u;
]=])
endif()

# --- Firmware target -----------------------------------------------------------
# src/common/* sources are owned by a sibling agent; they are compiled in here
# but never modified by this project.
set(COMMON_DIR "${KEYSTROKER_ROOT}/src/common")
add_executable(${TARGET_NAME}
  "${SHARED_DIR}/main.c"
  "${SHARED_DIR}/usb_descriptors.c"
  "${SHARED_DIR}/hal_port.c"
  "${COMMON_DIR}/config_parser.c"
  "${COMMON_DIR}/hid_keymap.c"
  "${COMMON_DIR}/button.c"
  "${COMMON_DIR}/typing_engine.c"
  "${GEN_DIR}/payload.h"
  ${KEYSTROKER_EMBED_EXTRA_SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
  ${TARGET_INCDIRS}
  "${COMMON_DIR}"
  "${GEN_DIR}")

target_compile_definitions(${TARGET_NAME} PRIVATE ${TARGET_DEFS})
target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${TARGET_NAME} PRIVATE pico_stdlib tinyusb_device)

# USB stdio OFF (we are a HID device), UART stdio ON for debug prints.
pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

# Generate .uf2 (+ .bin/.hex/.map) next to the .elf.
pico_add_extra_outputs(${TARGET_NAME})
