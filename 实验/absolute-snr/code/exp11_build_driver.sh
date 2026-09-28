#!/usr/bin/env bash
# 编译 P2-M5 闭环证据驱动：直接编译本仓生产源 weight_chain.cpp（只读，不改一行），
# 无第三方依赖、无静态库（weight_chain.cpp 仅依赖自身头 + std）。
# 产物落 run/FINAL-07/审核包/科研审查/P2_订正/evidence/prod/（gitignore 的过程产物区）。
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$ROOT/run/FINAL-07/审核包/科研审查/P2_订正/evidence/prod}"
mkdir -p "$OUT"
flock /tmp/astrocs_build_exp11.lock g++ -O2 -std=c++17 -Wall \
  -I "$ROOT/lib/algorithms/integration/phase2_integrate/include" \
  "$HERE/exp11_recon_driver.cpp" \
  "$ROOT/lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp" \
  -o "$OUT/exp11_recon_driver"
echo "built $OUT/exp11_recon_driver"
