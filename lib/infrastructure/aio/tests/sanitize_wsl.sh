#!/usr/bin/env bash
# sanitize_wsl.sh - WSL ASan/UBSan 可移植核心验证 (Phase1 Final Closure V3, Phase D)
# 覆盖: AIO HiPS writer/reader, DR3SP parser —— **仅这两项**，两项都真编真跑。
#
# 覆盖声明纪律（勿回退）:
#   · 脚本只宣称它真正执行的阶段。原先公告的第三阶段 "PipelineFrame/cache"
#     只有一行 `echo ... || true`，不编译不执行却仍计入「3 阶段」，属制造绿色产物，
#     已删除；它给的 SKIP 理由「aio_pipeline 依赖未移植」也不成立 ——
#     lib/infrastructure/aio/src/aio_pipeline.cpp 存在，且由根 CMakeLists.txt:624 编译。
#     该面要么另起一个真能跑的阶段（须先有可驱动的目标），要么不宣称；本脚本选后者。
#   · ALL_SANITIZE_PASS 只在「公告阶段数 == 实跑阶段数」且各阶段真通过时打印。
set -e

# 仓根 = 本文件所在 lib/infrastructure/aio/tests 的**四**级上级。
# （原为三级 ⇒ 解析到 <repo>/lib ⇒ "$ROOT/lib/..." 展开成 lib/lib/... 不存在。
#   同目录参照 hips_direct_smoke.py:25 的四级写法。）
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
[ -d "$ROOT/lib/infrastructure/aio/include" ] || {
    echo "ALL_SANITIZE_FAIL: 仓根算错，$ROOT 下找不到 lib/infrastructure/aio/include" >&2
    exit 1
}

BUILD="/tmp/acsd_sanitize"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

STAGES_TOTAL=2
STAGES_DONE=0

FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall"

echo "=== [1/2] HiPS writer/reader (vendored CFITSIO) ==="
# 编译失败即失败：原为 `-c 2>/dev/null || true`，把 cfitsio 的编译错误吞掉、只靠
# 下面的 `ls *.o` 兜底，属静默降级（禁）。去掉 `|| true`，由 set -e 硬失败。
gcc $FLAGS -I"$ROOT/lib/infrastructure/aio/third_party/cfitsio" \
    "$ROOT"/lib/infrastructure/aio/third_party/cfitsio/*.c \
    -c
# 排除 Fortran/GSI FTP (与 Windows 构建清单一致)
rm -f f77_wrap1.o f77_wrap2.o f77_wrap3.o f77_wrap4.o drvrgsiftp.o windumpexts.o vmsieee.o
ls *.o >/dev/null
g++ $FLAGS -I"$ROOT/lib/infrastructure/aio/include" -I"$ROOT/lib/infrastructure/aio/src" \
    -I"$ROOT/lib/infrastructure/aio/third_party/cfitsio" \
    "$ROOT/lib/infrastructure/aio/src/hips/aio_hips_writer.cpp" \
    "$ROOT/lib/infrastructure/aio/src/hips/aio_hips_reader.cpp" \
    "$ROOT/lib/infrastructure/aio/tests/hips_sanitize_driver.cpp" \
    *.o -lz -lm -o hips_sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./hips_sanitize /tmp/hips_san
rm -rf /tmp/hips_san
STAGES_DONE=$((STAGES_DONE+1))

echo "=== [2/2] DR3SP parser (gaia_client) ==="
gcc $FLAGS -I"$ROOT/lib/infrastructure/gaia_xpsd_client/src" \
    "$ROOT/lib/infrastructure/gaia_xpsd_client/src/gaia_client.c" \
    "$ROOT/lib/infrastructure/aio/tests/gaia_sanitize_driver.c" \
    -lz -lm -o gaia_sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    ./gaia_sanitize "/mnt/f/Astro dev/Astro CS Normalization Database/GaiaDR3SP"
STAGES_DONE=$((STAGES_DONE+1))

# 覆盖计数闸：公告了几段就必须真跑了几段。任一段被跳过或删除而未同步改
# STAGES_TOTAL 时，这里硬失败 —— 杜绝「少跑一段仍打全过」。
if [ "$STAGES_DONE" -ne "$STAGES_TOTAL" ]; then
    echo "ALL_SANITIZE_FAIL: 公告 $STAGES_TOTAL 阶段，实跑 $STAGES_DONE 阶段" >&2
    exit 1
fi
echo "ALL_SANITIZE_PASS ($STAGES_DONE/$STAGES_TOTAL 阶段实跑通过)"