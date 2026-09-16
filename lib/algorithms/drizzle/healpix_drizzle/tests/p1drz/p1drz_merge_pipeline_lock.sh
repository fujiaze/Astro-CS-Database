#!/usr/bin/env bash
# ============================================================================
# P22 DRIZZLE-PAR · 归约流水线 (scratch pool) 线程预算不变式回归锁
# ----------------------------------------------------------------------------
# 覆盖 P22 新增的“池化 + 待归约槽”归约流水线: 用 *高瘦* 帧 (宽 x 高 = 256x1024,
# 64 个 stripe >> 线程预算) 迫使池在“无空闲 map -> 合并 pending 槽”路径上反复
# 切换, 并在 FP32/FP64 两种精度、taskset 1/2/4/8/16 预算、每预算两轮重复下,
# 要求逐 leaf 转储 (.canon) 与产物科学载荷 (.norm.hiss) sha256 完全一致。
#
# 与 P15a 的 p1drz_taskset_invariance 互补:
#   * 该锁用正方形 256x256 (仅 16 stripe) 只覆盖“每线程一 stripe”的浅路径;
#   * 本锁用 64 stripe + 极小预算 (1) 强制同一线程连续认领并即时合并多个 stripe,
#     覆盖池耗尽/归还、跨线程合并归属、以及 repeat 稳定性。
# 用法: p1drz_merge_pipeline_lock.sh <probe 可执行> <工作目录>
# ============================================================================
set -u
PROBE="${1:?probe path}"; WORK="${2:?work dir}"
if ! command -v taskset >/dev/null 2>&1; then
  echo "[SKIP] taskset 不可用 (非 Linux 或无 util-linux)"; exit 0
fi
aff=$(taskset -pc $$ 2>/dev/null | sed "s/.*: //") || true
if [ -z "${aff:-}" ]; then echo "[SKIP] 无法读取 affinity"; exit 0; fi
cpus=$(python3 - "$aff" <<'PY'
import sys
s = sys.argv[1].strip()
out = []
for part in s.split(","):
    part = part.strip()
    if "-" in part:
        a, b = part.split("-"); out += list(range(int(a), int(b) + 1))
    elif part:
        out.append(int(part))
print(" ".join(map(str, out)))
PY
)
read -r -a CPULIST <<< "$cpus"
n=${#CPULIST[@]}
if [ "$n" -lt 2 ]; then echo "[SKIP] 可用 CPU 少于 2 (n=$n)"; exit 0; fi
rm -rf "$WORK"; mkdir -p "$WORK"
fail=0
for prec in fp32 fp64; do
  hashes=(); canons=()
  for b in 1 2 4 8 16; do
    [ "$b" -gt "$n" ] && continue
    sel=$(printf "%s," "${CPULIST[@]:0:$b}"); sel=${sel%,}
    for rep in r1 r2; do
      tag="$WORK/${prec}_b${b}_${rep}"
      if ! taskset -c "$sel" "$PROBE" "$tag" 0 256 256 1024 5000 20 20260915 "$prec" \
           > "$tag.log" 2>&1; then
        echo "[FAIL] $prec 预算 $b $rep 运行失败"; tail -5 "$tag.log"; exit 1
      fi
      [ "$rep" = r1 ] || continue
      hashes+=("$(sha256sum "$tag.norm.hiss" | awk '{print $1}')")
      canons+=("$(sha256sum "$tag.canon" | awk '{print $1}')")
      thr=$(grep -o "threads=[0-9]*" "$tag.log" | head -1)
      echo "$prec budget=$b cpus=$sel $thr norm.hiss=${hashes[-1]} canon=${canons[-1]}"
    done
    # repeat 一致性
    if ! cmp -s "$WORK/${prec}_b${b}_r1.norm.hiss" "$WORK/${prec}_b${b}_r2.norm.hiss"; then
      echo "[FAIL] $prec 预算 $b 重复运行 .norm.hiss 不一致"; fail=1
    fi
  done
  first="${hashes[0]}"; firstc="${canons[0]}"
  for h in "${hashes[@]}"; do [ "$h" != "$first" ] && { echo "[FAIL] $prec 跨预算 .norm.hiss 不一致"; fail=1; }; done
  for c in "${canons[@]}"; do [ "$c" != "$firstc" ] && { echo "[FAIL] $prec 跨预算 .canon 不一致"; fail=1; }; done
  [ "$fail" -eq 0 ] && echo "[PASS] $prec: taskset 1..16 x r1/r2 .norm.hiss=$first .canon=$firstc"
done
[ "$fail" -eq 0 ] || exit 1
echo "[PASS] p1drz_merge_pipeline_lock: 池化归约流水线线程预算不变式成立"
