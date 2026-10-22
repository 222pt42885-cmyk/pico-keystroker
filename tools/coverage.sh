#!/bin/sh
# Union line coverage over src/common/*.c using gcov.
# Usage: sh tools/coverage.sh [build-dir] [threshold]
# Expects the test binaries to have run already (build-dir/*.gcda present).
# Fails (exit 1) when overall line coverage is below the threshold.
set -u

BUILD="${1:-build/host}"
THRESHOLD="${2:-90}"

if ! command -v gcov >/dev/null 2>&1; then
    echo "coverage: 'gcov' not found on PATH" >&2
    exit 2
fi

total_exec=0
total_lines=0
failed=0

for src in src/common/*.c; do
    out=$(gcov -o "$BUILD" "$src" 2>&1) || {
        echo "coverage: gcov failed for $src" >&2
        echo "$out" >&2
        failed=1
        continue
    }
    # Take the "Lines executed" summary belonging to this source file.
    line=$(printf '%s\n' "$out" | awk -v want="$src" '
        /^File / { active = (index($0, want) > 0) }
        active && /^Lines executed:/ { print; active = 0; exit }
    ')
    if [ -z "$line" ]; then
        echo "coverage: no gcov data for $src (run 'make test' first?)" >&2
        echo "$out" >&2
        failed=1
        continue
    fi
    pct=$(printf '%s' "$line" | sed -E 's/.*:([0-9.]+)%.*/\1/')
    n=$(printf '%s' "$line" | sed -E 's/.*of ([0-9]+).*/\1/')
    echo "coverage: $src: $line"
    total_exec=$(awk "BEGIN { print $total_exec + ($pct / 100) * $n }")
    total_lines=$(awk "BEGIN { print $total_lines + $n }")
done

rm -f ./*.gcov

if [ "$failed" -ne 0 ]; then
    exit 1
fi

overall=$(awk "BEGIN { print ($total_lines > 0) ? 100 * $total_exec / $total_lines : 0 }")
echo "coverage: src/common overall line coverage: $overall% ($total_exec/$total_lines lines; threshold ${THRESHOLD}%)"
awk "BEGIN { exit (!($overall >= $THRESHOLD)) }" || {
    echo "coverage: FAIL: below threshold ${THRESHOLD}%" >&2
    exit 1
}
echo "coverage: PASS: threshold ${THRESHOLD}% met"
