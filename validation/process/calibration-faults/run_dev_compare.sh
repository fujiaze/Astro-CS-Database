#!/usr/bin/env bash
# C-CAL-DEV ON: run the same calibration Host case with ACSD_DEV_MODE=0 and
# ACSD_DEV_MODE=1, then prove that the scientific code/model/hash and the
# harness exit status are identical and that the probe counters are real.
#
# Builds:
#   ACSD_BUILD      default /tmp/acsd-build      (ACSD_DEV_MODE=0)
#   ACSD_DEV_BUILD  default /tmp/acsd-build-dev  (ACSD_DEV_MODE=1, pre-built)
#
# The two binaries share the same cal_host.cpp source; only the runtime/science
# archives differ.  Evidence is written next to this script.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
DEV_BUILD="${ACSD_DEV_BUILD:-/tmp/acsd-build-dev}"
CXX="${CXX:-g++}"
OUT="$ROOT/validation/process/calibration-faults"
SRC="$ROOT/validation/runtime/calibration/cal_host.cpp"

link_libs() {
  local build=$1
  local libs="-Wl,--start-group"
  for name in acsd_builtin runtime codec workflows photometry sources coadd sky \
              information projection geometry response resources storage \
              observation catalogue uncertainty operators coordinates numerics model; do
    if [ -e "$build/lib$name.a" ]; then
      libs+=" -l$name"
    else
      echo "missing archive: $build/lib$name.a" >&2
      return 3
    fi
  done
  libs+=" -Wl,--end-group"
  echo "$libs"
}

OFF_LIBS="$(link_libs "$BUILD")" || exit 3
DEV_LIBS="$(link_libs "$DEV_BUILD")" || exit 3

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib" \
  "$SRC" -L "$BUILD" $OFF_LIBS -lpthread -lzstd -o /tmp/acsd-cal-dev-off
[ $? -ne 0 ] && { echo "dev_compare: off build failed" >&2; exit 2; }
"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "$ROOT/include" -I "$ROOT/src" -I "$ROOT/validation/runtime/lib" \
  "$SRC" -L "$DEV_BUILD" $DEV_LIBS -lpthread -lzstd -o /tmp/acsd-cal-dev-on
[ $? -ne 0 ] && { echo "dev_compare: dev build failed" >&2; exit 2; }

run_case() {
  local name=$1
  shift
  local dir="/tmp/acsd-dev-compare-$name"
  rm -rf "$dir"
  (cd "$ROOT" && env "$@" /tmp/acsd-cal-dev-on > "$OUT/observed_dev_${name}.stdout.json" 2>&1)
  echo "$? $dir"
}

OFF_STATUS=$( (cd "$ROOT" && /tmp/acsd-cal-dev-off > "$OUT/observed_dev_off.stdout.json" 2>&1); echo $? )
read -r SELECTED_STATUS SELECTED_DIR <<<"$(run_case selected ACSD_DEV_DIR=/tmp/acsd-dev-compare-selected ACSD_DEV_PROBES=normalize.calibration.signal)"
read -r DROPPED_STATUS DROPPED_DIR <<<"$(run_case dropped ACSD_DEV_DIR=/tmp/acsd-dev-compare-dropped ACSD_DEV_PROBES=normalize.calibration.signal,framework.invocation.phase ACSD_DEV_MAX_ITEMS=1)"
read -r FAILED_STATUS FAILED_DIR <<<"$(run_case diagnostics_failed ACSD_DEV_DIR=/dev/null/acsd-dev-unavailable ACSD_DEV_PROBES=normalize.calibration.signal)"

echo "dev_compare: off=$OFF_STATUS selected=$SELECTED_STATUS dropped=$DROPPED_STATUS failed=$FAILED_STATUS"

python3 - "$OUT" "$OFF_STATUS" "$SELECTED_STATUS" "$DROPPED_STATUS" "$FAILED_STATUS" \
  "$SELECTED_DIR" "$DROPPED_DIR" "$FAILED_DIR" <<'PY'
import json
import sys

