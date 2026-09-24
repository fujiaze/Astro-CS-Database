#!/usr/bin/env bash
# 编译 SCI-RECON-SNR-01 生产链路驱动：直接编译本仓生产源（只读，不改一行）+ 链接已构建的
# phase2 静态库（天光面 / 排异）。沿用 code/build_prod_driver.sh 的既有先例。
# 产物落 run/SCI-RECON-SNR-01/bin/（gitignore），不入库。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$ROOT/run/SCI-RECON-SNR-01/bin}"
mkdir -p "$OUT"
NOISE="$ROOT/lib/algorithms/noise_snr/cpp"
V6="$ROOT/lib/algorithms/integration/v6"
COV="$ROOT/lib/algorithms/coverage"
# -fopenmp: 生产 sdet 的 GOMP 并行区需要；线程数由运行时 OMP_NUM_THREADS 决定，不硬编码。
flock /tmp/astrocs_build_b7.lock g++ -O2 -std=c++17 -Wall -Wno-unused-variable -fopenmp \
  -I "$NOISE/include" \
  -I "$ROOT/lib/algorithms/noise_snr/include" \
  -I "$V6/include" \
  -I "$COV/include" \
  -I "$ROOT/lib/algorithms/star_detection/include" \
  "$HERE/b7_recon_driver.cpp" \
  "$NOISE/src/snr_science.cpp" \
  "$NOISE/src/noise_model.cpp" \
  "$NOISE/src/information_weight.cpp" \
  "$V6/src/weight_chain.cpp" \
  "$ROOT/build/libastrocs_phase2.a" \
  "$ROOT/build/libastrocs_p1_sdet.a" \
  "$ROOT/build/libastrocs_probes.a" \
  "$ROOT/build/libastrocs_common.a" \
  -lpthread -ldl -lm -lgsl -lgslcblas \
  -o "$OUT/b7_recon_driver"
echo "built $OUT/b7_recon_driver"
