#!/usr/bin/env bash
# L4 重建：带计时 + 逐阶段剖析的执行器
# 采集三层数据：
#   1) 每步 wall/user/sys/maxrss（/usr/bin/time）        -> timings.csv
#   2) 事件流 NODE_START/NODE_END（--events-jsonl）      -> logs/<tag>.events.jsonl （逐阶段 wall_ms）
#   3) 仓库自带资源遥测（run 收尾自动出）                -> logs/<tag>.resource_*     （CPU/RSS/io_wait 时间序列）
#   4) RELEASE-02 sysmon.py 系统利用率时间序列（整机 + runner 进程树） -> logs/sysmon.csv
# stage_profile.py 把 2)+3) 按时间对齐 => Phase1 逐阶段性能表。
# 只观测，不改科学参数；不写死线程数（AGENTS §5）。
set -u
cd '/workspace/Astro CS Database'
export TMPDIR=/dev/shm/astrocs_l4
mkdir -p "$TMPDIR"

L4=run/RELEASE-02/L4-rebuild
BIN=build/astrocs
NORM=$L4/norm
LOGS=$L4/logs
mkdir -p "$NORM" "$LOGS"
TIMINGS=$L4/timings.csv
echo 'step,rc,wall_s,user_s,sys_s,maxrss_kb' > "$TIMINGS"

# ── RELEASE-02 性能探针: 系统监视器随程序同步启动, 结束(含异常退出)自动收尾 ──
# ASTROCS_SYSMON=0 可关闭; ASTROCS_SYSMON_INTERVAL 覆盖采样间隔 (默认 1s)。
# 跟踪 $$ (本 runner) 的整个进程树 ⇒ 覆盖每一步 astrocs 子进程; 不做线程数假设。
SYSMON_PID=""
SYSMON_CSV="$LOGS/sysmon.csv"
sysmon_stop() {
  if [ -n "$SYSMON_PID" ] && kill -0 "$SYSMON_PID" 2>/dev/null; then
    kill -TERM "$SYSMON_PID" 2>/dev/null || true
    wait "$SYSMON_PID" 2>/dev/null || true
    echo "[sysmon] stopped; csv=$SYSMON_CSV"
  fi
}
if [ "${ASTROCS_SYSMON:-1}" = "1" ] && [ -f tools/l4_rebuild/sysmon.py ]; then
  python3 tools/l4_rebuild/sysmon.py --pid $$ \
      --interval "${ASTROCS_SYSMON_INTERVAL:-1}" --tag l4_rebuild \
      --out "$SYSMON_CSV" > "$LOGS/sysmon.stdout" 2>&1 &
  SYSMON_PID=$!
  echo "[sysmon] started pid=$SYSMON_PID out=$SYSMON_CSV"
  trap sysmon_stop EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
fi

run_one() {
  local tag="$1"; shift
  local out="$NORM/$tag"
  mkdir -p "$out"
  local t0 t1 rc stats res
  t0=$(date +%s.%N)
  res=$(/usr/bin/time -f '%e,%U,%S,%M' "$@" > "$LOGS/$tag.events.jsonl" 2> "$LOGS/$tag.stderr"; echo "rc=$?")
  rc=$(echo "$res" | tail -1 | cut -d= -f2)
  stats=$(tail -1 "$LOGS/$tag.stderr")
  t1=$(date +%s.%N)
  echo "$tag,$rc,$(echo "$t1 - $t0" | bc),$stats" >> "$TIMINGS"
  echo "[$tag] rc=$rc wall=$(echo "$t1 - $t0" | bc)s"
  local f
  for f in resource_summary.json resource_timeseries.csv worker_balance.csv alloc_samples.csv alloc_report.json run_context.json; do
    [ -f "$out/$f" ] && cp -f "$out/$f" "$LOGS/$tag.$f"
  done
  if [ -f "$LOGS/$tag.events.jsonl" ] && [ -f "$LOGS/$tag.resource_timeseries.csv" ]; then
    python3 tools/l4_rebuild/stage_profile.py "$LOGS/$tag.events.jsonl" "$LOGS/$tag.resource_timeseries.csv" \
      > "$LOGS/$tag.stage_profile.txt" 2>&1 || true
  fi
  return $rc
}

overall0=$(date +%s)
for cfg in run/RELEASE-01/e2e/l4/configs/p1_m42_*_red.json; do
  tag=$(basename "$cfg" .json | sed 's/^p1_m42_//')
  run_one "$tag" "$BIN" normalize --json "$cfg" -y --events-jsonl
done

echo '--- 闸门：产品数必须 == 49 ---'
python3 tools/l4_rebuild/merge_products.py || { echo 'ABORT: product count gate failed'; exit 2; }

run_one 'mosaic_49' "$BIN" mosaic --json "$L4/mosaic_49.json" -y --events-jsonl

overall1=$(date +%s)
echo "TOTAL wall = $((overall1-overall0)) s" | tee "$L4/total_wall.txt"
echo '--- 汇总热点 ---'
python3 tools/l4_rebuild/hotspots.py || true
[ -f "$SYSMON_CSV" ] && echo "sysmon csv = $SYSMON_CSV"
echo 'done; timings=' "$TIMINGS"