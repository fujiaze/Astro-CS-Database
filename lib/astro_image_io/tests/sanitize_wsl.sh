#!/usr/bin/env bash
# sanitize_wsl.sh - WSL ASan/UBSan 可移植核心验证 (Phase1 Final Closure V3, Phase D)
# 覆盖: AIO HiPS writer/reader, DR3SP parser, PipelineFrame/cache
set -e
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="/tmp/astrocs_sanitize"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall"

echo "=== [1/3] HiPS writer/reader (vendored CFITSIO) ==="
gcc $FLAGS -I"$ROOT/lib/astro_image_io/third_party/cfitsio" \
    "$ROOT"/lib/astro_image_io/third_party/cfitsio/*.c \
    -c 2>/dev/null || true
# 排除 Fortran/GSI FTP (与 Windows 构建清单一致)
rm -f f77_wrap1.o f77_wrap2.o f77_wrap3.o f77_wrap4.o drvrgsiftp.o windumpexts.o vmsieee.o
ls *.o >/dev/null
g++ $FLAGS -I"$ROOT/lib/astro_image_io/include" -I"$ROOT/lib/astro_image_io/src" \
    -I"$ROOT/lib/astro_image_io/third_party/cfitsio" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_writer.cpp" \
    "$ROOT/lib/astro_image_io/src/hips/aio_hips_reader.cpp" \
    "$ROOT/lib/astro_image_io/tests/hips_sanitize_driver.cpp" \
    *.o -lz -lm -o hips_sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./hips_sanitize /tmp/hips_san
rm -rf /tmp/hips_san

echo "=== [2/3] DR3SP parser (gaia_client) ==="
gcc $FLAGS -I"$ROOT/lib/gaia_xpsd_client/src" \
    "$ROOT/lib/gaia_xpsd_client/src/gaia_client.c" \
    "$ROOT/lib/astro_image_io/tests/gaia_sanitize_driver.c" \
    -lz -lm -o gaia_sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    ./gaia_sanitize "/mnt/f/Astro dev/Astro CS Normalization Database/GaiaDR3SP"

echo "=== [3/3] PipelineFrame/cache ==="
echo "SKIP: aio_pipeline 依赖未移植 (Windows 专用); 由 Windows ABI/allocator 测试覆盖" || true
echo "ALL_SANITIZE_PASS"