out, off_status, selected_status, dropped_status, failed_status = sys.argv[1:6]
selected_dir, dropped_dir, failed_dir = sys.argv[6:9]


def load_stdout(path):
    text = open(path, encoding="utf-8").read()
    lines = text.splitlines()
    for index in range(len(lines) - 1, -1, -1):
        if lines[index].lstrip().startswith("{"):
            candidate = "\n".join(lines[index:]).strip()
            try:
                return json.loads(candidate)
            except json.JSONDecodeError:
                continue
    raise SystemExit("no JSON payload in " + path)


def summary(directory):
    try:
        return json.load(open(directory + "/summary.json", encoding="utf-8"))
    except FileNotFoundError:
        return None


off = load_stdout(out + "/observed_dev_off.stdout.json")
selected = load_stdout(out + "/observed_dev_selected.stdout.json")
dropped = load_stdout(out + "/observed_dev_dropped.stdout.json")
failed = load_stdout(out + "/observed_dev_diagnostics_failed.stdout.json")

science_keys = ["checks", "values"]
same_science = all(off[key] == run[key] for run in (selected, dropped, failed) for key in science_keys)
same_code = off.get("failures") == [] and all(
    run.get("failures") == [] for run in (selected, dropped, failed)
)
same_exit = all(status == "0" for status in (off_status, selected_status, dropped_status, failed_status))
selected_summary = summary(selected_dir)
dropped_summary = summary(dropped_dir)
failed_summary = summary(failed_dir)

counts = {
    "selected": bool(selected_summary and selected_summary.get("selected", 0) > 0),
    "not_selected": bool(selected_summary and selected_summary.get("not_selected", 0) > 0),
    "written": bool(selected_summary and selected_summary.get("written", 0) > 0),
    "dropped": bool(dropped_summary and dropped_summary.get("dropped", 0) > 0),
    "dropped_skipped_budget": bool(dropped_summary and dropped_summary.get("skipped_budget", 0) > 0),
    "diagnostics_failure_left_sink_absent": failed_summary is None,
    "off_identity_matches_on": off.get("values", {}).get("host.dev_off_identity")
    == selected.get("values", {}).get("host.dev_off_identity"),
}
checks = {
    "same_science_checks_and_values": same_science,
    "same_success_exit_and_no_failures": same_code and same_exit,
    "selected_counters_real": counts["selected"] and counts["not_selected"] and counts["written"],
    "dropped_counter_real": counts["dropped"],
    "diagnostics_failure_did_not_change_science": failed_summary is None and failed.get("failures") == [],
    "off_and_on_same_identity": counts["off_identity_matches_on"],
}
failed_checks = [name for name, passed in checks.items() if not passed]
observed = {
    "case": "cal.dev_compare",
    "command": "validation/process/calibration-faults/run_dev_compare.sh",
    "off_build": "/tmp/acsd-build",
    "on_build": "/tmp/acsd-build-dev",
    "off": off,
    "selected": selected,
    "dropped": dropped,
    "diagnostics_failed": failed,
    "exit_codes": {
        "off": int(off_status),
        "selected": int(selected_status),
        "dropped": int(dropped_status),
        "diagnostics_failed": int(failed_status),
    },
    "probe_summaries": {
        "selected": selected_summary,
        "dropped": dropped_summary,
        "diagnostics_failed": failed_summary,
    },
    "checks": checks,
    "failures": failed_checks,
    "note": (
        "an all-unselected run cannot be observed through summary.json because no probe "
        "enqueue schedules a flush and the Host path does not destroy the sink before "
        "process exit; the selected/dropped runs report the real selected, not_selected "
        "and dropped counters instead"
    ),
    "status": "pass" if not failed_checks else "fail",
}
with open(out + "/observed_dev_compare.json", "w", encoding="utf-8") as handle:
    json.dump(observed, handle, ensure_ascii=False, indent=2)
print("dev_compare:", observed["status"], "/".join(sorted(checks)))
raise SystemExit(0 if not failed_checks else 1)
PY
exit $?
