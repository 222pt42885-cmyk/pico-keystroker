# Host-testable common core: unit tests + coverage (no Pico SDK needed)
# plus Docker-backed firmware verification (mirrors CI release.yml).
#
#   make test       build & run host unit tests (gcc/clang, gcov)
#   make validate   target-contract checks (tools/validate_targets.py) +
#                   S3 host contract test (tests/test_s3_contract.c);
#                   runs automatically as part of `make test`
#   make coverage   test + enforce >=90% line coverage on src/common
#   make embed      regenerate generated/payload.h from config/
#   make build      Docker gate: ALL firmware builds below (fails fast)
#   make build-rp2040 / build-rp2040-zero / build-rp2350
#                   Pi firmware via Docker (ubuntu:24.04 + pico-sdk 2.1.1)
#   make build-stm32-mini / build-blackpill / build-esp32s3
#                   PlatformIO firmware via Docker (python:3.12 + pio)
#   make build-pico / make build-pico2  aliases (rp2040 / rp2350)
#   make clean      remove build artefacts
#   make help       this message
#
# Works on macOS (Apple clang) and Linux CI (gcc). No cmake needed on the
# host for `test`; firmware builds run inside Docker (only `docker` needed).

CC       ?= gcc
CFLAGS   ?= -std=c99 -Wall -Wextra -g -O0
COVFLAGS  = --coverage
INCLUDES  = -Isrc/common -Itests

BUILD        := build/host
COMMON_SRCS  := src/common/config_parser.c \
                src/common/hid_keymap.c \
                src/common/typing_engine.c \
                src/common/button.c
COMMON_OBJS  := $(patsubst src/common/%.c,$(BUILD)/%.o,$(COMMON_SRCS))
TEST_NAMES   := test_config_parser test_hid_keymap test_typing_engine test_button
TEST_BINS    := $(addprefix $(BUILD)/,$(TEST_NAMES))

