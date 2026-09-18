#!/usr/bin/env bash
# L4 重建：带计时的执行器（辅助计时器）
# 用途：跑 12 个 normalize + mosaic，逐步记录 wall time，并收集仓库自带的资源遥测。
# 注意：计时器只做观测，不改任何科学参数；不写死线程数（AGENTS §5）。
set -u
cd '/workspace/Astro CS Database'
export TMPDIR=/dev/shm/astrocs_l4
mkdir -p "$TMPDIR"

L4=run/RELEASE-02/L4-rebuild
BIN=build/astrocs
NORM=$L4/norm
mkdir -p "$NORM"
TIMINGS=$L4/timings.csv
echo 'step,rc,wall_s,user_s,sys_s,maxrss_kb' > "$TIMINGS"

# 用 /usr/bin/time 抓 user/sys/maxrss（若有），否则退化为 bash SECONDS
TIMEFMT='%e,%U,%S,%M'
run_one() {
  local tag="$1"; shift
  local out="$NORM/$tag"
  mkdir -p "$out"
  local t0 t1 rc
  t0=$(date +%s.%N)
  if [ -x /usr/bin/time ]; then
    local res rc
    res=$(/usr/bin/time -f "$TIMEFMT" "$@" > "$L4/logs/$tag.stdout" 2> "$L4/logs/$tag.stderr" ; echo "rc=$?")
    rc=$(echo "$res" | tail -1 | cut -d= -f2)
    local stats; stats=$(tail -1 "$L4/logs/$tag.stderr")
    t1=$(date +%s.%N)
    echo "$tag,$rc,$(echo "$t1 - $t0" | bc),$stats" >> "$TIMINGS"
  else
    "$@" > "$L4/logs/$tag.stdout" 2> "$L4/logs/$tag.stderr"; rc=$?
    t1=$(date +%s.%N)
    echo "$tag,$rc,$(echo "$t1 - $t0" | bc),,," >> "$TIMINGS"
  fi
  echo "[$tag] rc=$rc wall=$(echo "$t1 - $t0" | bc)s"
  return $rc
}

mkdir -p "$L4/logs"
overall0=$(date +%s)
for cfg in run/RELEASE-01/e2e/l4/configs/p1_m42_*_red.json; do
  tag=$(basename "$cfg" .json | sed 's/^p1_m42_//')
  run_one "$tag" "$BIN" normalize --json "$cfg" -y
done

echo '--- 核对产品数必须 == 49，否则中止 ---'
python3 $L4/merge_products.py || { echo 'ABORT: product count gate failed'; exit 2; }

run_one 'mosaic_49' "$BIN" mosaic --json "$L4/mosaic_49.json" -y

overall1=$(date +%s)
echo "TOTAL wall = $((overall1-overall0)) s" | tee "$L4/total_wall.txt"
echo '--- 遥测收集 ---'
for d in "$NORM"/*/; do
  t=$(basename "$d")
  for f in resource_summary.json resource_timeseries.csv worker_balance.csv alloc_samples.csv run_context.json; do
    [ -f "$d$f" ] && cp -f "$d$f" "$L4/logs/${t}.$f" 2>/dev/null
  done
done
echo 'done; see' "$TIMINGS"