#!/usr/bin/env bash
# Publishes a synthetic frame product, reopens it from the disk and reads the
# published signal with an independent parser.  The program uses only the
# public declaration contract.
set -u
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/validation/apps/evidence"
BUILD="$ROOT/.work/apps-build"
mkdir -p "$BUILD" "$OUT"

LIBS=()
for lib in workflows photometry sources coadd sky information projection geometry response resources storage observation catalogue uncertainty operators coordinates numerics model; do
  LIBS+=("$ROOT/build/science/lib$lib.a")
done

# The program is always rebuilt: the static libraries of the build directory
# change with every commit of the science modules.
echo "[build] compiling the round trip program"
g++ -std=c++20 -O1 -I"$ROOT/include" "$ROOT/validation/apps/product_roundtrip.cpp" \
    -o "$BUILD/product_roundtrip" "${LIBS[@]}" -lzstd -lcfitsio -lpthread || exit 4

"$BUILD/product_roundtrip" "$ROOT/.work/product-roundtrip" | tee "$OUT/product_roundtrip.json"
python3 "$ROOT/validation/apps/check_product_roundtrip.py" "$OUT/product_roundtrip.json" \
    "$ROOT/.work/product-roundtrip" | tee "$OUT/product_roundtrip.check.txt"
