# Host-testable common core: unit tests + coverage (no Pico SDK needed).
#
#   make test       build & run host unit tests (gcc/clang, gcov)
#   make validate   target-contract checks (tools/validate_targets.py) +
#                   S3 host contract test (tests/test_s3_contract.c);
#                   runs automatically as part of `make test`
#   make coverage   test + enforce >=90% line coverage on src/common
#   make embed      regenerate generated/payload.h from config/
#   make build      help for firmware builds (owned by targets/ session)
#   make build-pico / make build-pico2  firmware build entry points
#   make clean      remove build artefacts
#   make help       this message
#
# Works on macOS (Apple clang) and Linux CI (gcc). No cmake needed here.

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

.PHONY: help test validate coverage build build-pico build-pico2 clean embed

help:
	@echo "pico-keystroker common core"
	@echo ""
	@echo "  make test        build & run host unit tests (+ validate)"
	@echo "  make validate    target contract checks + S3 host contract test"
	@echo "  make coverage    test + require >=$(COV_THRESHOLD)% line coverage on src/common"
	@echo "  make embed       regenerate generated/payload.h from config/"
	@echo "  make build       firmware build help (see targets/ + CMakeLists)"
	@echo "  make build-pico  build firmware for Pico (RP2040)"
	@echo "  make build-pico2 build firmware for Pico 2 (RP2350)"
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

build:
	@echo "Firmware builds live in targets/ (Pico SDK + TinyUSB)."
	@echo "This Makefile only covers the host-testable core."
	@echo ""
	@echo "Once the firmware sources exist, configure with CMake, e.g.:"
	@echo "  cmake -S . -B build/pico  -DPICO_BOARD=pico  -DPICO_PLATFORM=rp2040"
	@echo "  cmake --build build/pico"
	@echo "or use the platformio project (see platformio.ini when added)."
	@echo ""
	@echo "Host tests: make test | Coverage: make coverage | Embed: make embed"

build-pico:
	@if test -f CMakeLists.txt && command -v cmake >/dev/null 2>&1; then \
		cmake -S . -B build/pico -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 && \
		cmake --build build/pico; \
	else \
		echo "build-pico: firmware sources/CMakeLists.txt not present yet (owned by targets/ session)."; \
		echo "Intended build:"; \
		echo "  cmake -S . -B build/pico -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040"; \
		echo "  cmake --build build/pico"; \
	fi

build-pico2:
	@if test -f CMakeLists.txt && command -v cmake >/dev/null 2>&1; then \
		cmake -S . -B build/pico2 -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350 && \
		cmake --build build/pico2; \
	else \
		echo "build-pico2: firmware sources/CMakeLists.txt not present yet (owned by targets/ session)."; \
		echo "Intended build:"; \
		echo "  cmake -S . -B build/pico2 -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350"; \
		echo "  cmake --build build/pico2"; \
	fi

clean:
	rm -rf $(BUILD) build
	rm -f ./*.gcov
	rm -f generated/payload.h
