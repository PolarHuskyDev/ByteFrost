#!/usr/bin/env bash
#
# test_compiled.sh — End-to-end test for ByteFrost compiled programs.
#
# For each .bf test file, this script:
#   1. Compiles the .bf source directly to an executable using byte_frost
#   2. Runs the executable and captures stdout
#   3. Compares stdout against the expected output
#
# Usage:
#   ./scripts/test_compiled.sh [-v|--verbose] [path/to/byte_frost]
#
# Options:
#   -v, --verbose   Show the stdout output of each test executable.
#
# If no path is given, defaults to build/Release/byte_frost.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VERBOSE=false
COMPILER=""

for arg in "$@"; do
    case "$arg" in
        -v|--verbose) VERBOSE=true ;;
        *) COMPILER="$arg" ;;
    esac
done

COMPILER="${COMPILER:-$PROJECT_DIR/build/Release/byte_frost}"
TESTS_DIR="$PROJECT_DIR/tests"
TMP_DIR=$(mktemp -d)

trap 'rm -rf "$TMP_DIR"' EXIT

PASS=0
FAIL=0
SKIP=0
TOTAL=0

# Colors (only if stdout is a terminal).
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    GREEN=''
    RED=''
    YELLOW=''
    NC=''
fi

# ---------------------------------------------------------------------------
# run_test NAME BF_FILE EXPECTED_OUTPUT [TIMEOUT]
#   Compile a .bf file and verify its stdout matches EXPECTED_OUTPUT.
# ---------------------------------------------------------------------------
run_test() {
    local name="$1"
    local bf_file="$2"
    local expected="$3"
    local timeout_secs="${4:-10}"

    TOTAL=$((TOTAL + 1))

    # Step 1: Compile .bf -> executable directly
    local exe_file="$TMP_DIR/${name}"
    if ! "$COMPILER" "$bf_file" -o "$exe_file" 2>"$TMP_DIR/${name}.compile_err"; then
        echo -e "  ${RED}FAIL${NC} $name — compilation failed"
        cat "$TMP_DIR/${name}.compile_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi

    # Step 2: Run the executable with a timeout.
    local actual
    if ! actual=$(timeout "$timeout_secs" "$exe_file" 2>"$TMP_DIR/${name}.run_err"); then
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}FAIL${NC} $name — timed out after ${timeout_secs}s"
        else
            echo -e "  ${RED}FAIL${NC} $name — runtime error (exit code $exit_code)"
            cat "$TMP_DIR/${name}.run_err" | sed 's/^/    /'
        fi
        FAIL=$((FAIL + 1))
        return
    fi

    # Show output if verbose.
    if [ "$VERBOSE" = true ]; then
        echo -e "  --- $name output ---"
        echo "$actual" | sed 's/^/    /'
        echo -e "  ---"
    fi

    # Step 4: Compare output.
    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} $name — output mismatch"
        echo "    Expected:"
        echo "$expected" | sed 's/^/      /'
        echo "    Actual:"
        echo "$actual" | sed 's/^/      /'
        FAIL=$((FAIL + 1))
    fi
}

skip_test() {
    local name="$1"
    local reason="$2"
    TOTAL=$((TOTAL + 1))
    SKIP=$((SKIP + 1))
    echo -e "  ${YELLOW}SKIP${NC} $name — $reason"
}

