# ESP32-C3 "Super Mini" — documented STUB (no USB-HID possible)

## Short answer: this target cannot be a USB keyboard

The ESP32-C3 has **no USB-OTG peripheral**. Its only USB hardware is a
fixed-function **USB Serial/JTAG controller** (CDC-ACM serial + JTAG
debugging). It cannot be reconfigured as an HID (or any other USB class)
device. Sources:

- Espressif ESP-FAQ: *"ESP32-C3 … only supports USB-Serial-JTAG function"*
  and *"No, it only supports USB-Serial-JTAG function and can only be used
  as the USB device"*
  (<https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/usb.html>).
- ESP-IDF `usb-serial-jtag-console` docs: the controller is *"a
  fixed-function USB device … it cannot be reconfigured to perform any
  function other than a serial port and JTAG debugging"* (in contrast to
  the USB-OTG controllers on other ESP chips).
- ESP-IDF TinyUSB HID-device example: supported targets are
  **ESP32-S2/S3/H4/P4 only** — C3 is absent because there is no OTG block
  for TinyUSB to drive.

So a C3 board (e.g. ESP32-C3 Super Mini) **will never enumerate as a USB
keyboard**. This directory is a stub that says so honestly instead of
shipping fake HID code.

## Future options (out of scope for this stub)

- **BLE-HID:** the C3 has Bluetooth LE 5. A BLE HID-keyboard profile
  (e.g. ESP-IDF NimBLE / Bluedroid HID device) could act as a *wireless*
  keyboard. No pairing/battery handling is provided here.
- **Use an S2/S3 board instead:** ESP32-S2 and ESP32-S3 *do* have USB-OTG
  and work with TinyUSB HID device mode (`tud_hid_*`). For a working
  USB-HID target see **`targets/esp32s3`** (env `esp32s3-zero`,
  `firmware.bin` via `esptool`; full matrix in `targets/README.md`).

## What the skeleton does

`main.cpp` + `platformio.ini` build a stub that **reuses the shared
`src/common` typing/button logic** (`button.c`, `typing_engine.c`,
`config_parser.c`, `hid_keymap.c`) but routes the HAL's HID output to
the USB-Serial/JTAG console (`Serial`). Pressing BOOT (GPIO9) prints the
keystroke the firmware *would* have typed. Useful to validate the state
machine on real hardware; it does **not** type into the host.

Build (from this directory, after `ln -s ../../src/common common`):

```sh
pio run -e super-mini-c3      # firmware.bin / firmware.elf
pio run -e super-mini-c3 -t upload && pio device monitor
```
