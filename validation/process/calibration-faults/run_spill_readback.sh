#!/usr/bin/env bash
# Build and run the C-CAL-SPILL finish readback case.
#
# The same input is calibrated in two separate processes: Placement::memory and
# Placement::spill.  Separate processes are required because the runtime block
# store is a process-wide cache; a spill run in the same process would hit the
# memory run's cached bytes and would not exercise the asynchronous readback.
#
# Required archives come from $ACSD_BUILD (default /tmp/acsd-build).  Evidence
# is the raw stdout of both runs plus the comparison JSON written here.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/process/calibration-faults"
EXE="${TMPDIR:-/tmp}/acsd-cal-spill-run"

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
  echo "spill_readback: pending, missing archives:$missing" >&2
  exit 3
fi

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" \
  "$OUT/spill_readback.cpp" -L "$BUILD" $LIBS -lpthread -lzstd -o "$EXE"
if [ $? -ne 0 ]; then
  echo "spill_readback: build failed" >&2
  exit 2
fi

(cd "$ROOT" && "$EXE" memory > "$OUT/observed_spill_memory.stdout.json" 2>&1)
memory_status=$?
(cd "$ROOT" && "$EXE" spill > "$OUT/observed_spill_spill.stdout.json" 2>&1)
spill_status=$?
echo "spill_readback: memory exit=$memory_status spill exit=$spill_status"

python3 - "$OUT/observed_spill_memory.stdout.json" \
          "$OUT/observed_spill_spill.stdout.json" \
          "$OUT/observed_spill.json" <<'PY'
import json
import sys

memory_path, spill_path, output_path = sys.argv[1], sys.argv[2], sys.argv[3]


def payload(path):
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if line.startswith("{"):
            return json.loads(line)
    raise SystemExit("no JSON payload in " + path)


memory = payload(memory_path)
spill = payload(spill_path)
checks = {
    "memory_ok": memory["code"] == 0,
    "spill_ok": spill["code"] == 0,
    "same_identity": memory["identity"] == spill["identity"],
    "same_content_hash": memory["content_hash"] == spill["content_hash"],
    "same_samples": memory["samples"] == spill["samples"],
    "memory_no_readback": memory["materialized_bytes"] == 0,
    "spill_readback_ran": spill["readback_bytes"] > 0,
    "spill_materialized_bytes": spill["materialized_bytes"] > 0,
    "two_blocks_per_array": spill["blocks_per_array"] >= 2,
}
failed = [name for name, passed in checks.items() if not passed]
observed = {
    "case": "cal.spill_readback",
    "command": "validation/process/calibration-faults/run_spill_readback.sh",
    "n": spill["n"],
    "block_elements": spill["block_elements"],
    "blocks_per_array": spill["blocks_per_array"],
    "readback_bytes": spill["readback_bytes"],
    "materialized_bytes": spill["materialized_bytes"],
    "memory": memory,
    "spill": spill,
    "checks": checks,
    "failures": failed,
    "status": "pass" if not failed else "fail",
}
with open(output_path, "w", encoding="utf-8") as handle:
    json.dump(observed, handle, ensure_ascii=False, indent=2)
print("spill_readback:", observed["status"], "/".join(sorted(checks)))
raise SystemExit(0 if not failed else 1)
PY
status=$?
exit $status
