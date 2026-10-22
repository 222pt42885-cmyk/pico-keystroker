# cmake/arm-toolchain.cmake — OPTIONAL bare-metal ARM toolchain helper.
#
# You normally do NOT need this file: the Raspberry Pi Pico SDK ships and
# applies its own toolchain setup automatically (it passes its bundled
# pico_arm_gcc.cmake when PICO_TOOLCHAIN is default). Just install
# arm-none-eabi-gcc and configure with -DPICO_SDK_PATH=...
#
# This helper exists only for SDK-less checks (e.g. `cmake --build` try-compile
# probes, editor IntelliSense hosts, or sibling-agent lint environments):
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-toolchain.cmake \
#         -DKEYSTROKER_MCU=rp2040
#
# KEYSTROKER_MCU: "rp2040" (Cortex-M0+, default) or "rp2350" (Cortex-M33).

cmake_minimum_required(VERSION 3.13)

set(KEYSTROKER_MCU "rp2040" CACHE STRING "Target MCU: rp2040 | rp2350")
set_property(CACHE KEYSTROKER_MCU PROPERTY STRINGS rp2040 rp2350)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)

if(KEYSTROKER_MCU STREQUAL "rp2350")
  set(CMAKE_SYSTEM_PROCESSOR cortex-m33)
  set(_CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=soft")
else()
  set(CMAKE_SYSTEM_PROCESSOR cortex-m0plus)
  set(_CPU_FLAGS "-mcpu=cortex-m0plus -mthumb")
endif()

find_program(_ARM_CC arm-none-eabi-gcc NAMES arm-none-eabi-gcc REQUIRED
    DOC "ARM bare-metal C compiler")
find_program(_ARM_CXX arm-none-eabi-g++ NAMES arm-none-eabi-g++ REQUIRED
    DOC "ARM bare-metal C++ compiler")

set(CMAKE_C_COMPILER "${_ARM_CC}")
set(CMAKE_CXX_COMPILER "${_ARM_CXX}")

set(CMAKE_C_FLAGS_INIT "${_CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${_CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -nostartfiles")

# No test executables can run on the host under this toolchain.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

unset(_CPU_FLAGS)
unset(_ARM_CC CACHE)
unset(_ARM_CXX CACHE)
