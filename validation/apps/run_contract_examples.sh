#!/usr/bin/env bash
# Runs the three contract examples through the real acsd-contract driver and
# stores the responses, the hashes and the comparison with the expected model.
#
# The script never reads an expected model to produce a result: the driver
# decodes the request and calls the public API.  The comparison is a pure
# post-processing step.
#
# Usage:
#   validation/apps/run_contract_examples.sh [output-directory]
#
# Environment:
#   ACSD_CONTRACT   path to an existing acsd-contract binary.  When it is not
#                   set the script first tries the CMake target and then falls
#                   back to a direct compile of the driver with the base
#                   libraries.
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/validation/apps/evidence}"
ATTEMPTS="$OUT/attempts"
mkdir -p "$OUT" "$ATTEMPTS"

BIN="${ACSD_CONTRACT:-}"

if [[ -z "$BIN" ]]; then
  BUILD="$ROOT/.work/apps-build"
  mkdir -p "$BUILD"
  BIN="$BUILD/acsd-contract"
  if [[ ! -x "$BIN" ]]; then
    echo "[build] compiling the contract driver against the base libraries"
    SOURCES=()
    for dir in model numerics operators uncertainty storage resources coordinates; do
      for file in "$ROOT"/src/$dir/*.cpp; do
        [[ -e "$file" ]] && SOURCES+=("$file")
      done
    done
    for file in "$ROOT"/apps/contract/*.cpp; do
      SOURCES+=("$file")
    done
    if ! g++ -std=c++20 -O1 -fno-fast-math -ffp-contract=off -I"$ROOT/include" -I"$ROOT/src" \
         "${SOURCES[@]}" -o "$BIN" -lzstd -lpthread 2>"$BUILD/build.log"; then
      echo "[build] the direct compile failed, see $BUILD/build.log" >&2
      exit 4
    fi
  fi
fi

echo "[run] driver: $BIN"
STATUS=0
for name in u01 external-array to-fits; do
  request="$ROOT/examples/$name.request.json"
  response="$OUT/$name.response.json"
  echo "[run] $name"
  "$BIN" --request "$request" --response "$response"
  code=$?
  if [[ $code -ne 0 ]]; then
    echo "[run] $name exited with $code, the attempt is kept" >&2
    cp "$response" "$ATTEMPTS/$name.failure.response.json" 2>/dev/null || true
    STATUS=$code
  fi
  echo "$code" > "$OUT/$name.exit_code"
done

python3 "$ROOT/validation/apps/compare_examples.py" "$OUT" "$ROOT/examples" | tee "$OUT/comparison.txt"
exit $STATUS