# ---------------------------------------------------------------------------
# run_negative_test NAME BF_FILE [EXPECTED_ERR_SUBSTRING]
#   Compile a .bf file and verify the compiler REJECTS it (non-zero exit).
#   Optionally, check that stderr contains EXPECTED_ERR_SUBSTRING.
# ---------------------------------------------------------------------------
run_negative_test() {
    local name="$1"
    local bf_file="$2"
    local expected_err="${3:-}"

    TOTAL=$((TOTAL + 1))

    local exe_file="$TMP_DIR/${name}"
    if "$COMPILER" "$bf_file" -o "$exe_file" 2>"$TMP_DIR/${name}.compile_err"; then
        echo -e "  ${RED}FAIL${NC} $name — expected compilation failure but it succeeded"
        FAIL=$((FAIL + 1))
        return
    fi

    if [ -n "$expected_err" ]; then
        if grep -qF "$expected_err" "$TMP_DIR/${name}.compile_err"; then
            echo -e "  ${GREEN}PASS${NC} $name (correctly rejected with expected error)"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} $name — compiler rejected but error message didn't match"
            echo "    Expected stderr to contain: $expected_err"
            echo "    Actual stderr:"
            cat "$TMP_DIR/${name}.compile_err" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC} $name (correctly rejected)"
        PASS=$((PASS + 1))
    fi
}

# ---------------------------------------------------------------------------
# run_orca_test NAME TOML_PATH EXPECTED_OUTPUT [TIMEOUT]
#   Build an orca project and verify its stdout matches EXPECTED_OUTPUT.
# ---------------------------------------------------------------------------
run_orca_test() {
    local name="$1"
    local toml_path="$2"
    local expected="$3"
    local timeout_secs="${4:-30}"

    TOTAL=$((TOTAL + 1))

    local exe_file="$TMP_DIR/${name}"
    local orca_bin
    orca_bin="$(dirname "$COMPILER")/orca"

    if ! "$orca_bin" --project "$toml_path" -o "$exe_file" \
            2>"$TMP_DIR/${name}.compile_err" 1>"$TMP_DIR/${name}.orca_out"; then
        echo -e "  ${RED}FAIL${NC} $name — orca build failed"
        cat "$TMP_DIR/${name}.compile_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi

    local actual
    if ! actual=$(timeout "$timeout_secs" "$exe_file" 2>"$TMP_DIR/${name}.run_err"); then
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}FAIL${NC} $name — timed out after ${timeout_secs}s"
        else
            echo -e "  ${RED}FAIL${NC} $name — runtime error (exit code $exit_code)"
            cat "$TMP_DIR/${name}.run_err" | sed 's/^/    /'
        fi
        FAIL=$((FAIL + 1))
        return
    fi

    if [ "$VERBOSE" = true ]; then
        echo -e "  --- $name output ---"
        echo "$actual" | sed 's/^/    /'
        echo -e "  ---"
    fi

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} $name — output mismatch"
        echo "    Expected:"
        echo "$expected" | sed 's/^/      /'
        echo "    Actual:"
        echo "$actual" | sed 's/^/      /'
        FAIL=$((FAIL + 1))
    fi
}

# ---------------------------------------------------------------------------
# run_orca_negative_test NAME TOML_PATH [EXPECTED_ERR_SUBSTRING]
#   Build an orca project and verify the build FAILS (non-zero exit).
#   Optionally, check that stderr contains EXPECTED_ERR_SUBSTRING.
# ---------------------------------------------------------------------------
run_orca_negative_test() {
    local name="$1"
    local toml_path="$2"
    local expected_err="${3:-}"

    TOTAL=$((TOTAL + 1))

    local orca_bin
    orca_bin="$(dirname "$COMPILER")/orca"

    if "$orca_bin" --project "$toml_path" \
            2>"$TMP_DIR/${name}.build_err" 1>/dev/null; then
        echo -e "  ${RED}FAIL${NC} $name — expected orca build failure but it succeeded"
        FAIL=$((FAIL + 1))
        return
    fi

    if [ -n "$expected_err" ]; then
        if grep -qF "$expected_err" "$TMP_DIR/${name}.build_err"; then
            echo -e "  ${GREEN}PASS${NC} $name (correctly rejected with expected error)"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} $name — orca rejected but error message didn't match"
            echo "    Expected stderr to contain: $expected_err"
            echo "    Actual stderr:"
            cat "$TMP_DIR/${name}.build_err" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC} $name (correctly rejected)"
        PASS=$((PASS + 1))
    fi
}

