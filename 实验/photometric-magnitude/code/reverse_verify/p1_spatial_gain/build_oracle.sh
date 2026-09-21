#!/usr/bin/env bash
# P1-SPATIAL-GAIN 独立 C++ Oracle 的最小构建入口。
#
# 依据：ENGINEERING_SPEC.md §1「构建系统：唯一根 CMake + presets；不引入第二套构建入口」。
# 原 reverse_verify/CMakeLists.txt（standalone project）随 reverse_verify/ 根条目解散而退役，
# 其唯一 target rv_p1sg_oracle 由本脚本等价承担（g++ -O2 -Wall，无外部依赖）。
# 构建目录落 run/reverse_verify/p1_spatial_gain/（gitignore），不污染主线 build/。
#
#   用法：bash 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/build_oracle.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../../../.." && pwd)"
OUT="${OUT:-$ROOT/run/reverse_verify/p1_spatial_gain}"
mkdir -p "$OUT"
g++ -std=c++17 -O2 -Wall "$HERE/cpp/p1sg_oracle.cpp" -o "$OUT/p1sg_oracle"
echo "built: $OUT/p1sg_oracle"
