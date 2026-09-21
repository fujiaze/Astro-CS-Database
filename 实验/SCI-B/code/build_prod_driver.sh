#!/usr/bin/env bash
# 编译 SCI-B 仓内实测驱动：生产源 snr_science.cpp（只读）+ 本单元驱动。
# 产物落 run/SCI-402/（gitignore），不入库。构建串行化加锁。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$ROOT/run/SCI-402}"
mkdir -p "$OUT"
flock /tmp/astrocs_build.lock g++ -O2 -std=c++17 -Wall \
  -I "$ROOT/lib/algorithms/noise_snr/cpp/include" \
  "$HERE/prod_snr_driver.cpp" \
  "$ROOT/lib/algorithms/noise_snr/cpp/src/snr_science.cpp" \
  -o "$OUT/prod_snr_driver"
echo "built $OUT/prod_snr_driver"
