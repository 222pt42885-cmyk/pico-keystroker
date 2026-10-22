# ESP32-S3 (Zero / DevKitC-1) — USB-HID keystroker target

Real USB keyboard over the S3's native **USB-OTG peripheral**
(GPIO19 = D-, GPIO20 = D+) using the Arduino-ESP32 `USBHIDKeyboard`
stack (`KeyReport` + `Keyboard.sendReport()`). Reuses `src/common`
unchanged (config parser, HID keymap, typing engine, button machine);
only the 6-function HAL (`hal_port.cpp`) is board-specific.

## Wiring

- **Trigger:** press the board's **BOOT button (GPIO0, active-low)** —
  no wiring needed (internal pull-up, inversion handled in
  `hal_button_read()`).
- **Optional external button:** wire a momentary button between
  **GPIO4 and GND** (internal pull-up; OR-ed with BOOT). Change the pin
  with `-DEXT_BUTTON_PIN=...` (or `-1` to disable).
- **USB:** plug the board's **native-USB / OTG port** into the target
  host. On boards with two USB ports, the UART port will *not* work as a
  keyboard — use the OTG one.
- **Debug console:** `Serial` is **UART0 (TX GPIO43 / RX GPIO44)** at
  115200 baud, because CDC-On-Boot is off (see below). A USB serial
  monitor will show nothing; use a USB-UART adapter if you need logs.
- **Status LED:** none. The onboard WS2812 is *not* a plain GPIO LED
  (needs the NeoPixel protocol) and is deliberately unused.

## USB-OTG note

The firmware builds with `-DARDUINO_USB_MODE=0
-DARDUINO_USB_CDC_ON_BOOT=0` (forced in `platformio.ini`), which selects
the Arduino-ESP32 **USB-OTG (TinyUSB) device mode**. The S3 default
(`ARDUINO_USB_MODE=1`, Hardware CDC + JTAG) has no TinyUSB device and
cannot enumerate as HID — `hal_port.cpp` `#error`s out in that mode
instead of shipping a dead keyboard.

## First flash (BOOT + RESET)

The ROM USB-DFU bootloader only listens while BOOT is held at reset:

1. Hold **BOOT (GPIO0)**, tap **RESET** (or replug USB while holding
   BOOT on boards without a RESET button), release BOOT.
2. Build and flash:
   ```sh
   ln -s ../../src/common common   # once; on Windows copy the dir instead
   pio run -e esp32s3-zero
   pio run -e esp32s3-zero -t upload
   ```
3. Subsequent flashes normally work over USB without the button dance.

## Artifacts

`.pio/build/esp32s3-zero/firmware.bin` (plus `bootloader.bin`,
`partitions.bin`) flashed with esptool. **No `.uf2`** — UF2 is
Raspberry-Pi-bootrom-only (see `targets/README.md`).

## Config / language warning

`config/config.ini` + `config/input.txt` are baked in at compile time
(`tools/embed.py` → `payload.h`; copy it next to `main.cpp`). The
`LANGUAGE` setting (keyboard layout, default `en_US`) must match the
host's layout or typed characters will be wrong. ASCII only — unmapped
bytes are skipped. `HID_KEYBOARD_NAME` in the config is parsed but
cannot rename the USB device at runtime: the host-visible product string
is the `HID_KEYBOARD_NAME` compile-time macro; rebuild to change it.
