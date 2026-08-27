#!/usr/bin/env bash
# Workaround rebuild of AIO with -fPIC (out-of-repo build copy only; documented clean command failed with TLS relocation).
set -u
ROOT=$(cat /home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt)
SRC="$ROOT/current/lib/astro_image_io"
LOG="$ROOT/logs"
mkdir -p "$LOG"
cd "$SRC"
echo "=== manual clean of generated objects (build dir only) ==="
rm -f *.o *.dll third_party/cfitsio/*.o
CXXFLAGS_FPIC="-O2 -march=native -Wall -fPIC -std=c++17 -fopenmp -DAIO_ENABLE_FITS -DAIO_ENABLE_XISF -DAIO_ENABLE_AHPX -DAIO_ENABLE_HEALPIX -DAIO_ENABLE_COMPRESSOR -DAIO_ENABLE_PIPELINE -DHAS_ZSTD -DHAS_LZ4"
CFLAGS_VENDORED_FPIC="-O2 -march=native -w -fPIC"
echo "=== CMD: make CXXFLAGS=... CFLAGS_VENDORED=... -j1 all  (WORKAROUND: -fPIC added; Makefile source unmodified) ==="
/usr/bin/time -v -o "$LOG/aio_build_fpic_time.txt" timeout 3600 make CXXFLAGS="$CXXFLAGS_FPIC" CFLAGS_VENDORED="$CFLAGS_VENDORED_FPIC" -j1 all > "$LOG/aio_build_fpic_stdout.log" 2> "$LOG/aio_build_fpic_stderr.log"
EC=$?
echo "=== exit=$EC"
echo "=== artifact ==="
ls -la astro_image_io.dll 2>/dev/null && sha256sum astro_image_io.dll && file astro_image_io.dll && ldd astro_image_io.dll 2>&1 | head -20
echo "=== warnings(first-party) ==="
grep -c "warning:" "$LOG/aio_build_fpic_stdout.log" 2>/dev/null || echo 0
echo "AIO_FPIC_BUILD_DONE exit=$EC"
