#!/bin/bash
# COMPRESS-01: 构建基准小工具 (纯 C, 链接系统 libzstd / libcfitsio; 不触碰仓库构建树)
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p bin logs
echo "== gcc zstd_bench =="
gcc -O2 -o bin/zstd_bench code/zstd_bench.c -lzstd
echo "== gcc fits_bench =="
gcc -O2 -o bin/fits_bench code/fits_bench.c -lcfitsio -lm
echo "== ok =="
ls -la bin/
