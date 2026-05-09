#!/usr/bin/env bash
#
# Data-driven e2e test runner for ByteFrost compiler and Orca tooling.
#
# Discovers all metadata.json files under tests/e2e/** and executes each case
# according to its kind and target.
#
# Test structure:
#   tests/e2e/compiler/<test_id>/
#     metadata.json
#     src/main.bf (required)
#     stdout.txt (optional - validates stdout)
#     stderr.txt (optional - validates stderr)
#     stdin.txt (optional - provides stdin)
#
#   tests/e2e/orca/<test_id>/
#     metadata.json
#     orca.toml (required)
#     src/ (project sources)
#     stdout.txt (optional)
#     stderr.txt (optional)
#     stdin.txt (optional)
#
# Usage:
#   ./scripts/test_compiled.sh [-v|--verbose] [path/to/byte_frost]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
E2E_DIR="$PROJECT_DIR/tests/e2e"
VERBOSE=false
COMPILER=""

for arg in "$@"; do
    case "$arg" in
        -v|--verbose) VERBOSE=true ;;
        *) COMPILER="$arg" ;;
    esac
done

COMPILER="${COMPILER:-$PROJECT_DIR/build/Release/byte_frost}"
ORCA_BIN="$(dirname "$COMPILER")/orca"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

PASS=0
FAIL=0
SKIP=0
TOTAL=0
TOTAL_ELAPSED=0

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

PYTHON_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
else
    echo -e "${RED}FAIL${NC} missing python (required to parse metadata.json)"
    exit 1
fi

get_time_ns() {
    date +%s%N
}

format_duration() {
    local ns=$1
    local ms=$((ns / 1000000))
    if [ $ms -lt 1000 ]; then
        echo "${ms}ms"
    else
        local seconds=$((ms / 1000))
        local remainder=$((ms % 1000))
        echo "${seconds}.$(printf '%03d' $remainder)s"
    fi
}

parse_json() {
    local file="$1"
    "$PYTHON_BIN" - "$file" <<'PY'
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as f:
    data = json.load(f)
id_val = data.get('id', '')
target_val = data.get('target', '')
kind_val = data.get('kind', '')
description_val = data.get('description', '')
timeout_val = data.get('timeoutSeconds', '10')
print(f"{id_val}|{target_val}|{kind_val}|{description_val}|{timeout_val}")
PY
}

normalize_output() {
    tr -d '\r'
}

read_optional_file() {
    local path="$1"
    if [ ! -f "$path" ]; then
        echo ""
        return
    fi
    cat "$path"
}

run_test() {
    local case_dir="$1" metadata_file="$2"
    local id kind description target timeout
    local test_start test_end elapsed_ns
    local json_fields
    
    # Parse JSON once and extract all fields in a single Python call
    json_fields="$(parse_json "$metadata_file")"
    IFS='|' read -r id target kind description timeout <<< "$json_fields"
    
    if [ -n "$description" ]; then
        echo "[$kind] $id - $description"
    else
        echo "[$kind] $id"
    fi
    
    test_start=$(get_time_ns)
    
    case "$target" in
        compiler) run_compiler_test "$case_dir" "$id" "$kind" "$timeout" ;;
        orca) run_orca_test "$case_dir" "$id" "$kind" "$timeout" ;;
        *)
            echo -e "  ${YELLOW}SKIP${NC} unknown target '$target'"
            SKIP=$((SKIP + 1))
            test_end=$(get_time_ns)
            elapsed_ns=$((test_end - test_start))
            TOTAL_ELAPSED=$((TOTAL_ELAPSED + elapsed_ns))
            return
            ;;
    esac
    
    test_end=$(get_time_ns)
    elapsed_ns=$((test_end - test_start))
    TOTAL_ELAPSED=$((TOTAL_ELAPSED + elapsed_ns))
    printf "    (%.0fms)\n" $((elapsed_ns / 1000000))
}

