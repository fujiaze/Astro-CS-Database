#!/usr/bin/env bash
# run_all.sh — SNR-DESIGN 全部数值实验的一键复跑入口
# 用法:  TMPDIR=/dev/shm/astrocs_snrd ./run_all.sh
# 结果落 run/reverse_verify/snr_design/（gitignore），同时在本目录留一份小 JSON 作为证据。
set -u
set -o pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../../.." && pwd)"
OUT="$ROOT/run/reverse_verify/snr_design"
NORM="$ROOT/run/RELEASE-02/L4-rebuild/norm"

mkdir -p "$OUT"
export TMPDIR="${TMPDIR:-/dev/shm/astrocs_snrd}"
mkdir -p "$TMPDIR"

echo "== SNR-DESIGN run_all =="
echo "   repo root : $ROOT"
echo "   output    : $OUT"
echo "   TMPDIR    : $TMPDIR"
echo

run() {
  local name="$1"; shift
  echo "-- $name"
  if python3 "$HERE/$name" "$@" --out "$OUT/${name%.py}.json" > "$OUT/${name%.py}.log" 2>&1; then
    cp -f "$OUT/${name%.py}.json" "$HERE/${name%.py}.json"
    echo "   OK  -> $OUT/${name%.py}.json"
  else
    echo "   FAIL (see $OUT/${name%.py}.log)"
    tail -20 "$OUT/${name%.py}.log"
    return 1
  fi
}

rc=0
run exp1_weight_penalty.py            || rc=1
FRAME="$NORM/t2_m1_red/calibrated_M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts"
run exp2_sparse_snr_reconstruction.py \
    --frame "$FRAME" \
    --p1-sources "$NORM/t2_m1_red/p1_sources.json" \
    --p1-snr "$NORM/t2_m1_red/p1_snr.json" || rc=1
run exp3_multiframe_weight_penalty.py --norm-dir "$NORM" --tile t2_m2_red --max-tiles 3 || rc=1
run exp4_kriging_scaling.py           || rc=1
run exp5_error_budget.py              || rc=1

echo
if [ "$rc" -eq 0 ]; then echo "== all experiments OK =="; else echo "== SOME EXPERIMENTS FAILED =="; fi
exit "$rc"
