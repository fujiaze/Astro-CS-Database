#!/usr/bin/env bash
# Build and run the C-CAL-WRITE / C-CAL-CLEAN fault injection cases.
#
# Required archives come from $ACSD_BUILD (default /tmp/acsd-build).  The raw
# stdout is kept next to this script and the comparison JSON is written here.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/process/calibration-faults"
EXE="${TMPDIR:-/tmp}/acsd-cal-faults-run"

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
  echo "calibration_faults: pending, missing archives:$missing" >&2
  exit 3
fi

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" \
  "$OUT/calibration_faults.cpp" -L "$BUILD" $LIBS -lpthread -lzstd -o "$EXE"
if [ $? -ne 0 ]; then
  echo "calibration_faults: build failed" >&2
  exit 2
fi

(cd "$ROOT" && "$EXE" > "$OUT/observed_faults.stdout.json" 2>&1)
status=$?
echo "calibration_faults: exit=$status"

python3 - "$OUT/observed_faults.stdout.json" "$OUT/observed_faults.json" <<'PY'
import json
import sys

raw_path, output_path = sys.argv[1], sys.argv[2]
payload = None
checks = []
for line in open(raw_path, encoding="utf-8"):
    line = line.strip()
    if line.startswith("{"):
        payload = json.loads(line)
    elif line.startswith("PASS:") or line.startswith("FAIL:"):
        checks.append({"name": line.split(":", 1)[1].strip(), "pass": line.startswith("PASS:")})
if payload is None:
    raise SystemExit("no JSON payload in " + raw_path)
failed = [check["name"] for check in checks if not check["pass"]]
observed = {
    "case": "cal.faults",
    "command": "validation/process/calibration-faults/run_faults.sh",
    "checks": checks,
    "failures": failed,
    "cases": payload["cases"],
    "status": "pass" if not failed else "fail",
    "limitation": (
        "cleanup-only secondary issues cannot be injected through the public Storage API: "
        "private-spool removal is best effort and has no public failure switch"
    ),
}
with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(observed, handle, ensure_ascii=False, indent=2)
print("calibration_faults:", observed["status"], f"{len(checks) - len(failed)}/{len(checks)}")
raise SystemExit(0 if not failed else 1)
PY
status=$?
exit $status