run_compiler_test() {
    local case_dir="$1" id="$2" kind="$3" timeout="$4"
    local src_file="$case_dir/src/main.bf"
    local expected_stdout="$case_dir/stdout.txt"
    local expected_stderr="$case_dir/stderr.txt"
    local stdin_file="$case_dir/stdin.txt"
    local exe_file="$TMP_DIR/${id}.exe"
    
    # Verify src/main.bf exists
    if [ ! -f "$src_file" ]; then
        echo -e "  ${RED}FAIL${NC} src/main.bf not found"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Compile
    if ! "$COMPILER" "$src_file" -o "$exe_file" 2>"$TMP_DIR/${id}.compile_err"; then
        if [ "$kind" = "compiler-negative" ]; then
            # Expected to fail - check for expected error if present
            if [ -f "$expected_stderr" ]; then
                expected_err=$(cat "$expected_stderr")
                actual_err=$(cat "$TMP_DIR/${id}.compile_err")
                if grep -qF "$expected_err" "$TMP_DIR/${id}.compile_err"; then
                    echo -e "  ${GREEN}PASS${NC} (correctly rejected)"
                    PASS=$((PASS + 1))
                else
                    echo -e "  ${RED}FAIL${NC} error mismatch"
                    echo "    Expected: $expected_err"
                    echo "    Got:"
                    cat "$TMP_DIR/${id}.compile_err" | sed 's/^/      /'
                    FAIL=$((FAIL + 1))
                fi
            else
                echo -e "  ${GREEN}PASS${NC} (correctly rejected)"
                PASS=$((PASS + 1))
            fi
        else
            echo -e "  ${RED}FAIL${NC} compilation failed"
            cat "$TMP_DIR/${id}.compile_err" | sed 's/^/    /'
            FAIL=$((FAIL + 1))
        fi
        return
    fi
    
    # For negative tests, failing to compile is already a pass
    if [ "$kind" = "compiler-negative" ]; then
        echo -e "  ${RED}FAIL${NC} expected compilation failure but succeeded"
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Run executable
    local actual exit_code
    if [ -f "$stdin_file" ]; then
        actual=$(cat "$stdin_file" | timeout "$timeout" "$exe_file" 2>"$TMP_DIR/${id}.run_err") || exit_code=$?
    else
        actual=$(timeout "$timeout" "$exe_file" 2>"$TMP_DIR/${id}.run_err") || exit_code=$?
    fi
    exit_code=${exit_code:-0}
    
    if [ $exit_code -eq 124 ]; then
        echo -e "  ${RED}FAIL${NC} timed out after ${timeout}s"
        FAIL=$((FAIL + 1))
        return
    elif [ $exit_code -ne 0 ]; then
        echo -e "  ${RED}FAIL${NC} runtime error (exit code $exit_code)"
        cat "$TMP_DIR/${id}.run_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi
    
    # Validate stdout if expected file exists
    if [ -f "$expected_stdout" ]; then
        actual=$(printf '%s' "$actual" | normalize_output)
        expected=$(cat "$expected_stdout" | normalize_output)
        
        if [ "$actual" = "$expected" ]; then
            echo -e "  ${GREEN}PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} output mismatch"
            echo "    Expected:"
            echo "$expected" | sed 's/^/      /'
            echo "    Actual:"
            echo "$actual" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        # No stdout validation - just check it ran
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    fi
}

run_orca_test() {
    local case_dir="$1" id="$2" kind="$3" timeout="$4"
    local project_file="$case_dir/orca.toml"
    local expected_stdout="$case_dir/stdout.txt"
    local expected_stderr="$case_dir/stderr.txt"
    local stdin_file="$case_dir/stdin.txt"
    local exe_file="$TMP_DIR/${id}.exe"
    
    # Verify orca.toml exists
    if [ ! -f "$project_file" ]; then
        echo -e "  ${RED}FAIL${NC} orca.toml not found"
        FAIL=$((FAIL + 1))
        return
    fi
    
    case "$kind" in
        orca-positive) run_orca_positive "$case_dir" "$id" "$timeout" "$exe_file" "$expected_stdout" ;;
        orca-negative) run_orca_negative "$case_dir" "$id" "$expected_stderr" ;;
        orca-run) run_orca_run "$case_dir" "$id" "$timeout" "$exe_file" "$expected_stdout" ;;
        orca-clean) run_orca_clean "$case_dir" "$id" ;;
        orca-check-pass) run_orca_check_pass "$case_dir" "$id" ;;
        orca-check-fail) run_orca_check_fail "$case_dir" "$id" "$expected_stderr" ;;
        orca-smoke) run_orca_smoke "$case_dir" "$id" "$timeout" "$stdin_file" ;;
        *)
            echo -e "  ${YELLOW}SKIP${NC} unknown orca kind '$kind'"
            SKIP=$((SKIP + 1))
            ;;
    esac
}

