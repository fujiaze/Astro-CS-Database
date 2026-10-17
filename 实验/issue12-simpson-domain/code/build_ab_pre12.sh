#!/usr/bin/env bash
# Issue #12 A/B 构建：把 simpson_integrator.cpp 换成冻结 main 的 pre-#12 版本构建
# acsd（→ /tmp/acsd_pre12），再恢复本轮修复源码重建（→ /tmp/acsd_post12）。
# 注意: 本轮修复**尚未提交**, 故用工作副本备份/恢复, 绝不用 `git checkout`（会丢改动）。
set -euo pipefail
cd "/workspace/Astro CS Database"
SRC=lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp
PRE=实验/issue12-simpson-domain/evidence/pre12_spectrum_integrator.cpp
LOG=实验/issue12-simpson-domain/build/02-AB-build.log
: > "$LOG"
echo "# [A/B] pre-#12 构建 (临时替换, 随后恢复)" | tee -a "$LOG"
echo "# main SHA: $(git rev-parse HEAD)" | tee -a "$LOG"

cp "$SRC" /tmp/issue12_simpson_post.cpp
cp "$PRE" "$SRC"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/acsd_pre12
sha256sum /tmp/acsd_pre12 | tee -a "$LOG"

cp /tmp/issue12_simpson_post.cpp "$SRC"
cmp -s "$SRC" /tmp/issue12_simpson_post.cpp && echo "# 恢复 post-#12 源码" | tee -a "$LOG"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/acsd_post12
sha256sum /tmp/acsd_post12 | tee -a "$LOG"
echo "# [A/B] done" | tee -a "$LOG"
