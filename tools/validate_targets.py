#!/usr/bin/env python3
"""Target-contract validation for pico-keystroker firmware ports.

For every known target dir under targets/ this script asserts:

  1. Required files exist (board_config.h / hal / main / build file, ...).
  2. Each platformio.ini parses and declares its expected env section(s)
     (with platform/board/framework keys).
  3. Each HAL implementation file defines all 6 HAL symbols from
     src/common/hal.h: hal_init, hal_is_mounted, hal_hid_ready,
     hal_hid_send, hal_delay_ms, hal_button_read.
  4. Each main file references button_init + typing_engine_type_string
     (i.e. it drives the shared src/common core instead of forking it).

Pi Zero-class boards share sources instead of duplicating them
(rp2040-zero reuses targets/rp2040/main.c + hal_port.c via CMake;
rp2350 reuses targets/rp2040/hal_port.c), so those entries point at the
shared files and additionally assert the local CMakeLists references them.

Boards without a separate hal file (esp32c3: console stub with the HAL
inline in main.cpp) point the HAL check at main.cpp.

Exit 0 with a per-target PASS summary, or exit 1 listing every failure.
Stdlib only (configparser, pathlib, re, sys).
"""

import configparser
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGETS = ROOT / "targets"

HAL_SYMBOLS = (
    "hal_init",
    "hal_is_mounted",
    "hal_hid_ready",
    "hal_hid_send",
    "hal_delay_ms",
    "hal_button_read",
)

# Per-target expectations. "hal"/"main" are paths relative to targets/
# (shared files for boards that reuse a sibling's sources). "cmake_refs"
# optionally asserts substrings in the local CMakeLists (source sharing).
EXPECT = {
    "rp2040": {
        "files": ["main.c", "hal_port.c", "board_config.h", "CMakeLists.txt"],
        "hal": "rp2040/hal_port.c",
        "main": "rp2040/main.c",
        "pio_envs": [],
    },
    "rp2040-zero": {
        "files": ["CMakeLists.txt", "rp2040_zero.h"],
        "hal": "rp2040/hal_port.c",  # shared, see local CMakeLists
        "main": "rp2040/main.c",  # shared, see local CMakeLists
        "pio_envs": [],
        "cmake_refs": ["hal_port.c", "main.c"],
    },
    "rp2350": {
        "files": ["main.c", "board_config.h", "CMakeLists.txt"],
        "hal": "rp2040/hal_port.c",  # shared, see local CMakeLists
        "main": "rp2350/main.c",
        "pio_envs": [],
        "cmake_refs": ["hal_port.c"],
    },
    "stm32f401-mini": {
        "files": ["main.c", "hal_port.c", "board_config.h", "platformio.ini"],
        "hal": "stm32f401-mini/hal_port.c",
        "main": "stm32f401-mini/main.c",
        "pio_envs": ["stm32f401_mini"],
    },
    "blackpill": {
        "files": ["main.c", "hal_port.c", "board_config.h", "platformio.ini"],
        "hal": "blackpill/hal_port.c",
        "main": "blackpill/main.c",
        "pio_envs": ["blackpill_f401"],
    },
    "esp32c3": {
        "files": ["main.cpp", "platformio.ini", "README.md"],
        # Stub: HAL is console-logging functions inline in main.cpp.
        "hal": "esp32c3/main.cpp",
        "main": "esp32c3/main.cpp",
        "pio_envs": ["super-mini-c3"],
    },
    "esp32s3": {
        "files": [
            "main.cpp",
            "hal_port.cpp",
            "board_config.h",
            "platformio.ini",
            "README.md",
        ],
        "hal": "esp32s3/hal_port.cpp",
        "main": "esp32s3/main.cpp",
        "pio_envs": ["esp32s3-zero"],
    },
}

# A HAL definition looks like "<ret> <name>(" at top level; call sites and
# bare mentions in comments never have a "void"/"bool" return type prefix.
HAL_DEF = re.compile(r"\b(?:void|bool)\s+(%s)\s*\(" % "|".join(HAL_SYMBOLS))