# ---------------------------------------------------------------------------
# run_orca_run_test NAME TOML_PATH EXPECTED_OUTPUT [TIMEOUT]
#   Run `orca run --project TOML_PATH -o TMP_EXE` and verify that the binary's
#   stdout (lines not prefixed with "[orca]") matches EXPECTED_OUTPUT.
# ---------------------------------------------------------------------------
run_orca_run_test() {
    local name="$1"
    local toml_path="$2"
    local expected="$3"
    local timeout_secs="${4:-30}"

    TOTAL=$((TOTAL + 1))

    local exe_file="$TMP_DIR/${name}_run"
    local orca_bin
    orca_bin="$(dirname "$COMPILER")/orca"

    local raw_output
    if ! raw_output=$(timeout "$timeout_secs" "$orca_bin" run \
            --project "$toml_path" -o "$exe_file" 2>/dev/null); then
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}FAIL${NC} $name — timed out after ${timeout_secs}s"
        else
            echo -e "  ${RED}FAIL${NC} $name — orca run failed (exit $exit_code)"
        fi
        FAIL=$((FAIL + 1))
        return
    fi

    # Filter out [orca] build-diagnostic lines; the remainder is the binary's output.
    local actual
    actual=$(echo "$raw_output" | grep -v '^\[orca\]' || true)

    if [ "$actual" = "$expected" ]; then
        echo -e "  ${GREEN}PASS${NC} $name"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} $name — output mismatch"
        echo "    Expected: $(echo "$expected" | head -3)"
        echo "    Actual:   $(echo "$actual" | head -3)"
        FAIL=$((FAIL + 1))
    fi
}

# ---------------------------------------------------------------------------
# run_orca_clean_test NAME TOML_PATH
#   Build the orca project, then clean it, and verify build/orca/ is removed.
# ---------------------------------------------------------------------------
run_orca_clean_test() {
    local name="$1"
    local toml_path="$2"

    TOTAL=$((TOTAL + 1))

    local orca_bin
    orca_bin="$(dirname "$COMPILER")/orca"
    local toml_dir
    toml_dir="$(dirname "$toml_path")"

    # Build first to create artifacts.
    if ! "$orca_bin" --project "$toml_path" -o "$TMP_DIR/${name}_clean_bin" \
            >/dev/null 2>&1; then
        echo -e "  ${RED}FAIL${NC} $name — setup build failed"
        FAIL=$((FAIL + 1))
        return
    fi

    local build_orca_dir="$toml_dir/build/orca"
    if [ ! -d "$build_orca_dir" ]; then
        echo -e "  ${RED}FAIL${NC} $name — build/orca not created by build step"
        FAIL=$((FAIL + 1))
        return
    fi

    # Now clean.
    if ! "$orca_bin" clean --project "$toml_path" >/dev/null 2>&1; then
        echo -e "  ${RED}FAIL${NC} $name — orca clean failed"
        FAIL=$((FAIL + 1))
        return
    fi

    if [ -d "$build_orca_dir" ]; then
        echo -e "  ${RED}FAIL${NC} $name — build/orca still exists after clean"
        FAIL=$((FAIL + 1))
        return
    fi

    echo -e "  ${GREEN}PASS${NC} $name"
    PASS=$((PASS + 1))
}

