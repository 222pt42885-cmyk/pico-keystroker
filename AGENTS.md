# AGENTS.md — contributor guide

## Layout

- `src/common/` — shared core, the only logic that matters: `config_parser`,
  `hid_keymap`, `typing_engine`, `button` (+ `hal.h` interface). Host-testable.
- `targets/<board>/` — thin per-board shims: `main.c` super-loop + `hal_port.c`
  + `board_config.h` + build file. Never built on the host. All 7 dirs:
  `rp2040`, `rp2040-zero` (shares rp2040 sources via CMake), `rp2350`,
  `stm32f401-mini`, `blackpill`, `esp32s3` (C++, `main.cpp`/`hal_port.cpp`),
  `esp32c3` (console stub — no USB-HID possible, HAL inline in `main.cpp`).
- `config/` — `config.ini` (settings) + `input.txt` (typed payload; ships
  with `#` header lines that ARE typed — the payload has no comment syntax).
- `INSTALL.md` — per-target build/flash/verify guide (README points there).
- `assets/banner.webp` — README header image (binary lands separately).
- `generated/` — `payload.h` output of `tools/embed.py` (do not commit).
- `tools/embed.py` — bakes `config/*` into `generated/payload.h` (byte arrays
  + `_len`, trailing NUL so they double as C strings).
- `tests/` — minunit suites, one per core module (4 suites).
- `cmake/`, `CMakeLists.txt` — Pico firmware build; `Makefile` — host only.

## Conventions

- `src/common` is strict C99, `-Wall -Wextra` clean, **no MCU headers**
  (no `pico/`, no `tusb.h`, no `Arduino.h`), **no malloc** — caller-owned
  structs, fixed-size buffers. Keep it that way; host tests must keep passing.
- `targets/` files are thin HAL shims: init core, poll button, call the
  typing engine. USB pumping belongs in `hal_port.c` (interrupt), not `main.c`.
- Config is embedded at compile time (`payload.h`); there is no filesystem.
  `WRITE_MODE=button|time` (`time` counts from USB mount), `COUNTDOWN_MS`
  1..3600000 (ignored in button mode), `LANGUAGE` en_US|tr_TR|de_DE|fr_FR.
- HAL contract (6 functions, `src/common/hal.h`): `hal_init`,
  `hal_is_mounted`, `hal_hid_ready`, `hal_hid_send(modifier, keycode[6])`,
  `hal_delay_ms`, `hal_button_read` (pressed = true, board inverts if needed).
  There is deliberately **no `hal_millis`** — targets own their tick
  (`get_absolute_time` on Pi, `HAL_GetTick` on STM32, `millis` on Arduino).
- Button: `button_init(&b, debounce_ms, double_click_ms)` (2nd arg is the
  ignore/double-click window); `button_update(&b, raw, now_ms)` returns
  `PRESS` / `DROPPED` (2nd click in window) / `NONE`. Unsigned ms math.
- Typing: `typing_engine_init(&eng, &(typing_engine_config_t){language,
  typing_delay_ms})`, then `typing_engine_type_string(eng, str, emit_cb, user)`;
  the callback does `hal_hid_send()` + `hal_delay_ms(typing_engine_delay_ms(eng))`.
  `'\n'`/`'\r'` → ENTER, `'\t'` → TAB, unmapped bytes skipped (`chars_skipped`).
- Naming: `config_parse_string` < `config_validate` (`CFG_OK == 0`, check both);
  `keystroker_payload_text/_len`, `keystroker_config_text/_len`.

## Build / test

