#!/usr/bin/env bash
# Build every validation/runtime case into validation/runtime/build.
#
# The libraries come from $ACSD_BUILD (default /tmp/acsd-build).  Two naming
# conventions are accepted because the CMake targets are named without the
# "acsd_" prefix (libmodel.a, libruntime.a): the script probes the directory
# and records the actual link command.  A missing runtime library is reported
# as pending_runtime, never as a pass.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${ACSD_BUILD:-/tmp/acsd-build}"
OUT="$ROOT/validation/runtime/build"
CXX="${CXX:-g++}"
COMMON=(-std=c++20 -O2 -fno-fast-math -ffp-contract=off
        -I "$ROOT/include" -I "$ROOT/validation/runtime/lib")
mkdir -p "$OUT"
: > "$OUT/build_results.tsv"

lib_flag() {
  local name="$1"
  if [ -e "$BUILD/lib$name.a" ] || [ -e "$BUILD/lib$name.so" ]; then
    printf -- '-l%s' "$name"
    return 0
  fi
  if [ -e "$BUILD/libacsd_$name.a" ] || [ -e "$BUILD/libacsd_$name.so" ]; then
    printf -- '-lacsd_%s' "$name"
    return 0
  fi
  return 1
}

# The default link set is fixed by the task: runtime, model, numerics, storage,
# pthread, zstd.  The "science" set is only for the C-CAL kernel case.
default_libs=""
missing_libs=""
for name in runtime model numerics storage; do
  if flag="$(lib_flag "$name")"; then
    default_libs+="$flag "
  else
    missing_libs+="$name "
  fi
done
science_libs=""
for name in builtin codec observation coordinates catalogue sources response photometry \
            geometry operators uncertainty information sky coadd projection workflows \
            storage resources numerics model runtime; do
  if flag="$(lib_flag "$name")"; then
    science_libs+="$flag "
  else
    missing_libs+="$name "
  fi
done

record() {  # case status classification command
  printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" >> "$OUT/build_results.tsv"
}

build_case() {
  local case="$1" extra_flags="$2" libs="$3" link_extra="$4"
  local src="$ROOT/validation/runtime/$case/$case.cpp"
  local exe="$OUT/$case"
  local log="$OUT/$case.build.log"
  local compile_cmd=( "$CXX" "${COMMON[@]}" $extra_flags -c "$src" -o "$OUT/$case.o" )
  local link_cmd=( "$CXX" "$OUT/$case.o" -L "$BUILD" -Wl,--start-group $libs -Wl,--end-group $link_extra -lpthread -lzstd -o "$exe" )
  {
    echo "compile: ${compile_cmd[*]}"
    "${compile_cmd[@]}" 2>&1
  } > "$log" 2>&1
  if [ $? -ne 0 ]; then
    record "$case" "not_built" "compile_error" "${compile_cmd[*]}"
    echo "[build] $case: compile_error (see $log)"
    return
  fi
  {
    echo "link: ${link_cmd[*]}"
    "${link_cmd[@]}" 2>&1
  } >> "$log" 2>&1
  if [ $? -ne 0 ]; then
    if grep -q "undefined reference to .acsd::rt::" "$log" || \
       grep -q "cannot find -l.*runtime" "$log"; then
      record "$case" "not_built" "pending_runtime" "${link_cmd[*]}"
      echo "[build] $case: pending_runtime (${missing_libs})"
    else
      record "$case" "not_built" "link_error" "${link_cmd[*]}"
      echo "[build] $case: link_error (see $log)"
    fi
    return
  fi
  record "$case" "built" "ok" "${link_cmd[*]}"
  echo "[build] $case: ok"
}

# A missing library is only pending when it is the runtime implementation,
# which is the subject of these cases; a missing science library for cal01 is
# a real build failure.
if [ -n "$missing_libs" ]; then
  echo "[build] missing libraries in $BUILD: $missing_libs" >&2
fi

ALL_CASES=(reg01 life01 work01 a01 floor01 fork01 order01 barrier01 multi01 pipe01 io01 b01 d01_off d01_on cal01)
SELECTED=("$@")
if [ ${#SELECTED[@]} -eq 0 ]; then
  SELECTED=("${ALL_CASES[@]}")
fi
for case_id in "${SELECTED[@]}"; do
  case "$case_id" in
    life01) build_case life01 "-g -fsanitize=address" "$default_libs" "-fsanitize=address" ;;
    d01_off) build_case d01_off "-DACSD_DEV_MODE=0" "" "" ;;
    cal01) build_case cal01 "" "$science_libs" "" ;;
    *) build_case "$case_id" "" "$default_libs" "" ;;
  esac
done

python3 - "$OUT/build_results.tsv" "$OUT/build_status.json" "$BUILD" <<'PY'
import json, os, sys
tsv, out, build = sys.argv[1], sys.argv[2], sys.argv[3]
cases = {}
if os.path.exists(out):
    try:
        cases = json.load(open(out)).get('cases', {})
    except Exception:
        cases = {}
with open(tsv) as handle:
    for line in handle:
        parts = line.rstrip('\n').split('\t')
        if len(parts) != 4:
            continue
        case, status, classification, command = parts
        cases[case] = {
            'built': status == 'built',
            'status': status,
            'classification': classification,
            'link_command': command,
            'log': f'validation/runtime/build/{case}.build.log',
        }
with open(out, 'w') as handle:
    json.dump({'build_dir': build, 'cases': cases}, handle, indent=2)
    handle.write('\n')
print(f'[build] wrote {out} ({sum(1 for c in cases.values() if c["built"])}/{len(cases)} built)')
PY
