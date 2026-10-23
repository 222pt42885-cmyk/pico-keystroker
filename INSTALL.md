# INSTALL — pico-keystroker per-target install guide

Barebones USB HID keystroker: plug it in, it enumerates as a USB keyboard
named `HID_KEYBOARD_NAME` and types `config/input.txt` once per trigger
(button press, or automatically after `COUNTDOWN_MS` in `time` mode).
No QMK. See [README.md](README.md) for what it is and how it works;
this file is the step-by-step build + flash + verify for each board.

General flow for every target:

1. Edit `config/config.ini` (settings) and `config/input.txt` (payload).
   Keys are case-insensitive; `#`/`;` start comment lines in the INI
   (unknown keys and `[section]` lines are ignored). **The payload has no
   comment syntax** — every byte of `input.txt`, including its shipped
   `#` header lines, is typed verbatim. Keep it plain ASCII; non-ASCII
   bytes have no HID mapping and are skipped. `LANGUAGE` must match the
   host OS keyboard layout or shifted/symbol keys come out wrong.
2. Build the firmware for your board (below). The config + payload are
   baked in at compile time (`tools/embed.py` → `payload.h`; Pi CMake
   runs it automatically, PlatformIO targets need the `common` symlink
   first). Rebuild + reflash after every `config/` change.
