#!/usr/bin/env bash
# Build and run the C-CAL kernel/plan case of observation.calibrate.
#
# Two modes:
#   * $ACSD_BUILD (default /tmp/acsd-build) contains the science archives:
#     link them directly;
#   * no archive is available yet: compile the small source subset needed by the
#     kernel/plan test from the repository (the same translation units CMake
#     compiles into libobservation).
#
# The program only emits raw JSON evidence; the frozen criteria are case.json.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/runtime/calibration"
EXE="${TMPDIR:-/tmp}/acsd-cal-kernel"
COMMON=(-std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse
        -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib")

lib_flag() {
  local name="$1"
  if [ -e "$BUILD/lib$name.a" ]; then printf -- '-L%s -l%s' "$BUILD" "$name"; return 0; fi
  if [ -e "$BUILD/libacsd_$name.a" ]; then printf -- '-L%s -lacsd_%s' "$BUILD" "$name"; return 0; fi
  return 1
}

LIBS=""
for name in observation runtime operators uncertainty numerics model storage; do
  if flag="$(lib_flag "$name")"; then LIBS+=" $flag"; fi
done

if printf '%s' "$LIBS" | grep -q -- "-lobservation"; then
  "$CXX" "${COMMON[@]}" "$OUT/cal_kernel.cpp" $LIBS -lpthread -lzstd -o "$EXE"
else
  "$CXX" "${COMMON[@]}" \
    "$ROOT/src/model/array.cpp" "$ROOT/src/model/exact.cpp" "$ROOT/src/model/hash.cpp" \
    "$ROOT/src/model/units.cpp" "$ROOT/src/numerics/interval.cpp" \
    "$ROOT/src/operators/api.cpp" "$ROOT/src/operators/linear_action.cpp" \
    "$ROOT/src/observation/observation.cpp" \
    "$ROOT/src/observation/calibration_kernel.cpp" "$ROOT/src/observation/calibration_plan.cpp" \
    "$OUT/cal_kernel.cpp" -lpthread -lzstd -o "$EXE"
fi
if [ $? -ne 0 ]; then
  echo "cal_kernel: build failed" >&2
  exit 2
fi
"$EXE" > "$OUT/observed.stdout.json" 2> "$OUT/observed.stderr.txt"
status=$?
echo "cal_kernel: exit=$status"
python3 - "$OUT/observed.stdout.json" "$OUT/case.json" <<'PY'
import json, sys
stdout, criteria = sys.argv[1], sys.argv[2]
observed = json.load(open(stdout))
case = json.load(open(criteria))
names = [check["check"] for check in observed.get("checks", [])]
passes = {check["check"]: bool(check.get("pass")) for check in observed.get("checks", [])}
missing = [name for name in case["required_checks"] if name not in passes]
failing = [name for name in case["required_checks"] if name in passes and not passes[name]]
reasons = []
if missing:
    reasons.append("required checks not attempted: " + ", ".join(missing))
if failing:
    reasons.append("required checks failed: " + ", ".join(failing))
if observed.get("attempts", 0) < case.get("min_attempts", 0):
    reasons.append("attempts below min_attempts")
if observed.get("failures"):
    reasons.append("harness reports failures")
comparison = {
    "case": case["case"],
    "status": "pass" if not reasons else "fail",
    "reason": "; ".join(reasons) if reasons else "all required checks passed",
    "attempts": observed.get("attempts", 0),
    "successes": observed.get("successes", 0),
    "expected_success_count": case.get("expected_success_count"),
    "observed_success_count": sum(1 for value in passes.values() if value),
    "required_checks": len(case["required_checks"]),
    "missing_checks": missing,
    "failed_checks": failing,
    "failures": observed.get("failures", []),
    "checks": observed.get("checks", []),
    "values": observed.get("values", {}),
    "resources": observed.get("resources", {}),
}
observed["command"] = "validation/runtime/calibration/run_cal_kernel.sh"
observed["exit_code"] = 0
observed["stderr_tail"] = ""
observed["timed_out"] = False
json.dump(observed, open(sys.argv[1].replace("observed.stdout.json", "observed.json"), "w"),
          ensure_ascii=False, indent=2)
json.dump(comparison, open(sys.argv[1].replace("observed.stdout.json", "comparison.json"), "w"),
          ensure_ascii=False, indent=2)
print("cal_kernel:", comparison["status"], f'{comparison["successes"]}/{comparison["attempts"]}')
PY
