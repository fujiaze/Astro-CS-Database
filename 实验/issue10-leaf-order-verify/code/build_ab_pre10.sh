#!/usr/bin/env bash
# 临时 A/B 构建：把 e8ff4bc9 的 #10 修复在**未提交的工作区**里还原成 k-9，构建出
# "pre-#10 同配置" 二进制用于隔离对照；随后用 git checkout 恢复源码并重建。
# 该还原**永不提交**；main 交付树保持零生产代码修改。
set -euo pipefail
cd "/workspace/Astro CS Database"
F=lib/infrastructure/scheduler/src/module_adapters.cpp
LOG=实验/issue10-leaf-order-verify/build/02-AB-build.log
: > "$LOG"

echo "# [A/B] pre-#10 临时还原 (uncommitted, restored after)" | tee -a "$LOG"
echo "# main SHA: $(git rev-parse HEAD)" | tee -a "$LOG"
echo "# pre-condition: git diff (must be empty for $F)" | tee -a "$LOG"
git diff --stat -- "$F" | tee -a "$LOG"

# 精确替换唯一一行（已在实验前核对：'leaf_order = k;' 全文件仅 1 处）
cp "$F" /tmp/issue10/bin/module_adapters.post.cpp
sed -i 's/^          leaf_order = k;$/          leaf_order = k - 9;/' "$F"
echo "# reverted line now:" | tee -a "$LOG"
grep -n "leaf_order = k" "$F" | tee -a "$LOG"
git diff --stat -- "$F" | tee -a "$LOG"

echo "# building pre-#10 ..." | tee -a "$LOG"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/issue10/bin/acsd_pre10
sha256sum /tmp/issue10/bin/acsd_pre10 | tee -a "$LOG"

echo "# restoring source via git checkout" | tee -a "$LOG"
git checkout -- "$F"
git diff --exit-code -- "$F" && echo "# restore verified clean (git diff empty)" | tee -a "$LOG"
cmp -s "$F" /tmp/issue10/bin/module_adapters.post.cpp && echo "# byte-identical to pre-A/B source" | tee -a "$LOG"

echo "# rebuilding post-#10 (delivered binary) ..." | tee -a "$LOG"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/issue10/bin/acsd_post
sha256sum /tmp/issue10/bin/acsd_post | tee -a "$LOG"
echo "# [A/B] done" | tee -a "$LOG"
