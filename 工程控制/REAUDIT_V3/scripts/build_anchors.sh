#!/usr/bin/env bash
# Build A (b38b446) and B (8347197) anchors: AIO(-fPIC) + phase2 release on Linux (Control §12 readiness).
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
B="$ROOT/builds"
LOG="$ROOT/logs"
mkdir -p "$B" "$LOG"

build_anchor() {
  local name="$1" treedir="$2"
  echo "=========== $name ==========="
  local AIO="$treedir/lib/astro_image_io"
  local P2="$treedir/lib/phase2"
  echo "--- AIO build (documented cmd) ---"
  cd "$AIO"
  rm -f *.o *.dll third_party/cfitsio/*.o 2>/dev/null
  timeout 3600 make -j1 all > "$LOG/${name}_aio_doc.log" 2>&1; echo "AIO doc exit=$?"
  if [ ! -f astro_image_io.dll ]; then
    echo "--- AIO build (-fPIC workaround) ---"
    rm -f *.o *.dll third_party/cfitsio/*.o 2>/dev/null
    timeout 3600 make -j1 CXXFLAGS="-O2 -march=native -Wall -fPIC -std=c++17 -fopenmp -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF -DAIO_ENABLE_AHPX -DAIO_ENABLE_HEALPIX -DAIO_ENABLE_COMPRESSOR -DAIO_ENABLE_PIPELINE -DHAS_ZSTD -DHAS_LZ4" CFLAGS_VENDORED="-O2 -march=native -w -fPIC" > "$LOG/${name}_aio_fpic.log" 2>&1; echo "AIO fpic exit=$?"
  fi
  ls -la astro_image_io.dll 2>/dev/null && sha256sum astro_image_io.dll || echo NO_AIO_DLL
  echo "--- phase2 CMake build ---"
  local PBDIR="$B/${name}_phase2"
  rm -rf "$PBDIR"; mkdir -p "$PBDIR"
  timeout 300 cmake -S "$P2" -B "$PBDIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DP2_ENABLE_OPENMP=OFF > "$LOG/${name}_p2_cfg.log" 2>&1; echo "cfg exit=$?"
  timeout 3600 cmake --build "$PBDIR" -j1 > "$LOG/${name}_p2_build.log" 2>&1; echo "build exit=$?"
  ls -la "$PBDIR/astrocs-stage2" 2>/dev/null && sha256sum "$PBDIR/astrocs-stage2" || echo NO_STAGE2
  grep -cE "error:" "$LOG/${name}_p2_build.log" 2>/dev/null || echo 0
  echo "--- ctest -N ---"
  timeout 60 ctest -N --test-dir "$PBDIR" 2>&1 | tail -3
}

build_anchor "anchorA_b38b446" "$ROOT/historical_b38b446"
build_anchor "anchorB_8347197" "$ROOT/historical_8347197"
echo "ANCHOR_BUILDS_DONE"
