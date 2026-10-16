#!/usr/bin/env bash
# 临时 A/B 构建：把 #11 的 `have_first` 修复在**未提交的工作区**里还原成旧
# `first.empty()` 逻辑，构建出 "pre-#11 同配置" 二进制；随后 git checkout 恢复
# 源码并重建。还原**永不提交**；main 交付树保持零生产代码修改。
set -euo pipefail
cd "/workspace/Astro CS Database"
F=lib/infrastructure/scheduler/src/module_adapters.cpp
LOG=实验/issue11-passband-identity/build/02-AB-build.log
: > "$LOG"
echo "# [A/B] pre-#11 临时还原 (uncommitted, restored after)" | tee -a "$LOG"
echo "# main SHA: $(git rev-parse HEAD)" | tee -a "$LOG"
git diff --stat -- "$F" | tee -a "$LOG"

cp "$F" /tmp/issue11_module_adapters.post.cpp
python3 - <<'PY'
p = "lib/infrastructure/scheduler/src/module_adapters.cpp"
s = open(p, encoding="utf-8").read()
new = """      std::map<std::string, uint64_t> counts;
      std::string first;
      bool have_first = false;  // 未取首帧与已取得空通带是不同状态
      bool same = true;
      for (const auto& fr : frames) {
        const std::string f = fr.value("filter_passband", std::string());
        if (!have_first) {
          first = f;
          have_first = true;
        } else if (f != first) {
          same = false;
        }
        counts[f] += 1;
      }
"""
old = """      std::map<std::string, uint64_t> counts;
      std::string first;
      bool same = true;
      for (const auto& fr : frames) {
        const std::string f = fr.value("filter_passband", std::string());
        if (first.empty()) first = f;
        else if (f != first) same = false;
        counts[f] += 1;
      }
"""
assert s.count(new) == 1, "fixed block not found exactly once"
open(p, "w", encoding="utf-8").write(s.replace(new, old))
print("reverted to pre-#11 logic")
PY
git diff --stat -- "$F" | tee -a "$LOG"

echo "# building pre-#11 ..." | tee -a "$LOG"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/issue11_acsd_pre11
sha256sum /tmp/issue11_acsd_pre11 | tee -a "$LOG"

echo "# restoring source via git checkout" | tee -a "$LOG"
git checkout -- "$F"
git diff --exit-code -- "$F" && echo "# restore verified clean" | tee -a "$LOG"
cmp -s "$F" /tmp/issue11_module_adapters.post.cpp && echo "# byte-identical to pre-A/B source" | tee -a "$LOG"

echo "# rebuilding post-#11 ..." | tee -a "$LOG"
ninja -C build/linux-sci-p5 acsd >> "$LOG" 2>&1
cp build/linux-sci-p5/acsd /tmp/issue11_acsd_post
sha256sum /tmp/issue11_acsd_post | tee -a "$LOG"
echo "# [A/B] done" | tee -a "$LOG"
