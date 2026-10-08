#!/usr/bin/env bash
# Builds the program and the unit test binaries, runs every unit test and the
# end-to-end suite, prints one PASS/FAIL line per test and exits non-zero on
# any failure. `make test` calls this and nothing else.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
UNIT_DIR="$REPO_DIR/build/test"
E2E_RUNNER="$SCRIPT_DIR/e2e/run_e2e.sh"

STATUS=0

build_program_and_unit_binaries() {
    make -s -C "$REPO_DIR" all unit-binaries
}

report() {
    local verdict=$1 name=$2
    echo "$verdict $name"
    [ "$verdict" = PASS ] || STATUS=1
}

run_one_unit_binary() {
    local binary=$1
    if "$binary" >/dev/null; then
        report PASS "unit/$(basename "$binary")"
    else
        report FAIL "unit/$(basename "$binary")"
    fi
}

run_every_unit_binary() {
    local binary
    for binary in "$UNIT_DIR"/test_*; do
        [ -x "$binary" ] && run_one_unit_binary "$binary"
    done
}

e2e_suite_exists() {
    [ -x "$E2E_RUNNER" ]
}

run_the_e2e_suite() {
    if e2e_suite_exists; then
        "$E2E_RUNNER" || STATUS=1
    fi
}

main() {
    build_program_and_unit_binaries
    run_every_unit_binary
    run_the_e2e_suite
    exit "$STATUS"
}

main "$@"
