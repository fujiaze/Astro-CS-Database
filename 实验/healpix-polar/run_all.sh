#!/usr/bin/env bash
# ============================================================================
# EXP-07-POLAR 一键复现（固定 seed、逐项 timeout、逐项落日志）
# 不链接、不运行任何产品二进制；不修改生产代码；不写 git。
# 用法:  bash run_all.sh [quick|full]
#   quick: 极点主消融 + 全天扫描 + 负例 + 破门区间（约 15 分钟）
#   full : 追加 chart 原生对照、REC-1 预算扫描、计时、HST 两档（约 60 分钟）
# ============================================================================
set -u
MODE="$1"
if [ -z "$MODE" ]; then MODE=quick; fi
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
UNIT="$HERE"
LOGS="$ROOT/run/EXP-07-POLAR/logs"
BIN="$ROOT/run/EXP-07-POLAR/verify"
mkdir -p "$LOGS" "$BIN"

INC="-I $HERE/code -I $ROOT/lib/algorithms/drizzle/healpix_drizzle -I $ROOT/lib/algorithms/shared"
SRC="$ROOT/lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp $ROOT/lib/algorithms/shared/healpix/healpix_core.cpp"
CXX=g++

build() {
  $CXX -O2 -std=c++17 $INC -o "$BIN/$1" "$HERE/code/$1.cpp" $SRC > "$LOGS/build_$1.log" 2>&1 \
      || { echo "BUILD FAIL $1"; tail -20 "$LOGS/build_$1.log"; exit 1; }
}

for p in p0_selftest p1_rootcause p2_algorithms p3_sweep_neg p4_hst p5_extent p6_leafmap p7_wcs3d p8_hstgrid p9_review p10_seam; do build "$p"; done
echo "build ok"

run() {
  name="$1"; shift
  timeout 5400 /usr/bin/time -v "$@" > "$LOGS/$name.out" 2> "$LOGS/$name.time"
  rc=$?
  echo "[$name] exit=$rc  $(grep -E 'Maximum resident' "$LOGS/$name.time" 2>/dev/null | tr -s ' ')"
  return $rc
}

cd "$BIN"
run p1_t1  ./p1 t1
run p1_t2  ./p1 t2
run p1_t3  ./p1 t3
run p1_t4_pole_full ./p1 t4 -8 8 0.5
run p1_t7  ./p1 t7
run p3_t11 ./p3 t11
run p3_t12 ./p3 t12 "$LOGS/t12_sweep.csv" -4 4 1.0 1e-6
run p3_t13 ./p3 t13
run p5_t17 ./p5 "$LOGS/t17_extent.csv"
run p6_leafmap ./p6 all
run p0_selftest ./p0
run p7_wcs3d ./p7
run p8_hstgrid ./p8
run p9_review ./p9 all
run p9_t28 ./p9 t28
run p9_t29 ./p9 t29
run p10_seam ./p10

if [ "$MODE" = "full" ]; then
  run p2_t8a ./p2 t8a
  run p2_t8b ./p2 t8b
  run p2_t9  ./p2 t9
  run p2_t10 ./p2 t10
  run p4_hst     ./p4 all 2097152
  run p4_hst_n23 ./p4 all 8388608
fi

# 快照必须在所有探针跑完之后生成（否则 code/ 的哈希会与实际不一致）
cp "$LOGS"/*.out "$LOGS"/*.csv "$UNIT/results/" 2>/dev/null
( cd "$UNIT" && sha256sum code/*.h code/*.cpp results/*.out results/*.csv > results/SNAPSHOT.sha256 )
( cd "$UNIT" && sha256sum -c results/SNAPSHOT.sha256 > /dev/null && echo "[snapshot] SNAPSHOT.sha256 校验通过（$(grep -c . results/SNAPSHOT.sha256) 项）" )

echo "---- 关键判据 ----"
grep -h "SUMMARY" "$LOGS"/p3_t12.out "$LOGS"/p3_t13.out "$LOGS"/p4_hst.out "$LOGS"/p4_hst_n23.out 2>/dev/null
