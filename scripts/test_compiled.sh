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

echo "=== ByteFrost Compiled Tests ==="
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
