# Standalone firmware build: RP2040-Zero style board (RP2040, custom header).
#
#   cmake -S . -B build -DPICO_SDK_PATH=/path/to/pico-sdk   (or FetchContent)
#   cmake --build build
#
# Output: build/pico-keystroker-zero.{elf,bin,hex,uf2}.
# Also selectable from the repo root via -DKEYSTROKER_TARGET=rp2040-zero.
#
# NOTE — sharing, not duplication: this board has the same RP2040 silicon, so
# it compiles the SAME sources as targets/rp2040 (main.c, usb_descriptors.c,
# hal_port.c) with different per-board defines:
#   PICO_BOARD=rp2040_zero (+ PICO_BOARD_HEADER_DIRS -> rp2040_zero.h here,
#     documents the pinout diff: WS2812 on GPIO16, no discrete LED,
#     button default GPIO15), USB PID 0x4002, Zero product string,
#   KEYSTROKER_BUTTON_PIN=15, KEYSTROKER_LED_PIN=-1 (no LED; LED code no-ops),
#   KEYSTROKER_WS2812_PIN=16 (reserved for future WS2812 status output).
# No separate board_config.h/tusb_config.h/hal_port.c exist here on purpose.

cmake_minimum_required(VERSION 3.13...3.27)

if(DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT PICO_BOARD)
  set(PICO_BOARD "rp2040_zero")
endif()
if(NOT PICO_BOARD_HEADER_DIRS)
  set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}"
      CACHE PATH "Custom PICO_BOARD header dir (rp2040_zero.h)")
endif()

# SDK guard: parse cleanly without the SDK (CI lint / sibling agents).
if(NOT PICO_SDK_FETCH_FROM_GIT AND NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
  message(STATUS "pico-keystroker-zero: Pico SDK not found "
                 "(PICO_SDK_PATH='${PICO_SDK_PATH}'); configure-only mode.")
  return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../pico_sdk_import.cmake")

project(pico-keystroker-zero C CXX ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

set(KEYSTROKER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SHARED_DIR "${KEYSTROKER_ROOT}/targets/rp2040")
set(TARGET_NAME "pico-keystroker-zero")

# --- Payload embed step (see repo-root CMakeLists for the full rationale) -----
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")
set(KEYSTROKER_INPUT "${KEYSTROKER_ROOT}/config/input.txt"
    CACHE FILEPATH "Text payload file to type")
set(KEYSTROKER_CONFIG "${KEYSTROKER_ROOT}/config/config.ini"
    CACHE FILEPATH "Keystroker INI config file to embed")
set(EMBED_SCRIPT "${KEYSTROKER_ROOT}/tools/embed.py")
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
    "HID_KEYBOARD_NAME=Arboard-Zero-pico-keystroker\n"
    "LANGUAGE=en_US\n"
    "TYPING_DELAY_MS=8\n"
    "DOUBLE_CLICK_MS=400\n"
    "DEBOUNCE_MS=30\n"
    "BUTTON_PIN=15\n";
static const size_t keystroker_config_text_len = sizeof(keystroker_config_text) - 1u;
]=])
endif()

# --- Firmware target (shared rp2040 sources + Zero defines) --------------------
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
  "${SHARED_DIR}"
  "${COMMON_DIR}"
  "${GEN_DIR}")

target_compile_definitions(${TARGET_NAME} PRIVATE
  KEYSTROKER_USB_PID=0x4002
  HID_KEYBOARD_NAME="Arboard-Zero-pico-keystroker"
  KEYSTROKER_BUTTON_PIN=15
  KEYSTROKER_LED_PIN=-1
  KEYSTROKER_WS2812_PIN=16)

target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${TARGET_NAME} PRIVATE pico_stdlib tinyusb_device)

pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

pico_add_extra_outputs(${TARGET_NAME})
