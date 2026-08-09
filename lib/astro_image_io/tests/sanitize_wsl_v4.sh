#!/usr/bin/env bash
# sanitize_wsl_v4.sh - WSL ASan/UBSan/LSan 健壮性验证 (Phase1 Final Signoff V4, G6)
#
# 覆盖:
#   [1] 共享 HEALPix core (ang2pix/pix2ang) vs 1,000,000 全天 Oracle
#   [2] HiPS writer/reader FP32 可移植核心
#   [3] HiPS robustness: FP32/FP64 + VOTable metadata + corrupt TSV/properties/Moc
#   [4] DR3SP parser (gaia_client)
#   [5] Catalogue spatial fuzz (order 7, 随机+对抗点) vs astropy-healpix
#
# 用法: sanitize_wsl_v4.sh <healpix_oracle.jsonl> <fuzz_oracle.jsonl> [log]
set -e
HEALPIX_ORACLE="$1"
FUZZ_ORACLE="$2"
LOG="${3:-/tmp/astrocs_sanitize_v4.log}"
exec > >(tee -a "$LOG") 2>&1

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="/tmp/astrocs_sanitize_v4"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra"
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:strict_string_checks=1"
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
export ASAN_OPTIONS UBSAN_OPTIONS

echo "=== [1/5] shared HEALPix core vs 1,000,000 full-sky Oracle ==="
timeout 1200 g++ $FLAGS -I"$ROOT/lib/common" \
    "$ROOT/lib/common/healpix/healpix_core.cpp" \
    "$ROOT/lib/common/healpix/tests/test_healpix_oracle.cpp" \
    -o healpix_oracle
timeout 1200 ./healpix_oracle "$HEALPIX_ORACLE" | tee healpix_oracle.out
grep -q "RESULT: PASS (mismatch=0)" healpix_oracle.out

echo "=== [2/5] HiPS writer/reader (FP32) ==="
gcc $FLAGS -I"$ROOT/lib/astro_image_io/third_party/cfitsio" \
    "$ROOT"/lib/astro_image_io/third_party/cfitsio/*.c -c > cfitsio_cc.log 2>&1 || true
rm -f f77_wrap1.o f77_wrap2.o f77_wrap3.o f77_wrap4.o drvrgsiftp.o windumpexts.o vmsieee.o
ls *.o >/dev/null
timeout 900 g++ $FLAGS -I"$ROOT/lib/astro_image_io/include" -I"$ROOT/lib/astro_image_io/src" \
    -I"$ROOT/lib/common" -I"$ROOT/lib/astro_image_io/third_party/cfitsio" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_writer.cpp" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_reader.cpp" \
    "$ROOT/lib/common/healpix/healpix_core.cpp" \
    "$ROOT/lib/astro_image_io/tests/hips_sanitize_driver.cpp" \
    *.o -lz -lm -o hips_sanitize
timeout 600 ./hips_sanitize /tmp/hips_san_v4_f32 | tee hips_sanitize.out
grep -q "HIPS_SANITIZE_OK" hips_sanitize.out
rm -rf /tmp/hips_san_v4_f32

echo "=== [3/5] HiPS robustness FP32/FP64 + corrupt inputs ==="
timeout 900 g++ $FLAGS -I"$ROOT/lib/astro_image_io/include" -I"$ROOT/lib/astro_image_io/src" \
    -I"$ROOT/lib/common" -I"$ROOT/lib/astro_image_io/third_party/cfitsio" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_writer.cpp" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_reader.cpp" \
    "$ROOT/lib/common/healpix/healpix_core.cpp" \
    "$ROOT/lib/astro_image_io/tests/hips_robust_sanitize_driver.cpp" \
    *.o -lz -lm -o hips_robust
timeout 600 ./hips_robust /tmp/hips_san_v4_robust_f32 0 | tee hips_robust_f32.out
grep -q "HIPS_ROBUST_SANITIZE_OK dtype=0" hips_robust_f32.out
rm -rf /tmp/hips_san_v4_robust_f32
timeout 600 ./hips_robust /tmp/hips_san_v4_robust_f64 1 | tee hips_robust_f64.out
grep -q "HIPS_ROBUST_SANITIZE_OK dtype=1" hips_robust_f64.out
rm -rf /tmp/hips_san_v4_robust_f64

echo "=== [4/5] DR3SP parser (gaia_client) ==="
timeout 900 gcc $FLAGS -I"$ROOT/lib/gaia_xpsd_client/src" \
    "$ROOT/lib/gaia_xpsd_client/src/gaia_client.c" \
    "$ROOT/lib/astro_image_io/tests/gaia_sanitize_driver.c" \
    -lz -lm -o gaia_sanitize
timeout 600 ./gaia_sanitize "/mnt/f/Astro dev/Astro CS Normalization Database/GaiaDR3SP" | tee gaia_sanitize.out

echo "=== [5/5] Catalogue spatial fuzz (order 7) ==="
timeout 600 ./healpix_oracle "$FUZZ_ORACLE" | tee healpix_fuzz.out
grep -q "RESULT: PASS (mismatch=0)" healpix_fuzz.out

echo "ALL_SANITIZE_V4_PASS"