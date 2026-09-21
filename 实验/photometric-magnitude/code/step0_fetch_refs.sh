#!/usr/bin/env bash
# SCI-A · 外部参考数据获取 + SHA256 **校验**（唯一需要网络的步骤）
# 来源：SVO Filter Profile Service（HST/WFC3_UVIS2 F657N/F673N/F502N 总系统透过率曲线）
# 校验：与本脚本内钉死的 SHA256 比对；不一致直接失败（防止缓存被替换而静默通过）
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$ROOT/run/SCI-401/data_cache"
mkdir -p "$OUT"
declare -A PIN=(
  [F657N]=2af43d2dec10904ca2a3207cbc74a9ca1f83fe02d35b7bfd97832d032ad745cf
  [F673N]=fdb18eb39936094323b90e20f06cc88c88412ce9a989c43f22e13cf8fdfa598a
  [F502N]=587ef650b0660cb060af58b0267768ecb05d06ef06f17f9d7d19aa8912ac5e3c
)
rc=0
for F in F657N F673N F502N; do
  URL="https://svo2.cab.inta-csic.es/svo/theory/fps/getdata.php?format=ascii&id=HST/WFC3_UVIS2.${F}"
  P="$OUT/svo_HST_WFC3_UVIS2_${F}.txt"
  if [ ! -s "$P" ]; then
    echo "[fetch] ${URL}"
    curl -fsS "${URL}" -o "$P"
  fi
  GOT="$(sha256sum "$P" | cut -d" " -f1)"
  if [ "$GOT" = "${PIN[$F]}" ]; then
    echo "[ok]   ${F}  sha256=${GOT}"
  else
    echo "[FAIL] ${F}  sha256=${GOT}  期望=${PIN[$F]}" >&2
    rc=1
  fi
done
if [ "$rc" -ne 0 ]; then
  echo "外部通带曲线校验失败：缓存被替换或上游曲线更新。请删除 run/SCI-401/data_cache/svo_*.txt 后重跑，并人工核对上游。" >&2
fi
exit "$rc"