run_orca_positive() {
    local case_dir="$1" id="$2" timeout="$3" exe_file="$4" expected_stdout="$5"
    local project_file="$case_dir/orca.toml"
    
    if ! "$ORCA_BIN" --project "$project_file" -o "$exe_file" 2>"$TMP_DIR/${id}.build_err" >/dev/null; then
        echo -e "  ${RED}FAIL${NC} orca build failed"
        cat "$TMP_DIR/${id}.build_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi
    
    if ! actual=$(timeout "$timeout" "$exe_file" 2>"$TMP_DIR/${id}.run_err"); then
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}FAIL${NC} timed out after ${timeout}s"
        else
            echo -e "  ${RED}FAIL${NC} runtime error (exit code $exit_code)"
            cat "$TMP_DIR/${id}.run_err" | sed 's/^/    /'
        fi
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ -f "$expected_stdout" ]; then
        actual=$(printf '%s' "$actual" | normalize_output)
        expected=$(cat "$expected_stdout" | normalize_output)
        
        if [ "$actual" = "$expected" ]; then
            echo -e "  ${GREEN}PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} output mismatch"
            echo "    Expected:"
            echo "$expected" | sed 's/^/      /'
            echo "    Actual:"
            echo "$actual" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    fi
}

run_orca_negative() {
    local case_dir="$1" id="$2" expected_stderr="$3"
    local project_file="$case_dir/orca.toml"
    
    if "$ORCA_BIN" --project "$project_file" 2>"$TMP_DIR/${id}.build_err" >/dev/null; then
        echo -e "  ${RED}FAIL${NC} expected orca build failure but succeeded"
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ -f "$expected_stderr" ]; then
        expected_err=$(cat "$expected_stderr")
        if grep -qF "$expected_err" "$TMP_DIR/${id}.build_err"; then
            echo -e "  ${GREEN}PASS${NC} (correctly rejected)"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} error mismatch"
            echo "    Expected: $expected_err"
            cat "$TMP_DIR/${id}.build_err" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC} (correctly rejected)"
        PASS=$((PASS + 1))
    fi
}

run_orca_run() {
    local case_dir="$1" id="$2" timeout="$3" exe_file="$4" expected_stdout="$5"
    local project_file="$case_dir/orca.toml"
    
    if ! raw=$(timeout "$timeout" "$ORCA_BIN" run --project "$project_file" -o "$exe_file" 2>"$TMP_DIR/${id}.run_err"); then
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "  ${RED}FAIL${NC} timed out after ${timeout}s"
        else
            echo -e "  ${RED}FAIL${NC} orca run failed (exit code $exit_code)"
            cat "$TMP_DIR/${id}.run_err" | sed 's/^/    /'
        fi
        FAIL=$((FAIL + 1))
        return
    fi
    
    actual=$(printf '%s' "$raw" | grep -v '^\[orca\]' | normalize_output || true)
    
    if [ -f "$expected_stdout" ]; then
        expected=$(cat "$expected_stdout" | normalize_output)
        if [ "$actual" = "$expected" ]; then
            echo -e "  ${GREEN}PASS${NC}"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} output mismatch"
            echo "    Expected:"
            echo "$expected" | sed 's/^/      /'
            echo "    Actual:"
            echo "$actual" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    fi
}

run_orca_clean() {
    local case_dir="$1" id="$2"
    local project_file="$case_dir/orca.toml"
    local project_dir="$(dirname "$project_file")"
    local build_orca_dir="$project_dir/build/orca"
    
    if ! "$ORCA_BIN" --project "$project_file" -o "$TMP_DIR/${id}.clean.exe" >/dev/null 2>"$TMP_DIR/${id}.build_err"; then
        echo -e "  ${RED}FAIL${NC} setup build failed"
        cat "$TMP_DIR/${id}.build_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ ! -d "$build_orca_dir" ]; then
        echo -e "  ${RED}FAIL${NC} build/orca not created"
        FAIL=$((FAIL + 1))
        return
    fi
    
    if ! "$ORCA_BIN" clean --project "$project_file" >/dev/null 2>"$TMP_DIR/${id}.clean_err"; then
        echo -e "  ${RED}FAIL${NC} orca clean failed"
        cat "$TMP_DIR/${id}.clean_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ -d "$build_orca_dir" ]; then
        echo -e "  ${RED}FAIL${NC} build/orca still exists after clean"
        FAIL=$((FAIL + 1))
        return
    fi
    
    echo -e "  ${GREEN}PASS${NC}"
    PASS=$((PASS + 1))
}

