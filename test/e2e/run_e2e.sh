#!/usr/bin/env bash
# Runs every test/e2e/cases/<name>.in through build/adventure under CLUE_SEED
# (from <name>.seed, default 1), diffs stdout+stderr against <name>.expected
# and checks the exit code (from <name>.exit, default 0); one PASS/FAIL line
# per case, non-zero exit when any case failed.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BINARY="$REPO_DIR/build/adventure"
CASES_DIR="$SCRIPT_DIR/cases"

DEFAULT_SEED=1
DEFAULT_EXIT=0
TIMEOUT_SECONDS=5
SCRATCH_DIR="$(mktemp -d "${TMPDIR:-/tmp}/clue-e2e.XXXXXX")"

STATUS=0

remove_the_scratch_dir_on_exit() {
    trap 'rm -rf "$SCRATCH_DIR"' EXIT
}

seed_for() {
    local name="$1"
    if [ -f "$CASES_DIR/$name.seed" ]; then
        cat "$CASES_DIR/$name.seed"
    else
        echo "$DEFAULT_SEED"
    fi
}

expected_exit_for() {
    local name="$1"
    if [ -f "$CASES_DIR/$name.exit" ]; then
        cat "$CASES_DIR/$name.exit"
    else
        echo "$DEFAULT_EXIT"
    fi
}

run_the_program_for() {
    local name="$1" seed="$2"
    CLUE_SEED="$seed" timeout "$TIMEOUT_SECONDS" "$BINARY" \
        <"$CASES_DIR/$name.in" >"$SCRATCH_DIR/$name.out" 2>&1
}

output_matches_expected() {
    local name="$1"
    diff -u "$CASES_DIR/$name.expected" "$SCRATCH_DIR/$name.out" >"$SCRATCH_DIR/$name.diff"
}

exit_code_matches() {
    local actual="$1" expected="$2"
    [ "$actual" -eq "$expected" ]
}

report_pass() {
    local name="$1"
    echo "PASS e2e/$name"
}

report_fail() {
    local name="$1" actual="$2" expected="$3"
    echo "FAIL e2e/$name (exit $actual, expected $expected)"
    cat "$SCRATCH_DIR/$name.diff"
    STATUS=1
}

run_one_case() {
    local name="$1"
    local seed expected_exit actual_exit=0
    seed="$(seed_for "$name")"
    expected_exit="$(expected_exit_for "$name")"
    run_the_program_for "$name" "$seed" || actual_exit="$?"
    if output_matches_expected "$name" && exit_code_matches "$actual_exit" "$expected_exit"; then
        report_pass "$name"
    else
        report_fail "$name" "$actual_exit" "$expected_exit"
    fi
}

run_every_case() {
    local input
    for input in "$CASES_DIR"/*.in; do
        run_one_case "$(basename "$input" .in)"
    done
}

exit_with_the_suite_status() {
    exit "$STATUS"
}

main() {
    remove_the_scratch_dir_on_exit
    run_every_case
    exit_with_the_suite_status
}

main "$@"