COMMON_HDRS := $(wildcard src/common/*.h)
TEST_DEPS   := tests/minunit.h

COV_THRESHOLD := 90

.DEFAULT_GOAL := help

# Keep compiled objects (pattern-rule outputs) so incremental builds work.
.SECONDARY:

.PHONY: help test validate coverage build build-rp2040 build-rp2040-zero build-rp2350 build-stm32-mini build-blackpill build-esp32s3 build-pico build-pico2 clean embed

help:
	@echo "pico-keystroker common core + Docker firmware verification"
	@echo ""
	@echo "  make test        build & run host unit tests (+ validate)"
	@echo "  make validate    target contract checks + S3 host contract test"
	@echo "  make coverage    test + require >=$(COV_THRESHOLD)% line coverage on src/common"
	@echo "  make embed       regenerate generated/payload.h from config/"
	@echo "  make build       Docker gate: ALL firmware builds below (stops on first failure)"
	@echo "  make build-rp2040       Pi Pico (RP2040) via Docker (PICO_BOARD=pico)"
	@echo "  make build-rp2040-zero  RP2040-Zero via Docker (KEYSTROKER_TARGET=rp2040-zero)"
	@echo "  make build-rp2350       Pico 2 (RP2350) via Docker"
	@echo "  make build-stm32-mini   Mini F401 via Docker (env stm32f401_mini)"
	@echo "  make build-blackpill    Black Pill via Docker (env blackpill_f401)"
	@echo "  make build-esp32s3      ESP32-S3 via Docker (env esp32s3-zero)"
	@echo "  (esp32c3 excluded: console stub, no USB-OTG HID possible)"
	@echo "  make build-pico / make build-pico2  aliases for build-rp2040 / build-rp2350"
	@echo "  make clean       remove build artefacts"
	@echo "  make help        this message"

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/common/%.c $(COMMON_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(COVFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/test_%: tests/test_%.c $(COMMON_OBJS) $(TEST_DEPS) $(COMMON_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) $(COVFLAGS) $(INCLUDES) $< $(COMMON_OBJS) -o $@

test: $(TEST_BINS) validate
	@rm -f $(BUILD)/*.gcda
	@pass=0; fail=0; \
	for t in $(TEST_BINS); do \
		if ./$$t; then pass=$$((pass + 1)); else fail=$$((fail + 1)); fi; \
	done; \
	echo "suites passed: $$pass, failed: $$fail"; \
	test $$fail -eq 0

# Target-contract validation: static checks over every targets/ dir
# (required files, platformio envs, 6 HAL symbols, core API references)
# plus the S3 host contract test, compiled here against src/common
# (target files never build on the host). Runs with `make test`.
validate: $(COMMON_OBJS) | $(BUILD)
	python3 tools/validate_targets.py
	$(CC) $(CFLAGS) $(COVFLAGS) $(INCLUDES) tests/test_s3_contract.c $(COMMON_OBJS) -o $(BUILD)/test_s3_contract
	./$(BUILD)/test_s3_contract

coverage: test
	@if command -v gcovr >/dev/null 2>&1; then \
		echo "coverage: using gcovr"; \
		gcovr --root . --filter 'src/common/[^/]+\.c' \
			--print-summary --fail-under-line $(COV_THRESHOLD); \
	else \
		echo "coverage: gcovr not found, using gcov fallback"; \
		sh tools/coverage.sh $(BUILD) $(COV_THRESHOLD); \
	fi

embed:
	python3 tools/embed.py --input config/input.txt \
		--config config/config.ini --output generated/payload.h

# --- Docker-backed firmware verification (mirrors .github/workflows/release.yml) ---
#
# Each target builds the working tree inside a throwaway container with the
# same steps CI uses (same base image, same SDK tag, same CMake flags and
# PlatformIO envs). Scratch logs live under build/verify-<name>/: removed on
# success, kept on failure (log tail is printed). Needs only Docker on the
# host (Linux + macOS); first run is slow (toolchain/SDK/framework downloads).
# targets/esp32c3 is excluded: console stub, no USB-OTG, no CI firmware build.

# All firmware targets: runs each in order, stops on the first failure.
build: build-rp2040 build-rp2040-zero build-rp2350 build-stm32-mini build-blackpill build-esp32s3
	@echo "build: ALL FIRMWARE BUILDS PASSED (rp2040, rp2040-zero, rp2350, stm32-mini, blackpill, esp32s3)"

# RP2040 Pico: same flags as CI build-rp2040 (PICO_BOARD=pico, no KEYSTROKER_TARGET).
build-rp2040:
	@mkdir -p build/verify-rp2040 && \
	docker run --rm -v "$(CURDIR):/work" -w /work ubuntu:24.04 \
	bash -c 'set -o pipefail; { set -eu; \
	export DEBIAN_FRONTEND=noninteractive; \
	apt-get update -o Acquire::Retries=3 -o Acquire::http::Timeout=30; \
	apt-get install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libusb-1.0-0-dev git python3; \
	git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk; \
	git -C /opt/pico-sdk submodule update --init --depth 1; \
	git clone --depth 1 https://github.com/raspberrypi/picotool.git /tmp/picotool; \
	cmake -S /tmp/picotool -B /tmp/build-picotool -G Ninja -DPICO_SDK_PATH=/opt/pico-sdk; \
	cmake --build /tmp/build-picotool; \
	cmake --install /tmp/build-picotool --prefix /tmp/picotool-install; \
	export PATH=/tmp/picotool-install/bin:$$PATH; \
	cmake -S /work -B /work/build/verify-rp2040 -G Ninja -DPICO_BOARD=pico -DPICO_SDK_PATH=/opt/pico-sdk -Dpicotool_DIR=/tmp/picotool-install/lib/cmake/picotool -DCMAKE_C_STANDARD=11; \
	cmake --build /work/build/verify-rp2040; \
	ls -l /work/build/verify-rp2040/*.uf2; \
	} 2>&1 | tee /work/build/verify-rp2040/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-rp2040: PASS"; ls -l build/verify-rp2040/*.uf2; rm -rf build/verify-rp2040; \
	else echo "build-rp2040: FAILED (rc=$$rc) -- scratch kept at build/verify-rp2040; log tail:"; \
	if [ -f build/verify-rp2040/build.log ]; then tail -n 50 build/verify-rp2040/build.log; fi; exit 1; fi

# RP2040-Zero: same flags as CI build-rp2040-zero (KEYSTROKER_TARGET only,
# never hand-pass PICO_BOARD here -- it would override the target mapping).
build-rp2040-zero:
	@mkdir -p build/verify-rp2040-zero && \
	docker run --rm -v "$(CURDIR):/work" -w /work ubuntu:24.04 \
	bash -c 'set -o pipefail; { set -eu; \
	export DEBIAN_FRONTEND=noninteractive; \
	apt-get update -o Acquire::Retries=3 -o Acquire::http::Timeout=30; \
	apt-get install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libusb-1.0-0-dev git python3; \
	git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk; \
	git -C /opt/pico-sdk submodule update --init --depth 1; \
	git clone --depth 1 https://github.com/raspberrypi/picotool.git /tmp/picotool; \
	cmake -S /tmp/picotool -B /tmp/build-picotool -G Ninja -DPICO_SDK_PATH=/opt/pico-sdk; \
	cmake --build /tmp/build-picotool; \
	cmake --install /tmp/build-picotool --prefix /tmp/picotool-install; \
	export PATH=/tmp/picotool-install/bin:$$PATH; \
	cmake -S /work -B /work/build/verify-rp2040-zero -G Ninja -DKEYSTROKER_TARGET=rp2040-zero -DPICO_SDK_PATH=/opt/pico-sdk -Dpicotool_DIR=/tmp/picotool-install/lib/cmake/picotool -DCMAKE_C_STANDARD=11; \
	cmake --build /work/build/verify-rp2040-zero; \
	ls -l /work/build/verify-rp2040-zero/*.uf2; \
	} 2>&1 | tee /work/build/verify-rp2040-zero/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-rp2040-zero: PASS"; ls -l build/verify-rp2040-zero/*.uf2; rm -rf build/verify-rp2040-zero; \
	else echo "build-rp2040-zero: FAILED (rc=$$rc) -- scratch kept at build/verify-rp2040-zero; log tail:"; \
	if [ -f build/verify-rp2040-zero/build.log ]; then tail -n 50 build/verify-rp2040-zero/build.log; fi; exit 1; fi

# RP2350 Pico 2: same flags as CI build-rp2350 (KEYSTROKER_TARGET + PICO_BOARD=pico2).
build-rp2350:
	@mkdir -p build/verify-rp2350 && \
	docker run --rm -v "$(CURDIR):/work" -w /work ubuntu:24.04 \
	bash -c 'set -o pipefail; { set -eu; \
	export DEBIAN_FRONTEND=noninteractive; \
	apt-get update -o Acquire::Retries=3 -o Acquire::http::Timeout=30; \
	apt-get install -y cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libusb-1.0-0-dev git python3; \
	git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk; \
	git -C /opt/pico-sdk submodule update --init --depth 1; \
	git clone --depth 1 https://github.com/raspberrypi/picotool.git /tmp/picotool; \
	cmake -S /tmp/picotool -B /tmp/build-picotool -G Ninja -DPICO_SDK_PATH=/opt/pico-sdk; \
	cmake --build /tmp/build-picotool; \
	cmake --install /tmp/build-picotool --prefix /tmp/picotool-install; \
	export PATH=/tmp/picotool-install/bin:$$PATH; \
	cmake -S /work -B /work/build/verify-rp2350 -G Ninja -DKEYSTROKER_TARGET=rp2350 -DPICO_BOARD=pico2 -DPICO_SDK_PATH=/opt/pico-sdk -Dpicotool_DIR=/tmp/picotool-install/lib/cmake/picotool -DCMAKE_C_STANDARD=11; \
	cmake --build /work/build/verify-rp2350; \
	ls -l /work/build/verify-rp2350/*.uf2; \
	} 2>&1 | tee /work/build/verify-rp2350/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-rp2350: PASS"; ls -l build/verify-rp2350/*.uf2; rm -rf build/verify-rp2350; \
	else echo "build-rp2350: FAILED (rc=$$rc) -- scratch kept at build/verify-rp2350; log tail:"; \
	if [ -f build/verify-rp2350/build.log ]; then tail -n 50 build/verify-rp2350/build.log; fi; exit 1; fi

# STM32 Mini F401: same steps as CI build-stm32 (common symlink + pio run -e).
# .pio/ + symlink are always removed afterwards so the repo stays clean.
build-stm32-mini:
	@mkdir -p build/verify-stm32-mini && \
	docker run --rm -v "$(CURDIR):/work" -w /work python:3.12 \
	bash -c 'set -o pipefail; { set -eu; \
	pip install platformio; \
	cd /work/targets/stm32f401-mini; \
	if [ -e common ] && [ ! -L common ]; then echo "common exists and is not a symlink -- refusing to overwrite"; exit 1; fi; \
	ln -sfn ../../src/common common; \
	pio run -e stm32f401_mini; \
	ls -l .pio/build/stm32f401_mini/firmware.bin; \
	ls -l .pio/build/stm32f401_mini/firmware.hex || true; \
	} 2>&1 | tee /work/build/verify-stm32-mini/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-stm32-mini: PASS"; ls -l targets/stm32f401-mini/.pio/build/stm32f401_mini/firmware.bin; \
	else echo "build-stm32-mini: FAILED (rc=$$rc); log tail:"; \
	if [ -f build/verify-stm32-mini/build.log ]; then tail -n 50 build/verify-stm32-mini/build.log; fi; fi; \
	rm -rf targets/stm32f401-mini/.pio; \
	if [ -L targets/stm32f401-mini/common ]; then rm targets/stm32f401-mini/common; fi; \
	if [ $$rc -eq 0 ]; then rm -rf build/verify-stm32-mini; \
	else echo "scratch kept at build/verify-stm32-mini"; exit 1; fi

# Black Pill F401CC: same shape as stm32-mini (env blackpill_f401).
build-blackpill:
	@mkdir -p build/verify-blackpill && \
	docker run --rm -v "$(CURDIR):/work" -w /work python:3.12 \
	bash -c 'set -o pipefail; { set -eu; \
	pip install platformio; \
	cd /work/targets/blackpill; \
	if [ -e common ] && [ ! -L common ]; then echo "common exists and is not a symlink -- refusing to overwrite"; exit 1; fi; \
	ln -sfn ../../src/common common; \
	pio run -e blackpill_f401; \
	ls -l .pio/build/blackpill_f401/firmware.bin; \
	ls -l .pio/build/blackpill_f401/firmware.hex || true; \
	} 2>&1 | tee /work/build/verify-blackpill/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-blackpill: PASS"; ls -l targets/blackpill/.pio/build/blackpill_f401/firmware.bin; \
	else echo "build-blackpill: FAILED (rc=$$rc); log tail:"; \
	if [ -f build/verify-blackpill/build.log ]; then tail -n 50 build/verify-blackpill/build.log; fi; fi; \
	rm -rf targets/blackpill/.pio; \
	if [ -L targets/blackpill/common ]; then rm targets/blackpill/common; fi; \
	if [ $$rc -eq 0 ]; then rm -rf build/verify-blackpill; \
	else echo "scratch kept at build/verify-blackpill"; exit 1; fi

# ESP32-S3: same steps as CI build-esp32s3 (common symlink + pio run -e esp32s3-zero).
build-esp32s3:
	@mkdir -p build/verify-esp32s3 && \
	docker run --rm -v "$(CURDIR):/work" -w /work python:3.12 \
	bash -c 'set -o pipefail; { set -eu; \
	pip install platformio; \
	cd /work/targets/esp32s3; \
	if [ -e common ] && [ ! -L common ]; then echo "common exists and is not a symlink -- refusing to overwrite"; exit 1; fi; \
	ln -sfn ../../src/common common; \
	pio run -e esp32s3-zero; \
	ls -l .pio/build/esp32s3-zero/firmware.bin; \
	} 2>&1 | tee /work/build/verify-esp32s3/build.log'; \
	rc=$$?; \
	if [ $$rc -eq 0 ]; then echo "build-esp32s3: PASS"; ls -l targets/esp32s3/.pio/build/esp32s3-zero/firmware.bin; \
	else echo "build-esp32s3: FAILED (rc=$$rc); log tail:"; \
	if [ -f build/verify-esp32s3/build.log ]; then tail -n 50 build/verify-esp32s3/build.log; fi; fi; \
	rm -rf targets/esp32s3/.pio; \
	if [ -L targets/esp32s3/common ]; then rm targets/esp32s3/common; fi; \
	if [ $$rc -eq 0 ]; then rm -rf build/verify-esp32s3; \
	else echo "scratch kept at build/verify-esp32s3"; exit 1; fi

build-pico: build-rp2040

build-pico2: build-rp2350

clean:
	rm -rf $(BUILD) build
	rm -f ./*.gcov
	rm -f generated/payload.h
