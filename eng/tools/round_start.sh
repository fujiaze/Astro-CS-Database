#!/usr/bin/env bash
# 开新一轮运行：回收旧轮次产物 → 建目录 → 写轮次元数据（负责人 2026-09-21 指令）。
# 用法：eng/tools/round_start.sh <ROUND-ID>       例：eng/tools/round_start.sh RELEASE-05
# 说明：run/ 是 gitignore 的临时区，历史轮次会累积到上百 GB；本脚本把"先清旧轮次"变成固定动作。
# 细节与硬护栏见 eng/tools/run_gc.py 头部注释；保留清单见 eng/tools/run_keep.txt。
#
# 路径单源（LINUXMAIN-PATH-01 A）：TOOLS_DIR 由 BASH_SOURCE 派生，ROOT = TOOLS_DIR/../..，
# 回收器与保留清单 = TOOLS_DIR 下的同名文件。原实现把 ROOT 算成 eng/（相对 tools/ 时代少退一级），
# 于是三处同时静默退化：①`python3 tools/run_gc.py` 恰在 eng/ 下命中而"看起来正常"；
# ②轮次目录被建到 eng/run/ 而 run/<ROUND> 从未存在；③run_gc 去读 repo/tools/run_keep.txt
# （不存在）⇒ 保留清单整条被静默忽略。实测（临时夹具）：--apply 删除了清单内明确登记的
# run/RELEASE-04 以及其余全部轮次，且 exit 0 打印"run/NEW-01 就绪"。
set -euo pipefail

# 参数解析（fail-closed）：**未知选项必须点名退出 2，不得静默忽略**。
# 原实现只取 "$1" 作轮次、其余参数一律丢弃 —— 于是 "round_start.sh NEW-01 --dry-run"
# 会**静默执行真实删除**（--dry-run 被当成多余的 $2 丢掉，而 run_gc.py 仍带 --apply）。
# 任何「我以为加了保护开关」的调用都会真的删掉证据；未知开关一律拒绝。
ROUND=""
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    -h|--help)
      echo "用法: eng/tools/round_start.sh <ROUND-ID> [--dry-run]"
      echo "  --dry-run  只预演回收（不删除、不建目录）；去掉它才实际执行"
      exit 0 ;;
    -*) echo "未知选项: $arg" >&2
        echo "用法: eng/tools/round_start.sh <ROUND-ID> [--dry-run]" >&2
        exit 2 ;;
    *) if [ -n "$ROUND" ]; then
         echo "多余的参数: $arg（轮次 ID 只能给一个）" >&2; exit 2
       fi
       ROUND="$arg" ;;
  esac
done
[ -n "$ROUND" ] || { echo "用法: eng/tools/round_start.sh <ROUND-ID> [--dry-run]" >&2; exit 2; }
SELF="${BASH_SOURCE[0]}"
case "$SELF" in
  */*) : ;;
  *) SELF="$(command -v -- "$SELF" || echo "$SELF")" ;;
esac
TOOLS_DIR="$(cd "$(dirname "$SELF")" && pwd)"
ROOT="$(cd "$TOOLS_DIR/../.." && pwd)"
GC="$TOOLS_DIR/run_gc.py"
KEEP="$TOOLS_DIR/run_keep.txt"

# fail-closed 前置断言（ENGINEERING_SPEC §10「锚存活」/「路径不存在即判红」）：
# 任一锚失效 ⇒ 点名后退出 2，绝不带着错的 ROOT 继续（那正是本脚本原来的静默退化）。
die_anchor() { echo "ANCHOR_STALE: $1 $2" >&2; exit 2; }
[ -f "$ROOT/CMakeLists.txt" ] || die_anchor ROOT_SENTINEL "$ROOT/CMakeLists.txt"
[ -f "$GC" ]                  || die_anchor RUN_GC "$GC"
[ -f "$KEEP" ]                || die_anchor KEEP_FILE "$KEEP"

cd "$ROOT"

if [ "$DRY_RUN" = "1" ]; then
  echo "== DRY-RUN：只预演回收（不删除、不建目录）=="
  python3 "$GC" --prune-products --keep "$ROUND" --keep-file "$KEEP"
  echo "== DRY-RUN 结束；去掉 --dry-run 才会实际执行 =="
  exit 0
fi

echo "== 1/3 回收旧轮次产物（保留 $KEEP 与 $ROUND；并回收保留轮次内的 out/ 产品树）=="
# --prune-products：保留清单用的是**族 glob**（PERF-* / E2E-* / P1-* …），命中面很宽，
# 只靠"删未命中轮次"压不住盘——真正的大头是各轮次内部的 run/<轮次>/out/。
# 该开关只删名为 out 的直接子目录，REPORT.md / evidence / results / logs 一律保留。
python3 "$GC" --apply --prune-products --keep "$ROUND" --keep-file "$KEEP"

echo "== 2/3 建立 run/$ROUND =="
mkdir -p "run/$ROUND/logs" "run/$ROUND/evidence"
[ -d "run/$ROUND/logs" ] && [ -d "run/$ROUND/evidence" ] \
  || die_anchor ROUND_DIR "$ROOT/run/$ROUND"

echo "== 3/3 写轮次元数据 =="
if [ ! -f "run/$ROUND/ROUND.md" ]; then
  {
    echo "# run/$ROUND"
    echo
    echo "- 开始: $(date -Iseconds)"
    echo "- commit: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "- 目的: （填写本轮目标）"
  } > "run/$ROUND/ROUND.md"
fi
echo "run/$ROUND 就绪；日志落 run/$ROUND/logs/，证据落 run/$ROUND/evidence/"
