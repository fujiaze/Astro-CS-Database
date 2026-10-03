#!/usr/bin/env bash
# 实验/m42-realdata/code/run_all.sh
# 第五实验单元（真实数据腿）一键复现。全部判据固定 seed，只读既有端到端产物，不重跑三命令。
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
LOGDIR="$ROOT/run/M42-REALDATA-01/logs"
mkdir -p "$LOGDIR"
cd "$HERE"

rc=0
for s in c1_photometry c2_absolute_snr c3_seam_additive c4_leaf_allocation; do
  echo "=== $s ==="
  /usr/bin/time -v timeout 3600 python3 "$s.py" > "$LOGDIR/$s.log" 2>&1
  st=$?
  echo "  exit=$st  log=$LOGDIR/$s.log"
  [ $st -ne 0 ] && rc=$st
done

# --------------------------------------------------------------------------
# 完整性锚：**只校验，不生成**
#
# 为什么不能在本脚本里生成锚：四条腿刚把 results/c[1-4]_*.json 重写了一遍，
# 锚若覆盖这些产物，就等于让锚跟着自己这次的产出走 —— 无论腿跑出什么数、
# 源码怎么被改，锚都会在最后一步被刷新成当前值，**判据结构上永远判绿、
# 没有任何检出力**。原实现（本文件旧第 22-23 行 find ... | sha256sum >
# SNAPSHOT.sha256）正是如此，且它把 .py/.md 也一并纳入，于是任何源码或判据
# 改动同样被静默「重新锚定」。那是自愈锚，不是完整性锚。
#
# 现在的分工：
#   ANCHOR.sha256   冻结锚，覆盖**复现动作不重写**的内容 —— 本单元的判据实现
#                   （code/*.py、code/run_all.sh）与判据正本/文档（docs/*.md、
#                   README.md、REPORT_paper.md）。本脚本只 `sha256sum -c` 校验，
#                   **任何脚本、任何人都不得由复现动作重写它**。只有人明确裁决
#                   「这次改动是有意的」之后，才可手工重新锚定，并须同时留下
#                   改了什么、为什么改的记录。
#   SNAPSHOT.sha256 已删除：它是整单元历史基线，其中第 12–15 行正是四个
#                   results/*.json 的校验和 —— 属"归档同步"性质的机器件，
#                   已随运行结果归档层一并移除，不再是本单元的分母。
#   RUN.sha256      本次运行的**产物记录**（每次跑都重写，落在 results/ 运行期
#                   暂存目录，不入库）。它是记录不是门禁：用来做新旧比对，
#                   不用来给产物背书 —— 产物自己证明不了自己。
#
# 上游输入不在本仓（run/RELEASE-05/vis/** 由 RELEASE-05 腿产出，未入库），
# 因此四条的读数无法在本仓内复现。这是 fail-closed 的前置条件：输入不在 ⇒
# 本次读数不得作为证据，脚本据此判红，而不是静默放行。
# --------------------------------------------------------------------------
cd "$ROOT"
ANCHOR="实验/m42-realdata/code/ANCHOR.sha256"
LOGDIR="$ROOT/run/M42-REALDATA-01/logs"
INPUTS_OK=1
for p in run/RELEASE-05/vis/vis/m42/vis_report.json \
         run/RELEASE-05/vis/configs/p1_m42.json \
         run/RELEASE-05/vis/out/m42_p1_t2/p1_phot.json \
         run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json \
         run/RELEASE-05/vis/out/m42_p2/p2_rejection.json \
         run/RELEASE-05/vis/out/m42_p3/output_phase3.fits; do
  [ -e "$p" ] || { echo "  上游输入缺失：$p"; INPUTS_OK=0; }
done
if [ "$INPUTS_OK" -ne 1 ]; then
  echo "上游输入不齐 ⇒ 四条腿的读数不可复现，本次运行不得作为证据（fail-closed）"
  rc=1
fi

if [ ! -s "$ANCHOR" ]; then
  echo "冻结锚缺失或为空：$ANCHOR ⇒ 本次读数不得作为证据（fail-closed）"
  rc=1
elif sha256sum -c "$ANCHOR" > "$LOGDIR/anchor.log" 2>&1; then
  echo "冻结锚 OK：$ANCHOR（判据实现与判据正本未被改动）"
else
  echo "冻结锚失配 ⇒ 判据实现或判据正本已被改动，本次读数不得作为证据"
  rc=1
fi
grep -v ': 成功$' "$LOGDIR/anchor.log" 2>/dev/null | sed 's/^/   /'

# 本次产物记录（记录，非门禁）
# 归档层移除后 results/ 在干净克隆上不存在；下面的重定向由 shell 在 xargs 之前建立，
# 目录缺失时直接失败，不依赖四条腿是否已跑过（腿可在建目录之前就 fail-closed 退出）。
mkdir -p "实验/m42-realdata/results"
find "实验/m42-realdata/results" -maxdepth 1 -type f -name 'c[0-9]_*.json' -print0 \
  | sort -z | xargs -0 sha256sum > "实验/m42-realdata/results/RUN.sha256"
echo "本次产物记录 -> 实验/m42-realdata/results/RUN.sha256（记录，非门禁）"
exit $rc
