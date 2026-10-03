#!/usr/bin/env bash
# 独立审计三路重做 + 补实验 · P2 跨帧绝对 SNR · 全量复现脚本
# 纪律：纯 Python3 + numpy（补实验另需 scipy.special.erf）；seed 写死于各脚本；
#       全部结果落各自的 canonical 快照目录 results/<route子目录>/（与入库布局同路径；
#       P2 订正：此前 18 个脚本写 results/<name>.json 顶层 ⇒ 产物与入库快照路径不一致）；
#       单脚本耗时跨 3 个量级（0 s ~ 442 s，随并发负载变动）：route1 多数 0–170 s，
#       route2 有单脚本 >7 min，全量按 1 h 量级预留；零仓库 import。
#       一键全量若被打断：已完成脚本的产物仍是有效快照（逐脚本独立写入）。
set -euo pipefail
cd "$(dirname "$0")"
# 运行结果归档层已移除 ⇒ 干净克隆上本目录下 results/ 不存在。
# 注意各路线脚本以 __file__ 锚定，落点是 <本目录>/results/<route>/（不是单元级
# ../results/），入口按同一相对位置把四棵目录树建出来。
mkdir -p results/route1 results/route2 results/route3 results/supplement_control_variance

echo "== route1 (seed 20260926) =="
for s in route1/*.py; do echo "--- $s"; python3 "$s"; done

echo "== route2 (seed 20260926) =="
for s in route2/*.py; do echo "--- $s"; python3 "$s"; done

echo "== route3 (seed 20260926) =="
# exp11 是 P2-M5 闭环脚本：需要 g++ 与 testdata M42 帧、单轮约 8 min（其余 route3 脚本 1-10 s）。
# 用 EXP11_SKIP=1 可跳过（跳过即不产生 P2-M5 的闭环读数，见 README「P2-M5 闭环脚本」一节）。
for s in route3/*.py; do
  if [ "$(basename "$s")" = "exp11_frozen_operator_transfer.py" ] && [ "${EXP11_SKIP:-0}" = "1" ]; then
    echo "--- $s (SKIP: EXP11_SKIP=1)"; continue
  fi
  echo "--- $s"; python3 "$s"
done

echo "== supplement_control_variance (seed 20260601/05, spotcheck 20260602-04) =="
for s in supplement_control_variance/*.py; do echo "--- $s"; python3 "$s"; done

echo "== done: 36 scripts（35 个路线原件 + exp11 P2-M5 闭环；exp11 另需 g++ 与 testdata M42 帧）=="