# Shim include: some mains reuse a sibling verbatim
# (e.g. rp2350/main.c is just `#include "../rp2040/main.c"`).
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def read_with_shims(path):
    """File text plus the text of any relative #include "..." shims it
    pulls in (one level, cycle-safe), so verbatim-sharing mains still
    satisfy the core-API reference check."""
    seen = set()
    texts = []

    def visit(p):
        p = p.resolve()
        if p in seen or not p.is_file():
            return
        seen.add(p)
        text = p.read_text(encoding="utf-8")
        texts.append(text)
        for inc in INCLUDE_RE.findall(text):
            visit(p.parent / inc)

    visit(Path(path))
    return "\n".join(texts)


def check_target(name, spec, errors):
    """Validate one target dir. Returns True when every check passes."""
    ok = True
    tdir = TARGETS / name
    if not tdir.is_dir():
        errors.append("[%s]: missing target dir targets/%s" % (name, name))
        return False

    for fname in spec["files"]:
        if not (tdir / fname).is_file():
            errors.append("[%s]: missing required file: %s" % (name, fname))
            ok = False

    for env in spec["pio_envs"]:
        ini = tdir / "platformio.ini"
        if not ini.is_file():
            errors.append(
                "[%s]: missing platformio.ini (expected env %s)" % (name, env)
            )
            ok = False
            continue
        try:
            cfg = configparser.ConfigParser(interpolation=None)
            cfg.read(str(ini))
        except configparser.Error as exc:
            errors.append("[%s]: platformio.ini does not parse: %s" % (name, exc))
            ok = False
            continue
        section = "env:%s" % env
        if not cfg.has_section(section):
            errors.append(
                "[%s]: platformio.ini declares no [%s] env" % (name, section)
            )
            ok = False
            continue
        for key in ("platform", "board", "framework"):
            if not cfg.has_option(section, key):
                errors.append(
                    "[%s]: [%s] is missing '%s'" % (name, section, key)
                )
                ok = False

    hal_path = TARGETS / spec["hal"]
    if not hal_path.is_file():
        errors.append("[%s]: missing HAL file: %s" % (name, spec["hal"]))
        ok = False
    else:
        found = set(HAL_DEF.findall(hal_path.read_text(encoding="utf-8")))
        for sym in HAL_SYMBOLS:
            if sym not in found:
                errors.append(
                    "[%s]: %s defines no '%s()'" % (name, spec["hal"], sym)
                )
                ok = False

    main_path = TARGETS / spec["main"]
    if not main_path.is_file():
        errors.append("[%s]: missing main file: %s" % (name, spec["main"]))
        ok = False
    else:
        # Follow verbatim-sharing #include shims (see read_with_shims).
        text = read_with_shims(main_path)
        for api in ("button_init", "typing_engine_type_string"):
            if api not in text:
                errors.append(
                    "[%s]: %s never references '%s'"
                    % (name, spec["main"], api)
                )
                ok = False

    if "cmake_refs" in spec:
        cmake = tdir / "CMakeLists.txt"
        if cmake.is_file():
            text = cmake.read_text(encoding="utf-8")
            for ref in spec["cmake_refs"]:
                if ref not in text:
                    errors.append(
                        "[%s]: CMakeLists.txt never references shared '%s'"
                        % (name, ref)
                    )
                    ok = False

    if ok:
        print("PASS [%s]" % name)
    return ok


def main():
    errors = []
    passed = 0
    for name in EXPECT:
        if check_target(name, EXPECT[name], errors):
            passed += 1
    print("targets checked: %d, passed: %d, failed: %d"
          % (len(EXPECT), passed, len(EXPECT) - passed))
    if errors:
        print("FAILURES:")
        for err in errors:
            print("  FAIL %s" % err)
        return 1
    print("PASS target contract (%d targets)" % len(EXPECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
