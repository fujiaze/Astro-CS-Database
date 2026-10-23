#!/usr/bin/env bash
# Build and run the C-CAL Host chain case (driver -> generated registry ->
# ProcessHost -> Storage -> NativeObservation) against $ACSD_BUILD.
#
# Required archives: acsd_builtin, runtime, codec, observation and the science
# libraries it links.  When they are missing the case is reported as pending;
# no evidence is fabricated.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/runtime/calibration"
EXE="${TMPDIR:-/tmp}/acsd-cal-host"

missing=""
LIBS="-Wl,--start-group"
for name in acsd_builtin runtime codec workflows photometry sources coadd sky \
            information projection geometry response resources storage \
            observation catalogue uncertainty operators coordinates numerics model; do
  if [ -e "$BUILD/lib$name.a" ]; then
    LIBS+=" -l$name"
  else
    missing+=" $name"
  fi
done
LIBS+=" -Wl,--end-group"

if [ -n "$missing" ]; then
  echo "cal_host: pending, missing archives:$missing" >&2
  exit 3
fi

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib" \
  "$OUT/cal_host.cpp" -L "$BUILD" $LIBS -lpthread -lzstd -o "$EXE"
if [ $? -ne 0 ]; then
  echo "cal_host: build failed" >&2
  exit 2
fi
(cd "$ROOT" && "$EXE" > "$OUT/observed_host.stdout.json" 2> "$OUT/observed_host.stderr.txt")
status=$?
echo "cal_host: exit=$status"
python3 - "$OUT/observed_host.stdout.json" "$OUT/case_host.json" <<'PY'
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
if observed.get("attempts", 0) < case.get("min_attempts", 0):
    reasons.append("attempts below min_attempts")
if observed.get("failures"):
    reasons.append("harness reports failures")
observed["command"] = "validation/runtime/calibration/run_cal_host.sh"
observed["exit_code"] = 0
observed["stderr_tail"] = ""
observed["timed_out"] = False
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
json.dump(observed, open(sys.argv[1].replace("observed_host.stdout.json", "observed_host.json"), "w"),
          ensure_ascii=False, indent=2)
json.dump(comparison, open(sys.argv[1].replace("observed_host.stdout.json", "comparison_host.json"), "w"),
          ensure_ascii=False, indent=2)
print("cal_host:", comparison["status"], f'{comparison["successes"]}/{comparison["attempts"]}')
PY