# ---------------------------------------------------------------------------
# run_orca_check_test NAME TOML_PATH EXPECT_PASS [EXPECTED_ERR_SUBSTRING]
#   Run `orca check --project TOML_PATH`.
#   If EXPECT_PASS is "pass", verify exit 0.  If "fail", verify exit non-zero.
# ---------------------------------------------------------------------------
run_orca_check_test() {
    local name="$1"
    local toml_path="$2"
    local expect_pass="$3"   # "pass" or "fail"
    local expected_err="${4:-}"

    TOTAL=$((TOTAL + 1))

    local orca_bin
    orca_bin="$(dirname "$COMPILER")/orca"

    local check_stderr="$TMP_DIR/${name}.check_err"
    if "$orca_bin" check --project "$toml_path" \
            >/dev/null 2>"$check_stderr"; then
        local succeeded=true
    else
        local succeeded=false
    fi

    if [ "$expect_pass" = "pass" ]; then
        if [ "$succeeded" = true ]; then
            echo -e "  ${GREEN}PASS${NC} $name"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} $name — orca check failed on valid project"
            cat "$check_stderr" | sed 's/^/    /'
            FAIL=$((FAIL + 1))
        fi
    else
        if [ "$succeeded" = false ]; then
            if [ -n "$expected_err" ] && ! grep -qF "$expected_err" "$check_stderr"; then
                echo -e "  ${RED}FAIL${NC} $name — check failed but wrong error message"
                cat "$check_stderr" | sed 's/^/    /'
                FAIL=$((FAIL + 1))
            else
                echo -e "  ${GREEN}PASS${NC} $name (correctly reported error)"
                PASS=$((PASS + 1))
            fi
        else
            echo -e "  ${RED}FAIL${NC} $name — orca check passed on invalid project"
            FAIL=$((FAIL + 1))
        fi
    fi
}


echo "Compiler: $COMPILER"
echo ""

# --- hello_world.bf ---
run_test "hello_world" "$TESTS_DIR/hello_world.bf" "Hello, World!"

# --- fib.bf ---
run_test "fib" "$TESTS_DIR/fib.bf" "$(printf '0\n1\n1\n2\n3\n5\n8\n13\n21\n34')"

# --- if_else.bf ---
run_test "if_else" "$TESTS_DIR/if_else.bf" "Odd"

# --- while_loop.bf ---
run_test "while_loop" "$TESTS_DIR/while_loop.bf" "$(printf '0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10')"

# --- match.bf ---
run_test "match" "$TESTS_DIR/match.bf" "High state matched"

# --- operators.bf ---
# a=3,b=2,c=8,d=5,e=1, comparisons, logical, bitwise (f=3,g=15,h=1,j=12,k=1),
# compound: 3+5=8, 8-3=5, 5*2=10, 10/4=2, 2%3=2, ++→3, --→2
OPERATORS_EXPECTED="$(printf '3\n2\n8\n5\n1\na == 3\na != 0\na < 5\na > 1\na <= 3\na >= 3\na > 0 && b > 0\na > 0 || b > 0\n!false\na > 0 ^^ b > 0 is false\n3\n15\n1\n-4\n12\n1\n8\n5\n10\n2\n2\n3\n2')"
run_test "operators" "$TESTS_DIR/operators.bf" "$OPERATORS_EXPECTED"

# --- break_continue.bf ---
# Prints 0,1, skips 2 (continue), prints 3-6, breaks at 7.
run_test "break_continue" "$TESTS_DIR/break_continue.bf" "$(printf '0\n1\n3\n4\n5\n6')"

# --- struct.bf ---
run_test "struct" "$TESTS_DIR/struct.bf" "Hi I am Peter, and I am 21 years old"

# --- constructor.bf ---
run_test "constructor" "$TESTS_DIR/constructor.bf" "Area: 22"

# --- containers.bf ---
CONTAINERS_EXPECTED="$(printf '[0, 1, 2, 3, 4, 5]\n[0, 1, 4, 9, 16, 25]\n{200: Ok, 201: Created, 404: Not Found}')"
run_test "containers" "$TESTS_DIR/containers.bf" "$CONTAINERS_EXPECTED"

# --- composition.bf ---
run_test "composition" "$TESTS_DIR/composition.bf" "Circle center: (10, 20), radius: 5.5"

