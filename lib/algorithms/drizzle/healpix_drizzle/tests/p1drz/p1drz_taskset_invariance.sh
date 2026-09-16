#!/usr/bin/env bash
# ============================================================================
# P15a DRIZZLE-DET-001 · 线程预算不变式回归锁 (taskset 口径)
# ----------------------------------------------------------------------------
# 同一输入在不同 CPU 份额 (taskset 1/2/4/8/16) 下, drizzle 产物 *科学载荷* 的
# sha256 必须逐位相同 (同一预算重复运行亦然)。
#
# 口径 (重要): 不使用 OMP_NUM_THREADS —— Runtime 的 ScopedOmpWorkerInjection
# 会在节点 execute 作用域内覆盖该 ICV; 这里用 taskset 固定 CPU affinity, 引擎以
# config.threads=0 走 omp_get_max_threads() 读取真实预算。
#
# 产物级比较用 <prefix>.norm.hiss: wall-clock 元数据 (history elapsed=%.3fs)
# 天然不可复现, 不属于科学载荷; 探针归零 elapsedSec 后另写一份供 sha256 比较。
# 用法: p1drz_taskset_invariance.sh <probe 可执行> <工作目录>
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
ok=1; hashes=()
for b in 1 2 4 8 16; do
  [ "$b" -gt "$n" ] && continue
  sel=$(printf "%s," "${CPULIST[@]:0:$b}"); sel=${sel%,}
  if ! taskset -c "$sel" "$PROBE" "$WORK/b$b" 0 256 256 256 5000 20 20260915 fp32 \
       > "$WORK/b$b.log" 2>&1; then
    echo "[FAIL] taskset 预算 $b 运行失败"; tail -5 "$WORK/b$b.log"; exit 1
  fi
  h=$(sha256sum "$WORK/b$b.norm.hiss" | awk "{print \$1}")
  hashes+=("$h")
  thr=$(grep -o "threads=[0-9]*" "$WORK/b$b.log" | head -1)
  echo "budget=$b cpus=$sel $thr norm.hiss=$h"
done
if [ "${#hashes[@]}" -lt 2 ]; then echo "[SKIP] 可比预算数不足"; exit 0; fi
first=${hashes[0]}
for h in "${hashes[@]:1}"; do
  if [ "$h" != "$first" ]; then
    echo "[FAIL] 线程预算不变式被破坏: drizzle 产物科学载荷 sha256 不一致"
    exit 1
  fi
done
echo "[PASS] taskset 1..16 预算 drizzle 产物科学载荷 sha256 一致 ($first)"