run_orca_check_pass() {
    local case_dir="$1" id="$2"
    local project_file="$case_dir/orca.toml"
    
    if "$ORCA_BIN" check --project "$project_file" >/dev/null 2>"$TMP_DIR/${id}.check_err"; then
        echo -e "  ${GREEN}PASS${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "  ${RED}FAIL${NC} orca check failed"
        cat "$TMP_DIR/${id}.check_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
}

run_orca_check_fail() {
    local case_dir="$1" id="$2" expected_stderr="$3"
    local project_file="$case_dir/orca.toml"
    
    if "$ORCA_BIN" check --project "$project_file" >/dev/null 2>"$TMP_DIR/${id}.check_err"; then
        echo -e "  ${RED}FAIL${NC} orca check passed but should fail"
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ -f "$expected_stderr" ]; then
        expected_err=$(cat "$expected_stderr")
        if grep -qF "$expected_err" "$TMP_DIR/${id}.check_err"; then
            echo -e "  ${GREEN}PASS${NC} (correctly reported error)"
            PASS=$((PASS + 1))
        else
            echo -e "  ${RED}FAIL${NC} check failed but error did not match"
            cat "$TMP_DIR/${id}.check_err" | sed 's/^/      /'
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  ${GREEN}PASS${NC} (correctly reported error)"
        PASS=$((PASS + 1))
    fi
}

run_orca_smoke() {
    local case_dir="$1" id="$2" timeout="$3" stdin_file="$4"
    local project_file="$case_dir/orca.toml"
    local exe_file="$TMP_DIR/${id}.smoke.exe"
    
    if ! "$ORCA_BIN" --project "$project_file" -o "$exe_file" >/dev/null 2>"$TMP_DIR/${id}.build_err"; then
        echo -e "  ${RED}FAIL${NC} orca build failed"
        cat "$TMP_DIR/${id}.build_err" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
        return
    fi
    
    if [ -f "$stdin_file" ]; then
        if ! cat "$stdin_file" | timeout "$timeout" "$exe_file" >"$TMP_DIR/${id}.out" 2>"$TMP_DIR/${id}.run_err"; then
            exit_code=$?
            if [ $exit_code -eq 124 ]; then
                echo -e "  ${RED}FAIL${NC} timed out after ${timeout}s"
            else
                echo -e "  ${RED}FAIL${NC} runtime error (exit code $exit_code)"
                cat "$TMP_DIR/${id}.run_err" | sed 's/^/    /'
            fi
            FAIL=$((FAIL + 1))
            return
        fi
    else
        if ! timeout "$timeout" "$exe_file" >"$TMP_DIR/${id}.out" 2>"$TMP_DIR/${id}.run_err"; then
            exit_code=$?
            if [ $exit_code -eq 124 ]; then
                echo -e "  ${RED}FAIL${NC} timed out after ${timeout}s"
            else
                echo -e "  ${RED}FAIL${NC} runtime error (exit code $exit_code)"
                cat "$TMP_DIR/${id}.run_err" | sed 's/^/    /'
            fi
            FAIL=$((FAIL + 1))
            return
        fi
    fi
    
    echo -e "  ${GREEN}PASS${NC}"
    PASS=$((PASS + 1))
}

echo "Compiler: $COMPILER"
echo "E2E Root: $E2E_DIR"
echo ""

if [ ! -x "$COMPILER" ]; then
    echo -e "${RED}FAIL${NC} compiler executable not found or not executable: $COMPILER"
    exit 1
fi

if [ ! -x "$ORCA_BIN" ]; then
    echo -e "${RED}FAIL${NC} orca executable not found or not executable: $ORCA_BIN"
    exit 1
fi

mapfile -t METADATA_FILES < <(find "$E2E_DIR" -type f -name metadata.json | sort)
if [ ${#METADATA_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}SKIP${NC} no e2e cases discovered under $E2E_DIR"
    exit 0
fi

for metadata_file in "${METADATA_FILES[@]}"; do
    TOTAL=$((TOTAL + 1))
    case_dir="$(dirname "$metadata_file")"
    run_test "$case_dir" "$metadata_file"
done

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
echo ""
echo "  Elapsed: $(format_duration $TOTAL_ELAPSED)"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
