#!/usr/bin/env bash
# 独立审计三路重做 + 补实验 · P2 跨帧绝对 SNR · 全量复现脚本
# 纪律：纯 Python3 + numpy（补实验另需 scipy.special.erf）；seed 写死于各脚本；
#       单脚本 CPU 秒级；全部结果落各自 results/ 快照目录；零仓库 import。
set -euo pipefail
cd "$(dirname "$0")"

echo "== route1 (seed 20260926) =="
for s in route1/*.py; do echo "--- $s"; python3 "$s"; done

echo "== route2 (seed 20260926) =="
for s in route2/*.py; do echo "--- $s"; python3 "$s"; done

echo "== route3 (seed 20260926) =="
for s in route3/*.py; do echo "--- $s"; python3 "$s"; done

echo "== supplement_control_variance (seed 20260601/05, spotcheck 20260602-04) =="
for s in supplement_control_variance/*.py; do echo "--- $s"; python3 "$s"; done

echo "== done: 35 scripts =="
