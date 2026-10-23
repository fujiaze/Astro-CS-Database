#!/usr/bin/env bash
# Build and run the P1-7 bounded ready window / P2 control ceiling evidence.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/process/calibration-faults"
EXE="${TMPDIR:-/tmp}/acsd-window-ceiling-run"

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
  echo "window_ceiling: pending, missing archives:$missing" >&2
  exit 3
fi

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" \
  "$OUT/window_ceiling.cpp" -L "$BUILD" $LIBS -lpthread -lzstd -o "$EXE"
if [ $? -ne 0 ]; then
  echo "window_ceiling: build failed" >&2
  exit 2
fi

(cd "$ROOT" && "$EXE" > "$OUT/observed_window_ceiling.stdout.json" 2>&1)
status=$?
echo "window_ceiling: exit=$status"

python3 - "$OUT/observed_window_ceiling.stdout.json" "$OUT/observed_window_ceiling.json" <<'PY'
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
    "case": "m2.window_ceiling",
    "command": "validation/process/calibration-faults/run_window_ceiling.sh",
    "submitted": payload["submitted"],
    "accepted": payload["accepted"],
    "refused": payload["refused"],
    "other": payload["other"],
    "max_active": payload["max_active"],
    "ready_window": "2P ready + 8P deferred (P=1 => at most 10)",
    "control_ceiling": "control work spends one command CPU token (P=2, ceiling=1 => max_active 1)",
    "checks": checks,
    "failures": failed,
    "status": "pass" if not failed else "fail",
}
with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(observed, handle, ensure_ascii=False, indent=2)
print("window_ceiling:", observed["status"], f"{len(checks) - len(failed)}/{len(checks)}")
raise SystemExit(0 if not failed else 1)
PY
exit $?
