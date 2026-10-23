#!/usr/bin/env bash
# Build and run the coadd.integrate evidence case against $ACSD_BUILD.
#
# The case uses the module's own headers and the runtime bridge; it links the
# coadd science archive and the runtime/operators/uncertainty/model libs.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/process/coadd.integrate"
EXE="${TMPDIR:-/tmp}/acsd-coadd-integrate-evidence"

missing=""
LIBS="-Wl,--start-group"
for name in coadd uncertainty operators numerics model runtime codec; do
  if [ -e "$BUILD/lib$name.a" ]; then
    LIBS+=" -l$name"
  else
    missing+=" $name"
  fi
done
LIBS+=" -Wl,--end-group"

if [ -n "$missing" ]; then
  echo "coadd_integrate: pending, missing archives:$missing" >&2
  exit 3
fi

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib" \
  "$OUT/coadd_integrate_evidence.cpp" -L "$BUILD" $LIBS -lpthread -lzstd -o "$EXE"
if [ $? -ne 0 ]; then
  echo "coadd_integrate: build failed" >&2
  exit 2
fi
(cd "$ROOT" && "$EXE" > "$OUT/observed.stdout.json" 2> "$OUT/observed.stderr.txt")
status=$?
echo "coadd_integrate: exit=$status"
python3 - "$OUT/observed.stdout.json" "$OUT/case.json" <<'PY'
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
observed["command"] = "validation/process/coadd.integrate/run_validation.sh"
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
json.dump(observed, open(sys.argv[1].replace("observed.stdout.json", "observed.json"), "w"),
          ensure_ascii=False, indent=2)
json.dump(comparison, open(sys.argv[1].replace("observed.stdout.json", "comparison.json"), "w"),
          ensure_ascii=False, indent=2)
print("coadd_integrate:", comparison["status"], f'{comparison["successes"]}/{comparison["attempts"]}')
PY
