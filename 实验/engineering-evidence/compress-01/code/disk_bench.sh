#!/bin/bash
# COMPRESS-01: 本机磁盘顺序吞吐实测 (端到端临界带宽的输入)
# 写: dd oflag=direct 2 GiB; 读: dd iflag=direct 2 GiB. 各 3 次取中位.
set -uo pipefail
cd "$(dirname "$0")/.."
mkdir -p tmp logs
F=tmp/disk_probe.bin
SZ=2048   # MiB
echo "== 写 (oflag=direct, 2048 MiB) =="
for i in 1 2 3; do
  sync
  /usr/bin/time -f "  run$i write: %e s" dd if=/dev/zero of="$F" bs=1M count=$SZ oflag=direct 2>&1 | grep -E "run|copied"
done
echo "== 读 (iflag=direct, 2048 MiB) =="
for i in 1 2 3; do
  /usr/bin/time -f "  run$i read: %e s" dd if="$F" of=/dev/null bs=1M iflag=direct 2>&1 | grep -E "run|copied"
done
echo "== 缓冲读 (无 direct, 可能命中 page cache) =="
for i in 1 2; do
  /usr/bin/time -f "  run$i bufread: %e s" dd if="$F" of=/dev/null bs=1M 2>&1 | grep -E "run|copied"
done
rm -f "$F"
