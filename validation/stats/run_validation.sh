#!/bin/sh
# Builds and runs the independent validation of the statistics modules.
#
# The validation only uses the public contract of include/acsd/contracts.hpp;
# it never includes a header of src/ and never reuses a helper of the solver
# under test.  The build directory that holds the science libraries is given
# as the first argument (default build/stats).
set -e
BUILD=${1:-build/stats}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$BUILD/validation"
mkdir -p "$OUT"
CXX=${CXX:-g++}
STATUS=0
for TEST in information_test sky_test coadd_test; do
  SOURCE="$ROOT/validation/stats/$TEST.cpp"
  if [ ! -f "$SOURCE" ]; then continue; fi
  echo "=== building $TEST ==="
  "$CXX" -std=c++20 -O2 -fno-fast-math -ffp-contract=off \
    -I "$ROOT/include" -I "$ROOT/validation/stats" \
    "$SOURCE" -o "$OUT/$TEST" \
    -L "$BUILD" -lsky -lcoadd -linformation -luncertainty -loperators -lnumerics -lmodel
  echo "=== running $TEST ==="
  "$OUT/$TEST" || STATUS=1
done
exit $STATUS