```sh
make test                 # host unit tests, must be 4/4 PASS (runs validate too)
make validate             # tools/validate_targets.py contract checks over every
                          # targets/ dir (required files, pio envs, 6 HAL symbols,
                          # core refs) + tests/test_s3_contract.c
make coverage             # same + >=90% line gate on src/common
make embed                # regenerate generated/payload.h from config/
cmake -S . -B build/pico -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 && cmake --build build/pico
# Pi Zero: -DKEYSTROKER_TARGET=rp2040-zero — NEVER hand-pass LED/button -Ds
# (the target sets PID/product string/LED_PIN=-1/WS2812_PIN=16 itself).
# Pico 2: -DKEYSTROKER_TARGET=rp2350
(cd targets/<stm32> && ln -s ../../src/common common && pio run)  # -> .bin/.hex
(cd targets/esp32s3 && ln -s ../../src/common common && pio run -e esp32s3-zero)  # -> firmware.bin
```

CI runs `make test` + `make coverage` per PR/push (+ an S3 firmware build).
Release: every push to `main` moves the floating `latest` tag + Release to
fresh binaries (`.github/workflows/release.yml`) — Pi ships `.uf2` (incl.
`keystroker-rp2040-zero.uf2`), STM32 `.bin`/`.hex`
(`keystroker-stm32f401-mini.*`, `keystroker-blackpill-f401cc.*`), S3
`keystroker-esp32s3-zero.bin`. Flash steps live in `INSTALL.md`.

## Adding a board

1. `targets/<name>/`: `board_config.h` (pins, USB notes), `hal_port.c`
   (the 6 HAL functions), `main.c` (copy `targets/rp2040/main.c` loop and
   adapt the tick source), build file (`platformio.ini`: symlink
   `../../src/common` → `./common`, compile `common/*.c`).
2. Reuse `src/common` as-is. Add a row to the matrix in `targets/README.md`
   (+ the compact tables in `README.md`) and flash steps in `INSTALL.md`.

## Language table

`src/common/hid_keymap.*`: per-`lang_t` ASCII→(keycode, modifier) tables in
Arduino `_asciimap` style. To add a layout: new `LANG_*` value, new tables,
extend `lang_from_string`/`lang_to_string`, cover with `tests/test_hid_keymap.c`.

## What NOT to do

- No QMK, no `stdio-USB` + TinyUSB together, no UTF-8 claims (ASCII only,
  unmapped bytes are skipped — host layout must match `LANGUAGE`).
- Do not commit `build/`, `generated/`, `*.uf2`, `*.o`, `*.gcda`, `.pio/`
  (see `.gitignore`); do not add MCU includes or allocation to `src/common`.
- Do not invent APIs — read `src/common/*.h` first. Sibling-owned paths
  (`src/common`, `tests`, `Makefile`, `CMakeLists.txt`, `targets/rp2040*`,
  `targets/rp2350*`, `*- hal_port.c / board_config.h / platformio.ini`,
  `targets/README.md`, `.github`, `cmake`, `pico_sdk_import.cmake`) are
  read-only for your change unless the task says otherwise.

## ESP32-S3 notes (`targets/esp32s3`, env `esp32s3-zero`)

- Arduino C++ target: `main.cpp` (setup/loop) + `hal_port.cpp` (not `.c`).
  Same 6-function HAL + same tick/typing shapes as the `.c` targets.
- The `USBHIDKeyboard Keyboard` object is **owned by `hal_port.cpp`**
  (`main.cpp` only declares `extern` and calls `Keyboard.begin()`).
- Setup ordering matters, it is load-bearing: `Keyboard.begin()` first,
  then `USB.manufacturerName()` / `USB.productName()`, then `USB.begin()`
  last — USB strings are descriptors fixed at enumeration time.
  Tick source is `millis()` (unsigned math stays wrap-safe); TinyUSB device
  work runs in the Arduino-ESP32 background task, so `main.cpp` never pumps.
- Build needs `-DARDUINO_USB_MODE=0 -DARDUINO_USB_CDC_ON_BOOT=0` (forced in
  `platformio.ini`; `hal_port.cpp` `#error`s otherwise). `Serial` is UART0.
- No LED driver: the onboard WS2812 is not a plain GPIO LED, deliberately
  unused. Trigger is BOOT on GPIO0 (+ optional external on GPIO4).
