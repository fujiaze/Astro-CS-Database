#!/bin/bash
# COMPRESS-01 战线一主基准 (第二批): shuffle 裁剪档位 + 整文件 + FITS 原生压缩
set -uo pipefail
cd "$(dirname "$0")/.."
export COMPRESS01_TMP=/tmp
mkdir -p logs evidence

echo "[2'/5] zstd 数据段 shuffle=4 (裁剪档位 -1,1,3,9,19; level>=12 在 shuffle 下过慢且收益为零)"
timeout 3600 ./bin/zstd_bench data/*.bin --levels -1,1,3,9,19 --reps-c 3 --reps-d 5 --shuffle 4 \
  --csv evidence/zstd_tiles_shuf4.csv 2> logs/zstd_tiles_shuf4.err
echo "  rc=$? rows=$(wc -l < evidence/zstd_tiles_shuf4.csv)"

echo "[3/5] zstd 真实整 .fits 文件 (含头/填充)"
timeout 3600 ./bin/zstd_bench $(cat files_real_fits.txt) --levels -7,-5,-3,-1,1,3,5,9,12,15,19 --reps-c 5 --reps-d 9 --shuffle 0 \
  --csv evidence/zstd_wholefits.csv 2> logs/zstd_wholefits.err
echo "  rc=$? rows=$(wc -l < evidence/zstd_wholefits.csv)"

echo "[4/5] FITS 原生压缩 (cfitsio = fpack 内核), tile=512x512"
timeout 3600 ./bin/fits_bench data/*.bin --codecs RICE_1,RICE_1_q0,GZIP_1,GZIP_1_q0,GZIP_2,GZIP_2_q0 \
  --tile 512x512 --reps-c 5 --reps-d 9 --csv evidence/fits_codecs.csv 2> logs/fits_codecs.err
echo "  rc=$? rows=$(wc -l < evidence/fits_codecs.csv)"

echo "[5/5] FITS 原生压缩, tile=default (fpack 默认行瓦片)"
timeout 3600 ./bin/fits_bench data/*.bin --codecs RICE_1,GZIP_1,GZIP_1_q0,GZIP_2_q0 \
  --tile default --reps-c 5 --reps-d 9 --csv evidence/fits_codecs_defaulttile.csv 2> logs/fits_defaulttile.err
echo "  rc=$? rows=$(wc -l < evidence/fits_codecs_defaulttile.csv)"

echo "ALL DONE"
