#!/usr/bin/env bash
# Builds and runs the independent geo validation program.
#
# The program uses only the public declaration contract (include/acsd/contracts.hpp)
# and the public free functions; it implements its own Resources, Storage and
# tangent plane / HEALPix chart oracle.  The science libraries are linked from
# the CMake build tree.
#
# Usage: validation/geo/run.sh [build-directory]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../.." && pwd)"
build="${1:-${root}/build/dev}"

if [ ! -f "${build}/libgeometry.a" ]; then
  cmake -S "${root}" -B "${build}" -G Ninja -DACSD_BUILD_SCIENCE=ON
  cmake --build "${build}" --target model numerics coordinates operators geometry
fi

g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
  -I "${root}/include" \
  "${here}/geo_validation.cpp" \
  -o "${here}/geo_validation" \
  "${build}/libgeometry.a" "${build}/libcoordinates.a" "${build}/liboperators.a" \
  "${build}/libnumerics.a" "${build}/libmodel.a"

"${here}/geo_validation" "${here}"
