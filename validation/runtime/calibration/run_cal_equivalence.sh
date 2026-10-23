#!/usr/bin/env bash
# Build and run the P1-4 equivalence case: the synchronous entry point and the
# typed validate/bounds hooks must return the same metadata verdict for the same
# RawObservation/Calibration.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/runtime/calibration"
EXE="${TMPDIR:-/tmp}/acsd-cal-equivalence"
COMMON=(-std=c++20 -O2 -fno-fast-math -ffp-contract=off
        -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib")

lib_flag() {
  local name="$1"
  if [ -e "$BUILD/lib$name.a" ]; then printf -- '-L%s -l%s' "$BUILD" "$name"; return 0; fi
  if [ -e "$BUILD/libacsd_$name.a" ]; then printf -- '-L%s -lacsd_%s' "$BUILD" "$name"; return 0; fi
  return 1
}

LIBS=""
for name in observation runtime operators uncertainty numerics model; do
  if flag="$(lib_flag "$name")"; then LIBS+=" $flag"; fi
done

if printf '%s' "$LIBS" | grep -q -- "-lobservation"; then
  "$CXX" "${COMMON[@]}" "$OUT/cal_equivalence.cpp" $LIBS -lpthread -lzstd -o "$EXE"
else
  "$CXX" "${COMMON[@]}" \
    "$ROOT/src/model/array.cpp" "$ROOT/src/model/exact.cpp" "$ROOT/src/model/hash.cpp" \
    "$ROOT/src/model/units.cpp" "$ROOT/src/numerics/interval.cpp" \
    "$ROOT/src/operators/api.cpp" "$ROOT/src/operators/linear_action.cpp" \
    "$ROOT/src/observation/observation.cpp" "$ROOT/src/observation/calibration_kernel.cpp" \
    "$ROOT/src/observation/calibration_plan.cpp" \
    "$OUT/cal_equivalence.cpp" -lpthread -lzstd -o "$EXE"
fi
if [ $? -ne 0 ]; then
  echo "cal_equivalence: build failed" >&2
  exit 2
fi
"$EXE" > "$OUT/observed_equivalence.stdout.json" 2> "$OUT/observed_equivalence.stderr.txt"
status=$?
echo "cal_equivalence: exit=$status"
python3 - "$OUT/observed_equivalence.stdout.json" "$OUT/case_equivalence.json" <<'PY'
import json, sys
observed = json.load(open(sys.argv[1]))
case = json.load(open(sys.argv[2]))
names = [check["check"] for check in observed.get("checks", [])]
passes = {check["check"]: bool(check.get("pass")) for check in observed.get("checks", [])}
missing = [name for name in case["required_checks"] if name not in passes]
failing = [name for name in case["required_checks"] if name in passes and not passes[name]]
reasons = []
if missing:
    reasons.append("required checks not attempted: " + ", ".join(missing))
if failing:
    reasons.append("required checks failed: " + ", ".join(failing))
if observed.get("failures"):
    reasons.append("harness reports failures")
observed["command"] = "validation/runtime/calibration/run_cal_equivalence.sh"
observed["exit_code"] = 0
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
json.dump(observed, open(sys.argv[1].replace("observed_equivalence.stdout.json", "observed_equivalence.json"), "w"),
          ensure_ascii=False, indent=2)
json.dump(comparison, open(sys.argv[1].replace("observed_equivalence.stdout.json", "comparison_equivalence.json"), "w"),
          ensure_ascii=False, indent=2)
print("cal_equivalence:", comparison["status"], f'{comparison["successes"]}/{comparison["attempts"]}')
PY
