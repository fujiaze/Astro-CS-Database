#!/usr/bin/env bash
# run_all_audit.sh — SNR-EXP-AUDIT 一键复跑（独立于 run_all.sh）
# 用法:  TMPDIR=/dev/shm/astrocs_snraudit ./audit/run_all_audit.sh
# 结果落 run/reverse_verify/snr_design/audit/（gitignore）
set -u
set -o pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
OUT="$ROOT/run/reverse_verify/snr_design/audit"

mkdir -p "$OUT"
export TMPDIR="${TMPDIR:-/dev/shm/astrocs_snraudit}"
mkdir -p "$TMPDIR"

echo "== SNR-EXP-AUDIT run_all =="
echo "   repo root : $ROOT"
echo "   output    : $OUT"
echo "   TMPDIR    : $TMPDIR"
echo

rc=0
for name in audit_sim_validation.py audit_sp0.py audit_exp3_physical.py audit_exp1245.py; do
  echo "-- $name"
  if python3 "$HERE/$name" --out "$OUT/${name%.py}.json" > "$OUT/${name%.py}.log" 2>&1; then
    echo "   OK  -> $OUT/${name%.py}.json"
  else
    echo "   FAIL (see $OUT/${name%.py}.log)"
    tail -20 "$OUT/${name%.py}.log"
    rc=1
  fi
done

echo
if [ "$rc" -eq 0 ]; then echo "== all audit experiments OK =="; else echo "== SOME AUDIT EXPERIMENTS FAILED =="; fi
exit "$rc"
