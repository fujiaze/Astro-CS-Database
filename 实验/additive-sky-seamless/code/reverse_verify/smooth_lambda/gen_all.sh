#!/usr/bin/env bash
# 生成全部合成场景（并行）。用法: bash gen_all.sh
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # 本实验目录（从脚本自身位置推导）
cd "$HERE/../../../../.."                               # 仓库根
export TMPDIR=/dev/shm/astrocs_lambda
G="$HERE/gen_synth.py"
common="--core-sigma 0.05 --core-amp 3.0e5 --core-ra 83.82 --core-dec -5.39"
# A: 光子散粒噪声主导（tex=0；天光逐帧差 0.5% 天空；read/dark/gain 物理）
A="--sky-base 1.0e4 --gain 2.0 --read-e 5.0 --dark-e 3.0 --tex-mad 0.0"
# B: 生产实测尺度（patch 空间 MAD 18% 天空；逐帧天光差 14% 天空）
B="--sky-base 1.0e4 --gain 2.0 --read-e 5.0 --dark-e 3.0 --tex-mad 0.18"

run() { name=$1; shift; python3 $G --out "$name" "$@" > run/reverse_verify/smooth_lambda/$name.gen.log 2>&1 && echo "OK $name" || echo "FAIL $name"; }
export -f run 2>/dev/null || true

( run A_base   --scene A_base   $common $A --sky-off-sigma 50 --sky-grad-sigma 50 ) &
( run A_vary   --scene A_vary   $common $A --sky-off-sigma 50 --sky-grad-sigma 50 --core-mode varying --core-var 0.10 ) &
( run A_nosky  --scene A_nosky  $common $A --no-sky ) &
( run A_nocore --scene A_nocore $A --sky-off-sigma 50 --sky-grad-sigma 50 --no-core ) &
( run A_tau    --scene A_tau    $common $A --sky-off-sigma 50 --sky-grad-sigma 50 --tau-sigma 0.02 ) &
( run A_scale  --scene A_scale  $common $A --sky-off-sigma 50 --sky-grad-sigma 50 --scale 1.0e9 ) &
( run B_prod   --scene B_prod   $common $B --sky-off-sigma 1400 --sky-grad-sigma 700 --tau-sigma 0.02 ) &
( run B_nosky  --scene B_nosky  $common $B --no-sky --tau-sigma 0.02 ) &
wait
echo GEN_ALL_DONE
ls -la run/reverse_verify/smooth_lambda/*.upmb
