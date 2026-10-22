# pico_sdk_import.cmake — SDK bootstrap shim for pico-keystroker.
#
# Adapted from the upstream Raspberry Pi Pico SDK import shim
# (<sdk>/pico_sdk_import.cmake). Include this file BEFORE project().
#
# SDK sourcing, in order:
#   1. -DPICO_SDK_PATH=... / $PICO_SDK_PATH
#   2. -DPICO_SDK_FETCH_FROM_GIT=ON (FetchContent fallback, needs network/git)
#      with optional -DPICO_SDK_FETCH_FROM_GIT_PATH=... (shared checkout dir)
#      and -DPICO_SDK_FETCH_FROM_GIT_TAG=... (default: SDK v2 release).
#
# Every firmware CMakeLists in this repo guards SDK presence BEFORE including
# this file, so a missing SDK prints a friendly configure-only message instead
# of hitting the FATAL_ERRORs below.

if(DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
  message("Using PICO_SDK_PATH from environment ('${PICO_SDK_PATH}')")
endif()

if(DEFINED ENV{PICO_SDK_FETCH_FROM_GIT} AND (NOT PICO_SDK_FETCH_FROM_GIT))
  set(PICO_SDK_FETCH_FROM_GIT $ENV{PICO_SDK_FETCH_FROM_GIT})
  message("Using PICO_SDK_FETCH_FROM_GIT from environment ('${PICO_SDK_FETCH_FROM_GIT}')")
endif()

if(DEFINED ENV{PICO_SDK_FETCH_FROM_GIT_PATH} AND (NOT PICO_SDK_FETCH_FROM_GIT_PATH))
  set(PICO_SDK_FETCH_FROM_GIT_PATH $ENV{PICO_SDK_FETCH_FROM_GIT_PATH})
  message("Using PICO_SDK_FETCH_FROM_GIT_PATH from environment ('${PICO_SDK_FETCH_FROM_GIT_PATH}')")
endif()

if(DEFINED ENV{PICO_SDK_FETCH_FROM_GIT_TAG} AND (NOT PICO_SDK_FETCH_FROM_GIT_TAG))
  set(PICO_SDK_FETCH_FROM_GIT_TAG $ENV{PICO_SDK_FETCH_FROM_GIT_TAG})
  message("Using PICO_SDK_FETCH_FROM_GIT_TAG from environment ('${PICO_SDK_FETCH_FROM_GIT_TAG}')")
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
set(PICO_SDK_FETCH_FROM_GIT "${PICO_SDK_FETCH_FROM_GIT}" CACHE BOOL
    "Fetch the Raspberry Pi Pico SDK from git (FetchContent fallback)")
set(PICO_SDK_FETCH_FROM_GIT_PATH "${PICO_SDK_FETCH_FROM_GIT_PATH}" CACHE FILEPATH
    "Optional directory for the FetchContent SDK checkout")
set(PICO_SDK_FETCH_FROM_GIT_TAG "2.1.1" CACHE STRING "SDK git tag for FetchContent")

if(NOT PICO_SDK_FETCH_FROM_GIT)
  get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
  if(NOT EXISTS "${PICO_SDK_PATH}")
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' not found. "
            "Set PICO_SDK_PATH or use -DPICO_SDK_FETCH_FROM_GIT=ON")
  endif()
  if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' does not appear to contain "
            "the Raspberry Pi Pico SDK (missing pico_sdk_init.cmake)")
  endif()
  set(PICO_SDK_INIT_CMAKE "${PICO_SDK_PATH}/pico_sdk_init.cmake")
else()
  include(FetchContent)
  set(_pico_sdk_fetch_args
      GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
      GIT_TAG "${PICO_SDK_FETCH_FROM_GIT_TAG}"
      GIT_SUBMODULES_RECURSE FALSE)
  if(PICO_SDK_FETCH_FROM_GIT_PATH)
    list(APPEND _pico_sdk_fetch_args SOURCE_DIR "${PICO_SDK_FETCH_FROM_GIT_PATH}")
  endif()
  FetchContent_Declare(pico_sdk ${_pico_sdk_fetch_args})
  FetchContent_GetProperties(pico_sdk)
  if(NOT pico_sdk_POPULATED)
    FetchContent_Populate(pico_sdk)
  endif()
  set(PICO_SDK_INIT_CMAKE "${pico_sdk_SOURCE_DIR}/pico_sdk_init.cmake")
endif()

include("${PICO_SDK_INIT_CMAKE}")
