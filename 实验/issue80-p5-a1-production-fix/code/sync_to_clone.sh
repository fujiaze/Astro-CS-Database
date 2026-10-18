#!/usr/bin/env bash
# #80 P0-A1 — 把本单拥有写面的文件同步到隔离构建副本（避免被并发 WIP 阻塞构建）。
# 用法: bash sync_to_clone.sh [clone_root]   （默认 /workspace/acsd-a1-clone）
set -euo pipefail
SRC="/workspace/Astro CS Database"
DST="${1:-/workspace/acsd-a1-clone}"
FILES=(
  "lib/algorithms/coverage/src/sky_plane.cpp"
  "lib/algorithms/coverage/src/sky_plane_test_hooks.h"
  "lib/infrastructure/observability/probes/src/dev_diagnostics.cpp"
  "CMakeLists.txt"
  "eng/tests/unit/sky_plane_deflation_test.cpp"
  "eng/tests/unit/run_sky_plane_deflation_test.py"
  "实验/issue80-p5-a1-production-fix/code/run_arms.py"
  "实验/issue80-p5-a1-production-fix/code/a1_penalized_oracle.py"
)
for f in "${FILES[@]}"; do
  if [ -f "$SRC/$f" ]; then
    mkdir -p "$DST/$(dirname "$f")"
    cp "$SRC/$f" "$DST/$f"
  fi
done
echo "synced ${#FILES[@]} paths -> $DST"
