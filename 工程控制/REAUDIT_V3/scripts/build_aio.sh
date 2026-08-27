#!/usr/bin/env bash
# Clean build of lib/astro_image_io from freshly exported current source
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current/lib/astro_image_io"
B="$ROOT/builds"
LOG="$ROOT/logs"
mkdir -p "$B/aio_clean" "$LOG"

GD=$(git -C "/home/lighthouse/Astro CS Database" rev-parse --short HEAD 2>/dev/null || echo unknown)
{
  echo "layer=astro_image_io-linux-release"
  echo "source_tree_commit=HEAD ($GD)"
  echo "source_dir_was=$SRC (fresh git archive export; no prior build artifacts)"
  echo "compiler=$(g++ --version | head -1)"
  echo "build_cmd=make -j1 all"
  echo "cxxflags_from_Makefile=-O2 -march=native -Wall -std=c++17 -fopenmp -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF -DAIO_ENABLE_AHPX -DAIO_ENABLE_HEALPIX -DAIO_ENABLE_COMPRESSOR -DAIO_ENABLE_PIPELINE -DHAS_ZSTD -DHAS_LZ4"
  echo "start_utc=$(date -u +%FT%TZ)"
} > "$B/aio_clean/BUILD_META.txt"
cd "$SRC"
echo "=== CWD: $PWD"
echo "=== CLEAN TREE CHECK ==="
ls -la *.dll *.o 2>/dev/null | head || echo "no artifacts present (clean)"
/usr/bin/time -v -o "$LOG/aio_build_time.txt" timeout 3600 make -j1 all > "$LOG/aio_build_stdout.log" 2> "$LOG/aio_build_stderr.log"
EC=$?
echo "=== build exit=$EC"
{ echo "end_utc=$(date -u +%FT%TZ)"; echo "build_exit=$EC"; } >> "$B/aio_clean/BUILD_META.txt"
echo "=== artifact ==="
ls -la astro_image_io.dll 2>/dev/null && sha256sum astro_image_io.dll && file astro_image_io.dll && ldd astro_image_io.dll 2>&1 | head -20
echo "=== warning/error counts ==="
echo "warnings: $(grep -ciE "warning:" "$LOG/aio_build_stdout.log" 2>/dev/null || echo 0)"
echo "errors: $(grep -ciE "error:" "$LOG/aio_build_stdout.log" 2>/dev/null || echo 0)"
echo "AIO_BUILD_DONE exit=$EC"