# --- trig_function.bf (negative: must fail — conflicts with stdlib without 'overridden') ---
echo ""
echo "--- Negative Tests ---"
run_negative_test "trig_function_no_override" \
    "$TESTS_DIR/trig_function.bf" \
    "conflicts with a stdlib math function"

# --- trig_function_overridden.bf (positive: user-defined math with 'overridden') ---
echo ""
echo "--- Stdlib Override Tests ---"
TRIG_EXPECTED="$(printf '1\n-1\n1')"
run_test "trig_function_overridden" "$TESTS_DIR/trig_function_overridden.bf" "$TRIG_EXPECTED"

# --- module_example orca project (cross-module imports/exports) ---
echo ""
echo "--- Orca Multi-Module Tests ---"
MODULE_EXPECTED="$(printf '25\n12\n12')"
run_orca_test "module_example" "$TESTS_DIR/module_example/orca.toml" "$MODULE_EXPECTED"

# --- overridden_import_alias: import abs as myAbs from math.utils ---
run_orca_test "overridden_import_alias" \
    "$TESTS_DIR/overridden_import_alias/orca.toml" \
    "$(printf '7\n3\n42')"

# --- overridden_import_qualified: import math.utils; utils.abs() ---
run_orca_test "overridden_import_qualified" \
    "$TESTS_DIR/overridden_import_qualified/orca.toml" \
    "$(printf '7\n3\n42')"

# --- overridden_import_direct_negative: import abs from math.utils (no alias → must fail) ---
run_orca_negative_test "overridden_import_direct_neg" \
    "$TESTS_DIR/overridden_import_direct_negative/orca.toml" \
    "conflicts with"

# --- null_safety: nullable struct semantics ---
# Tests: default null, explicit null, constructor init, reassign to null, struct-literal init.
# No negative (null-dereference) test here: that would be a runtime segfault, not a compile
# error. Compile-time null-dereference detection is deferred to Phase 7 (semantic analysis).
echo ""
echo "--- Nullable Struct Tests ---"
NULL_SAFETY_EXPECTED="$(printf 'PASS: r is null by default\nPASS: r is null via explicit assignment\nPASS: r is not null, area = 12\nPASS: r is null after reassignment\nPASS: struct init, area = 30')"
run_orca_test "null_safety" "$TESTS_DIR/null_safety/orca.toml" "$NULL_SAFETY_EXPECTED"

# --- orca subcommand tests: run / clean / check ---
echo ""
echo "--- Orca Subcommand Tests (run / clean / check) ---"

# orca run: build then execute module_example, verifying binary output.
run_orca_run_test "orca_run_module_example" \
    "$TESTS_DIR/module_example/orca.toml" \
    "$(printf '25\n12\n12')"

# orca clean: build/orca/ is removed and does not re-appear.
run_orca_clean_test "orca_clean_module_example" \
    "$TESTS_DIR/module_example/orca.toml"

# orca check: valid project reports no errors.
run_orca_check_test "orca_check_valid" \
    "$TESTS_DIR/module_example/orca.toml" pass

# orca check: project with a syntax error is rejected.
mkdir -p "$TMP_DIR/check_bad/src"
cat > "$TMP_DIR/check_bad/orca.toml" << 'TOML'
[project]
name    = "check_bad"
version = "0.1.0"
entry   = "src/main.bf"
TOML
printf 'main(): int { x: int = ; return 0; }\n' > "$TMP_DIR/check_bad/src/main.bf"
run_orca_check_test "orca_check_invalid" \
    "$TMP_DIR/check_bad/orca.toml" fail "error:"

echo ""
echo "=== Results ==="
echo -e "  ${GREEN}Passed: $PASS${NC}"
if [ $FAIL -gt 0 ]; then
    echo -e "  ${RED}Failed: $FAIL${NC}"
else
    echo "  Failed: 0"
fi
if [ $SKIP -gt 0 ]; then
    echo -e "  ${YELLOW}Skipped: $SKIP${NC}"
fi
echo "  Total:  $TOTAL"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