3. Flash, then verify (see [Verify](#verify)).

## Prerequisites

- **Pi (RP2040 / RP2350):** CMake + Ninja/Make, ARM toolchain, Pico SDK,
  picotool. Same set CI uses (`.github/workflows/release.yml`,
  `PICO_SDK_VERSION 2.1.1`):
  ```sh
  sudo apt-get install -y cmake ninja-build gcc-arm-none-eabi \
    libnewlib-arm-none-eabi build-essential libusb-1.0-0-dev git python3
  git clone --depth 1 -b 2.1.1 https://github.com/raspberrypi/pico-sdk.git
  export PICO_SDK_PATH="$PWD/pico-sdk"   # + submodules: git submodule update --init
  # picotool (or install from your package manager):
  git clone --depth 1 https://github.com/raspberrypi/picotool.git
  cmake -S picotool -B build-picotool && cmake --build build-picotool
  ```
  No special drivers: BOOTSEL shows up as a USB mass-storage drive
  (`RPI-RP2`); picotool talks to the bootrom over USB directly.
- **STM32 (Mini F401 / Black Pill):** [PlatformIO](https://platformio.org/)
  (`pip install platformio`), which pulls the ARM toolchain, the
  `ststm32` platform, and the Adafruit TinyUSB dependency by itself.
  For ROM-DFU flashing you need `dfu-util` on PATH
  (`apt install dfu-util` / `brew install dfu-util`); for SWD, an
  ST-Link probe plus `st-flash`/`openocd` and (on Linux) udev rules.
- **ESP32-S3 / ESP32-C3:** PlatformIO (`pip install platformio`), which
  pulls the `espressif32` platform and `esptool` automatically. No
  drivers on most systems (USB-Serial/JTAG or CDC); on Windows install
  the USB serial driver for your board if no COM port appears.

## Local Docker verification (mirrors CI)

Requires only [Docker](https://www.docker.com/) on the host — no
toolchains installed locally. Each target builds the working tree in a
throwaway container with the same steps as
`.github/workflows/release.yml` (same images, same pico-sdk tag, same
CMake flags, same PlatformIO envs):

```sh
make build            # all firmware targets below, stops on first failure
make build-rp2040     # Pi Pico: PICO_BOARD=pico -> *.uf2
make build-rp2040-zero
make build-rp2350     # Pico 2: KEYSTROKER_TARGET=rp2350 + PICO_BOARD=pico2
make build-stm32-mini # env stm32f401_mini -> firmware.bin (+ .hex)
make build-blackpill  # env blackpill_f401 -> firmware.bin (+ .hex)
make build-esp32s3    # env esp32s3-zero -> firmware.bin
```

First run is slow (each container downloads its toolchain, pico-sdk
2.1.1, or the PlatformIO platforms/Arduino core — ten minutes or more
per target); later runs reuse Docker's image layer cache. Per-target
logs live under `build/verify-*` (removed on success, kept on failure
with the log tail printed). The PlatformIO `common` symlink and `.pio/`
dirs are created inside the build and removed afterwards, so the repo
stays clean. `targets/esp32c3` is excluded: it is a console stub with no
USB-OTG peripheral, so it can never enumerate as a keyboard and has no
CI firmware build either.

## Raspberry Pi Pico / RP2040-Zero (RP2040)

Build at the repo root (`src/common` is compiled in; no symlink needed):

```sh
cmake -S . -B build/pico -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 \
  -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build build/pico        # -> build/pico/*.uf2
# or: make build-pico
```

RP2040-Zero class (same silicon, custom header: WS2812 on GPIO16, no
discrete LED, button default GPIO15) — select via `KEYSTROKER_TARGET`,
which shares `targets/rp2040` sources and sets the Zero USB PID/product
string plus `KEYSTROKER_LED_PIN=-1`:

```sh
cmake -S . -B build-zero -DKEYSTROKER_TARGET=rp2040-zero \
  -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build build-zero        # -> *.uf2
```

Flash — either drag-and-drop or picotool (commands per the
[picotool README](https://github.com/raspberrypi/picotool) — `load`
writes a file to a BOOTSEL device, `-x` executes/reboots after loading):

1. Hold **BOOTSEL**, plug the board in (still holding BOOTSEL), release.
   It appears as a mass-storage drive (`RPI-RP2`).
2. Copy the `.uf2` onto the drive — or flash from the terminal:
   ```sh
   picotool load build/pico/*.uf2 -x
   ```
   The board reboots into the keystroker on success.

## Raspberry Pi Pico 2 (RP2350)

```sh
cmake -S . -B build-pico2 -DKEYSTROKER_TARGET=rp2350 \
  -DPICO_SDK_PATH="$PICO_SDK_PATH"
cmake --build build-pico2       # -> *.uf2
# or: make build-pico2
```

Flash exactly like the Pico above (BOOTSEL + drag-drop, or
`picotool load <file>.uf2 -x`). UF2 is the Pi bootrom format — it
exists only for RP2040/RP2350 targets.

## STM32 Mini F401 / WeAct Black Pill (F401CC, F411CE²)

² F411CE is footprint/pin compatible on the Black Pill; rebuild with
the F411 target defines. USB device is PA11/PA12 on both boards; user
LED on PC13 (sink: LOW = on); trigger is an external button on
**PA0 → GND** (internal pull-up, active-low). **+5V pins tie directly
to USB +5V — never power from USB and an external supply at the same
time.**

Build (from the target dir; share the core first — on Windows copy
`src/common` to `common` instead of symlinking):

```sh
cd targets/stm32f401-mini && ln -s ../../src/common common
pio run -e stm32f401_mini   # -> .pio/build/stm32f401_mini/firmware.{bin,hex,dfu}
# Black Pill:
cd targets/blackpill && ln -s ../../src/common common
pio run -e blackpill_f401   # -> .pio/build/blackpill_f401/firmware.{bin,hex,dfu}
```

Flash option A — ROM DFU over USB (`upload_protocol = dfu` is already
set in both `platformio.ini` files):

1. Strap **BOOT0 = 1** (button/jumper) and tap **NRST** (or replug USB)
   so the chip boots its ROM DFU bootloader.
2. ```sh
   pio run -e stm32f401_mini -t upload    # or: dfu-util -a 0 -s 0x08000000 -D firmware.bin
   ```
3. Return BOOT0 to 0 and reset — the keystroker boots.

Flash option B — ST-Link (SWD on PA13/PA14): connect the probe and set
`upload_protocol = stlink` (see the comment in `platformio.ini`), then
`pio run -t upload`. Manual equivalents: `st-flash write firmware.bin
0x8000000`, or OpenOCD per the
[flash-programming docs](https://openocd.org/doc/html/Flash-Programming.html),
e.g. `openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program
firmware.bin exit 0x08000000"` (binary images need the explicit
`0x08000000` load address).

## ESP32-S3 Zero / DevKitC-1 (USB-OTG HID)

Trigger is the board's **BOOT button on GPIO0** (no wiring); optional
external button to GND on GPIO4. Use the board's **native-USB/OTG port**
(GPIO19 D- / GPIO20 D+); on two-port boards the UART port will not work
as a keyboard. `Serial` logs go to **UART0 (TX GPIO43 / RX GPIO44)** at
115200, not USB. There is no status LED (onboard WS2812 unused).

Build (from the target dir; share the core first):

```sh
cd targets/esp32s3 && ln -s ../../src/common common
pio run -e esp32s3-zero
# -> .pio/build/esp32s3-zero/firmware.bin (+ bootloader.bin, partitions.bin)
```

The build forces `-DARDUINO_USB_MODE=0 -DARDUINO_USB_CDC_ON_BOOT=0`
(USB-OTG/TinyUSB device mode). The S3 Arduino default (`=1`, Hardware
CDC + JTAG) has no TinyUSB device and cannot enumerate as HID —
`hal_port.cpp` `#error`s in that mode instead of shipping a dead
keyboard.

First flash — the ROM USB-DFU bootloader only listens while BOOT is
held at reset:

1. Hold **BOOT (GPIO0)**, tap **RESET** (or replug USB holding BOOT on
   boards without a RESET button), release BOOT.
2. ```sh
   pio run -e esp32s3-zero -t upload
   ```
   (`upload_protocol = esptool`; subsequent flashes usually work over
   USB without the button dance.)
3. Manual equivalent (per the
   [esptool docs](https://github.com/espressif/esptool/blob/master/docs/en/esptool/flashing-firmware.rst)
   — `write-flash` takes `<offset> <file>` pairs; exact build outputs
   print the offsets when you run `pio run`):
   ```sh
   esptool --chip esp32s3 write-flash 0x0 bootloader.bin \
     0x8000 partitions.bin 0x10000 firmware.bin
   ```

## ESP32-C3 Super Mini — documented STUB (no USB-HID possible)

The ESP32-C3 has **no USB-OTG peripheral** — only a fixed-function
USB-Serial/JTAG controller, so it can never enumerate as a USB keyboard
(see `targets/esp32c3/README.md` for sources). This target builds a
console-logging stub that reuses `src/common` and prints the keystrokes
it *would* have typed when you press BOOT (GPIO9). For a working
keyboard use `targets/esp32s3`, or a BLE-HID fork (out of scope).

```sh
cd targets/esp32c3 && ln -s ../../src/common common
pio run -e super-mini-c3 -t upload && pio device monitor
```

## Verify

1. Plug the flashed board into the target host (Pi: normal boot, not
   BOOTSEL; S3: the OTG port).
2. Confirm the device enumerates as a keyboard named `HID_KEYBOARD_NAME`
   (default `Arboard-2040-pico-keystroker`; fixed at compile time —
   rebuilding with a new name is the only way to change it): check the
   OS USB/keyboard list (`lsusb`, Device Manager, System Report…).
3. Open a text editor, focus it, and trigger once (press the button, or
   wait `COUNTDOWN_MS` in `time` mode — the countdown starts at USB
   mount, not power-on). The `input.txt` payload should appear,
   one ENTER per line, TABs where present. The board LED (where fitted)
   blinks to acknowledge each typing run.

## Troubleshooting

- **Wrong characters / garbled symbols, but the right length:** the
  `LANGUAGE` in `config.ini` does not match the host OS keyboard
  layout. Set `en_US | tr_TR | de_DE | fr_FR` to the host's layout and
  rebuild + reflash. (AZERTY digit/punctuation positions need Shift and
  are only partly covered — see `src/common/hid_keymap.h`.)
- **S3 builds and flashes but never types:** you are on
  `ARDUINO_USB_MODE=1` (Hardware CDC + JTAG, no TinyUSB device), or you
  plugged the UART-only port on a two-port board. The stock
  `platformio.ini` forces `=0`; if you overrode it, `hal_port.cpp`
  `#error`s at build time by design. Use the native-USB/OTG port.
- **C3 never shows up as a keyboard:** expected — the C3 has no USB-OTG
  (see above). It is a console stub, not HID.
- **`picotool` / drive does not appear (Pi):** you are not in BOOTSEL
  mode — hold BOOTSEL *while* plugging in. A normal boot enumerates as
  a keyboard, which picotool cannot see.
- **STM32 DFU not found:** BOOT0 was not strapped at reset, or
  `dfu-util` is missing. Re-strap BOOT0=1 + NRST and check `dfu-util -l`.
- **Nothing types in `time` mode:** `COUNTDOWN_MS` (1..3600000) counts
  from USB mount; an unmounted device never starts the countdown. In
  `button` mode `COUNTDOWN_MS` is ignored.
- **Double press typed once:** by design — a second press inside
  `DOUBLE_CLICK_MS` is dropped (`BUTTON_EVENT_DROPPED`).
- **Your `#` notes got typed:** `input.txt` has no comment syntax; every
  byte is payload. Strip the shipped `#` header lines if you do not want
  them typed.
