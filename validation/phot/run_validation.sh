#!/bin/sh
# Independent validation of the response and photometry modules.
#
# The program uses only the public declaration contract (include/acsd/contracts.hpp);
# it implements its own storage, resource and cancellation providers, so it does
# not depend on any internal header of the implementation.
#
# Usage:  validation/phot/run_validation.sh [build directory]
# The build directory must contain the libraries built by the project build
# (libmodel.a libnumerics.a liboperators.a libuncertainty.a libresponse.a
# libphotometry.a).
set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${1:-$ROOT/build/dev}
OUT=${OUT:-$ROOT/validation/phot/evidence}
CXX=${CXX:-g++}
FLAGS="-std=c++20 -O2 -fno-fast-math -ffp-contract=off"

if [ ! -f "$BUILD/libphotometry.a" ]; then
  echo "the libraries are missing in $BUILD; build them first:" >&2
  echo "  cmake -S . -B build/dev -G Ninja -DACSD_BUILD_SCIENCE=ON" >&2
  echo "  cmake --build build/dev --target response photometry" >&2
  exit 2
fi

mkdir -p "$OUT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

$CXX $FLAGS -I "$ROOT/include" -I "$ROOT/validation/phot" \
  "$ROOT/validation/phot/phot_validation.cpp" \
  "$BUILD/libphotometry.a" "$BUILD/libresponse.a" "$BUILD/libuncertainty.a" \
  "$BUILD/liboperators.a" "$BUILD/libnumerics.a" "$BUILD/libmodel.a" \
  -o "$TMP/phot_validation"

"$TMP/phot_validation" | tee "$OUT/phot_validation.txt"
