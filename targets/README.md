# Targets

Hardware ports of the keystroker. All non-Pi targets share
`src/common` (`config_parser.c`, `hid_keymap.c`, `typing_engine.c`,
`button.c` — host-testable, no MCU headers) and implement the 6-function
HAL from `src/common/hal.h`:

`hal_init`, `hal_is_mounted`, `hal_hid_ready`,
`hal_hid_send(mod, keycode[6])`, `hal_delay_ms`, `hal_button_read`.

## Matrix

| Dir | Board | MCU | USB-HID feasible | USB stack | Artifact | Flash method |
|---|---|---|---|---|---|---|
| `targets/rp2040` (other agent) | Raspberry Pi Pico | RP2040 | Yes | Pico SDK + TinyUSB | `.uf2` | BOOTSEL mass-storage |
| `targets/rp2040-zero` (other agent) | RP2040-Zero class | RP2040 | Yes | Pico SDK + TinyUSB | `.uf2` | BOOTSEL mass-storage |
| `targets/rp2350` (other agent) | Raspberry Pi Pico 2 | RP2350 | Yes | Pico SDK + TinyUSB | `.uf2` | BOOTSEL mass-storage |
| `targets/stm32f401-mini` | STM32 Mini F401 | STM32F401CCU6 | Yes | TinyUSB (synopsys FS)¹ | `.bin`/`.hex`/`.dfu` | DFU via USB (BOOT0) or ST-Link |
| `targets/blackpill` | WeAct Black Pill V1.2 | STM32F401CCU6 (F411CE²) | Yes | TinyUSB (synopsys FS)¹ | `.bin`/`.hex`/`.dfu` | DFU via USB (BOOT0) or ST-Link |
| `targets/esp32c3` | ESP32-C3 Super Mini | ESP32-C3 | **No** (stub) | N/A — USB-Serial/JTAG only | `firmware.bin` (console stub) | `esptool` via USB-CDC |
| `targets/esp32s3` | ESP32-S3 (Zero / DevKitC-1) | ESP32-S3 | Yes | Arduino-ESP32 USB-OTG (TinyUSB device) + USBHIDKeyboard | `firmware.bin` (+ `bootloader.bin`, `partitions.bin`) | `esptool` via USB (BOOT+RESET flash) |

¹ Baremetal alternative with the same `hal_*` surface: STM32Cube USB
Device Library (`USBD_HID`). See `hal_port.c` header comments.
² F411CE is footprint/pin compatible with F401CC on this board; same
pins apply (rebuild with the F411 target defines).

## Why only Pi gets UF2

UF2 is the ROM-bootloader format of RP2040/RP2350 (Microsoft UF2 flashed
through the Pi bootrom's USB mass-storage mode). STM32 and ESP32 chips do
not have a UF2 bootrom: STM32 flashes via its ROM DFU bootloader / SWD
(`.bin`/`.hex`/`.dfu`), ESP32-C3 via `esptool` over USB-Serial/JTAG
(`.bin`). ESP32-S3 also flashes `.bin` images via `esptool` (plus
`bootloader.bin`/`partitions.bin`). The release workflow therefore ships
`.uf2` for Pi targets, `.bin`/`.hex`/`.dfu` for STM32, and `firmware.bin`
(+ bootloader/partitions) for ESP32-S3.

## Pinout notes (STM32)

Both STM32 boards (refs: stm32-base Mini F401 and Black Pill V1.2 pages):

- User LED: **PC13**, sink type (LOW = on). No generic on-board user
  button (only NRST + BOOT0/switches; BOOT0 is boot-strapping).
- Trigger: **external button on PA0**, internal pull-up, active-low
  (wire PA0–GND). See `board_config.h` (`BUTTON_*`, `LED_*`).
- USB device: **PA11 (D-) / PA12 (D+)**; HSE 25 MHz → 84 MHz sysclk.
- SWD: PA13/PA14. Black Pill U3 footprint (PA4–PA7 SPI flash) unused.
- **+5V pins tie directly to USB +5V (no protection).** Never power from
  USB and an external supply simultaneously.

## Pinout notes (ESP32-S3)

ESP32-S3 Zero / DevKitC-1 (`targets/esp32s3`, env `esp32s3-zero`):

- Trigger: the board's **BOOT button on GPIO0**, active-low (internal
  pull-up, inversion handled in `hal_button_read()` — no wiring needed).
  Optional external button to GND on GPIO4 (`EXT_BUTTON_PIN`, `-1` disables).
- USB device: native **USB-OTG port on GPIO19 (D-) / GPIO20 (D+)**. On
  two-port boards use the OTG port, not the UART-only port.
- Debug console: `Serial` is **UART0 (TX GPIO43 / RX GPIO44)** at 115200.
- Status LED: none — the onboard WS2812 is not a plain GPIO LED and is
  deliberately unused (no LED driver).
- **Build requires `ARDUINO_USB_MODE=0`** (USB-OTG/TinyUSB device,
  `ARDUINO_USB_CDC_ON_BOOT=0`): the S3 Arduino default (`=1`, Hardware
  CDC + JTAG) has no TinyUSB device and cannot enumerate as HID.
  Forced in `platformio.ini`; `hal_port.cpp` `#error`s otherwise.
- First flash needs the ROM DFU bootloader: hold **BOOT (GPIO0)**, tap
  **RESET** (or replug USB holding BOOT), release BOOT, then
  `pio run -e esp32s3-zero -t upload`. Artifacts:
  `.pio/build/esp32s3-zero/firmware.bin` (+ `bootloader.bin`,
  `partitions.bin`) — **no `.uf2`** (see above).

## How to add a new board

1. Create `targets/<name>/` with:
   - `board_config.h` — `BOARD_NAME`, LED + button port/pin defines and
     active levels, USB/SWD pin notes.
   - `hal_port.c` — implement the 6 HAL functions against your USB stack
     (TinyUSB `tud_*` recommended; Cube `USBD_HID` equivalent is fine).
   - `main.c` — copy a sibling's super-loop (init core, poll button,
     call typing engine). USB pumping belongs in `hal_port.c`
     (interrupt), not in `main.c`.
   - Build file: `platformio.ini` (see STM32 examples: symlink
     `../../src/common` → `./common` and compile `common/*.c`), or a
     CMake/arm-none-eabi skeleton.
2. Reuse `src/common` as-is; board differences stay in `board_config.h`.
   If you need a core API name, check `src/common/*.h` — the call sites
   in `main.c` are the only coupling point.
3. Host tests (`make test`) are unaffected: target files are never built
   on the host. Add a row to the matrix above.
