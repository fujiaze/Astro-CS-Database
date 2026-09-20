#!/usr/bin/env bash
# λs 扫描（链接真实 upm.cpp）。SW_W=4 SW_MI=60 SW_LAM="..." bash sweep_all.sh <场景名...>
set -u
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/astrocs_lambda
S=$TMPDIR/upm_sweep
R=run/reverse_verify/smooth_lambda
LAM="0,1e-3,0.01,0.03,0.1,0.3,1,10,1000"
W=4; MI=60
if [ -n "${SW_W-}" ]; then W=$SW_W; fi
if [ -n "${SW_MI-}" ]; then MI=$SW_MI; fi
if [ -n "${SW_LAM-}" ]; then LAM=$SW_LAM; fi
for name in "$@"; do
  ( nice -n 5 $S --in $R/$name.upmb --out $R/sw_$name --lambdas "$LAM" --workers $W --max-iter $MI --tag $name \
      >> $R/sw_$name.log 2>&1 ; echo "SWEEP_DONE $name rc=$?" ) &
done
wait
echo ALL_SWEEPS_DONE
