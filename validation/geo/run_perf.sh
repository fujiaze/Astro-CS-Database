#!/usr/bin/env bash
# Builds and runs the geometry performance profile.
#
# The driver reproduces the configuration that blocked the normalise workflow
# (a 128 by 128 pixel frame split into blocks of 16 by 16 pixels, the HEALPix
# order 6, pixfrac 1.0 and the work budget of two million units) and records
# the wall clock per source row together with the stage counters of the two
# geometry algorithms.
#
# Usage: validation/geo/run_perf.sh [build-directory] [order] [block-size]
#                                   [width] [height] [max-work] [rows]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../.." && pwd)"
build="${1:-${root}/build/dev}"
order="${2:-6}"
block_size="${3:-16}"
width="${4:-128}"
height="${5:-128}"
max_work="${6:-2000000}"
rows="${7:-0}"
max_depth="${8:-24}"

if [ ! -f "${build}/libgeometry.a" ]; then
  cmake -S "${root}" -B "${build}" -G Ninja -DACSD_BUILD_SCIENCE=ON
  cmake --build "${build}" --target model numerics coordinates operators geometry
fi

g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "${root}/include" -I "${root}/src" \
  "${here}/perf_geometry.cpp" \
  -o "${here}/perf_geometry" \
  "${build}/libgeometry.a" "${build}/libcoordinates.a" "${build}/liboperators.a" \
  "${build}/libnumerics.a" "${build}/libmodel.a"

"${here}/perf_geometry" "${here}" "${order}" "${block_size}" "${width}" "${height}" \
  "${max_work}" "${rows}" "${max_depth:-24}"
