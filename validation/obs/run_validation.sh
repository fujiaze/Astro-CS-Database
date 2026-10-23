#!/bin/sh
# Builds and runs the independent observation/PSF/measurement validation.
#
# The program only includes the public contract header; it brings its own
# in-memory Storage and Resources.  The science libraries are compiled from
# src/ with the same numeric flags the project CMake uses.
#
# Usage: validation/obs/run_validation.sh [build-directory]
set -e

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD=${1:-"${TMPDIR:-/tmp}/acsd-obs-validation"}
mkdir -p "$BUILD"

CXX=${CXX:-g++}

compile() {
  source_file=$1
  object="$BUILD/$(echo "$source_file" | sed "s|$ROOT/||; s|/|_|g; s|\.cpp$|.o|")"
  # The object is rebuilt only when the source or the contract header is newer.
  if [ ! -f "$object" ] || [ "$source_file" -nt "$object" ] || \
     [ "$ROOT/include/acsd/contracts.hpp" -nt "$object" ]; then
    "$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse \
        -include iostream \
        -I "$ROOT/include" -I "$ROOT/src" -c "$source_file" -o "$object"
  fi
  echo "$object"
}

SOURCES="
src/model/array.cpp
src/model/exact.cpp
src/model/hash.cpp
src/model/units.cpp
src/numerics/interval.cpp
src/numerics/linalg.cpp
src/numerics/quadrature.cpp
src/operators/api.cpp
src/operators/linear_action.cpp
src/uncertainty/api.cpp
src/uncertainty/covariance.cpp
src/resources/cpu_resources.cpp
src/storage/local_storage.cpp
src/coordinates/healpix.cpp
src/coordinates/wcs.cpp
src/coordinates/tan_sip.cpp
src/coordinates/chart.cpp
src/observation/observation.cpp
src/observation/calibrate.cpp
src/observation/regroup.cpp
src/observation/bind.cpp
src/sources/psf.cpp
src/sources/psf_model.cpp
src/sources/psf_fit.cpp
src/sources/measure.cpp
validation/obs/obs_validation.cpp
validation/obs/cases_calibration.cpp
validation/obs/cases_regroup.cpp
validation/obs/cases_psf.cpp
validation/obs/cases_measurement.cpp
validation/obs/cases_bind.cpp
validation/obs/cases_timing.cpp
"

OBJECTS=""
for relative in $SOURCES; do
  source_file="$ROOT/$relative"
  if [ ! -f "$source_file" ]; then
    echo "missing source: $source_file" >&2
    exit 2
  fi
  OBJECTS="$OBJECTS $(compile "$source_file")"
done

"$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I "$ROOT/include" -I "$ROOT/src" \
    $OBJECTS -o "$BUILD/obs_validation" -lzstd -lpthread
"$BUILD/obs_validation"
