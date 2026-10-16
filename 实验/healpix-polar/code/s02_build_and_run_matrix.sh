#!/usr/bin/env bash
# build_and_run_s02.sh — S02 修前/修后成对复跑（只读链接既有构建；不做 git 写操作）
#
# 原理：同一份探针源码 probe_p3_s02.cpp 分别链接
#   · 修前目标文件 before.o（来自 git HEAD 的 spherical_overlap.cpp 快照）
#   · 修后目标文件 after.o （来自工作树快照，含 P3-NORM-01）
#   · 负例     after.o + ACSD_DRZ_P3_FAULT=legacy_cross_normal
# 目标文件显式列在归档之前 ⇒ 归档里的同名成员不会被拉入，读数即该快照的真实行为。
#
# 用法： bash run/ACSDSDOC-02/S02/code/build_and_run_s02.sh
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$ROOT"
OUT=run/ACSDSDOC-02/S02/results
CODE=run/ACSDSDOC-02/S02/code
BUILD=build/linux-sci
mkdir -p "$OUT"

INC=(-I lib/algorithms/drizzle/healpix_drizzle
     -I lib/infrastructure/aio/include -I lib/infrastructure/aio/src
     -I lib/infrastructure/aio/third_party/cfitsio
     -I lib/algorithms/shared -I lib/algorithms/shared/healpix
     -I lib/include -I lib/algorithms/shared/include
     -I lib/algorithms/shared/crypto -I lib/third_party
     -isystem lib/algorithms/drizzle/healpix_drizzle)
DEF=(-DAIO_ENABLE_FITS -DAIO_ENABLE_HEALPIX -DHP_DRIZZLE_STATIC)
FLAGS=(-O3 -DNDEBUG -std=gnu++17 -fopenmp)
LIBS=("$BUILD/libacsd_drizzle.a" "$BUILD/libacsd_hips.a" "$BUILD/libacsd_aio.a" "$BUILD/libacsd_common.a")

echo "[S02] git HEAD      = $(git rev-parse HEAD)"
echo "[S02] libacsd_drizzle.a mtime = $(stat -c %y "$BUILD/libacsd_drizzle.a")"
echo "[S02] source sha256:"
sha256sum "$CODE/spherical_overlap_prepatch_e31b2be66.cpp" "$CODE/spherical_overlap_patched_snapshot.cpp" "$CODE/probe_p3_s02.cpp"
echo "[S02] compile before/after/probe objects"
g++ "${FLAGS[@]}" "${DEF[@]}" "${INC[@]}" -c "$CODE/spherical_overlap_prepatch_e31b2be66.cpp" -o "$OUT/obj_before_spherical_overlap.o"
g++ "${FLAGS[@]}" "${DEF[@]}" "${INC[@]}" -c "$CODE/spherical_overlap_patched_snapshot.cpp"   -o "$OUT/obj_after_spherical_overlap.o"
g++ "${FLAGS[@]}" "${DEF[@]}" "${INC[@]}" -c "$CODE/probe_p3_s02.cpp" -o "$OUT/obj_probe_p3_s02.o"

echo "[S02] link"
g++ -O2 -std=c++17 "$OUT/obj_probe_p3_s02.o" "$OUT/obj_before_spherical_overlap.o" "${LIBS[@]}" -o "$OUT/probe_p3_s02_before" -lzstd -lz -fopenmp
g++ -O2 -std=c++17 "$OUT/obj_probe_p3_s02.o" "$OUT/obj_after_spherical_overlap.o"  "${LIBS[@]}" -o "$OUT/probe_p3_s02_after"  -lzstd -lz -fopenmp

echo "[S02] run before (OMP_NUM_THREADS=1)"
OMP_NUM_THREADS=1 "$OUT/probe_p3_s02_before" "$OUT/results_before.json" before 2> "$OUT/results_before.stderr"
echo "[S02] run after  (OMP_NUM_THREADS=1)"
OMP_NUM_THREADS=1 "$OUT/probe_p3_s02_after"  "$OUT/results_after.json"  after  2> "$OUT/results_after.stderr"
echo "[S02] run negative: ACSD_DRZ_P3_FAULT=legacy_cross_normal"
ACSD_DRZ_P3_FAULT=legacy_cross_normal OMP_NUM_THREADS=1 "$OUT/probe_p3_s02_after" "$OUT/results_negative_legacy_cross.json" negative_legacy_cross 2> "$OUT/results_negative.stderr"

echo "[S02] sha256 产物："
sha256sum "$OUT/results_before.json" "$OUT/results_after.json" "$OUT/results_negative_legacy_cross.json" "$OUT/probe_p3_s02_before" "$OUT/probe_p3_s02_after"
echo "[S02] done"
