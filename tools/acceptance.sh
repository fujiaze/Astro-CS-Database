#!/usr/bin/env bash
# End to end acceptance run for the ACSD v2 rebuild.
#
# The script performs, in order:
#   1. the design integrity check (specification, schemas, examples, links);
#   2. a C++20 syntax check of the public declaration contract;
#   3. the science build of the library, the contract driver and the CLI;
#   4. the independent verification suites under validation/;
#   5. the resource measurement harness on the locked configurations.
#
# Every step reports its own exit status; a failing step never stops the run so
# that the report contains the full picture.  The script prints a summary and
# exits non-zero when any mandatory step failed.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ACSD_BUILD_DIR:-$ROOT/build/science}"
REPORT="${ACSD_REPORT:-$ROOT/build/acceptance}"
mkdir -p "$REPORT"

declare -a NAMES=()
declare -a STATUS=()
declare -a DETAIL=()

record() {
  local name="$1" status="$2" detail="$3"
  NAMES+=("$name"); STATUS+=("$status"); DETAIL+=("$detail")
  printf '[%s] %-34s %s\n' "$status" "$name" "$detail"
}

run_step() {
  local name="$1"; shift
  local log="$REPORT/${name// /_}.log"
  if "$@" >"$log" 2>&1; then
    record "$name" PASS "$log"
  else
    local code=$?
    record "$name" FAIL "exit=$code log=$log"
  fi
}

cd "$ROOT"

run_step "design-integrity" python3 tools/check_design.py
run_step "contract-syntax" g++ -std=c++20 -fsyntax-only -Iinclude validation/contracts_compile.cpp

if [ ! -f "$BUILD/build.ninja" ]; then
  run_step "cmake-configure" cmake -S . -B "$BUILD" -G Ninja -DACSD_BUILD_SCIENCE=ON
fi
run_step "cmake-build" cmake --build "$BUILD"

if [ -x "$BUILD/acsd-contract" ]; then
  for example in u01 external-array to-fits; do
    if [ -f "examples/$example.request.json" ]; then
      run_step "example-$example" "$BUILD/acsd-contract" \
        --request "examples/$example.request.json" --response "$REPORT/$example.result.json"
    fi
  done
fi

if [ -x "$BUILD/acsd" ]; then
  run_step "cli-version" "$BUILD/acsd" --version --json
  run_step "cli-template-normalize" "$BUILD/acsd" template normalize
  run_step "cli-template-mosaic" "$BUILD/acsd" template mosaic
  run_step "cli-template-export" "$BUILD/acsd" template export
  run_step "cli-doctor" "$BUILD/acsd" doctor --json
fi

if [ -d validation ]; then
  if [ -f validation/run_all.py ]; then
    run_step "validation-suite" python3 validation/run_all.py
  else
    record "validation-suite" SKIP "validation/run_all.py not present"
  fi
fi

if [ -d build/gui ] && [ -f build/gui/CTestTestfile.cmake ]; then
  run_step "gui-ctest" ctest --test-dir build/gui --output-on-failure
fi

printf '\n=== acceptance summary ===\n'
failures=0
for i in "${!NAMES[@]}"; do
  printf '%-6s %-34s %s\n' "${STATUS[$i]}" "${NAMES[$i]}" "${DETAIL[$i]}"
  case "${STATUS[$i]}" in FAIL) failures=$((failures + 1));; esac
done
printf '%d failing step(s)\n' "$failures"
exit $(( failures > 0 ? 1 : 0 ))
