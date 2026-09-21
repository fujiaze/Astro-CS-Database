#!/usr/bin/env bash
# FRAME-SNR-CANON (RELEASE-02) 一键复跑
# 判据先行：所有阈值写在各脚本源码里；本脚本只负责按序跑并汇总 PASS/FAIL。
# 用法:  bash 实验/SCI-B/code/reverse_verify/frame_snr/run_all.sh
set -u

REPO="$( cd "$( dirname "${BASH_SOURCE[0]}" )/../../../../.." && pwd )"
EXP="${REPO}/reverse_verify/experiments/frame_snr"
OUT="${REPO}/run/reverse_verify/frame_snr"
PYLIBS="${FRAME_SNR_PYLIBS:-/dev/shm/astrocs_fsnr/frame_snr_canon/pylibs}"

export TMPDIR="${TMPDIR:-/dev/shm/astrocs_fsnr}"
mkdir -p "${TMPDIR}" "${OUT}"

# 外部对拍库（可选；不存在则对应项登记 UNAVAILABLE，不算失败）
if [ -d "${PYLIBS}" ]; then
  export PYTHONPATH="${PYLIBS}${PYTHONPATH:+:${PYTHONPATH}}"
  echo "[env] PYTHONPATH=${PYLIBS}"
else
  echo "[env] 外部对拍库缺失 -> photutils/sep 对拍项将登记 UNAVAILABLE"
fi

RC=0
run() {
  local name="${1}"; shift
  echo ""
  echo "================ ${name} ================"
  timeout 3600 python3 "${EXP}/${1}" "${@:2}"
  local rc=$?
  if [ "${rc}" -eq 0 ]; then
    echo "[OK] ${name}"
  else
    echo "[FAIL] ${name} (exit ${rc})"
    RC=1
  fi
}

run "T8-T12 解析红线"         run_redlines.py
run "P8-P14 物理红线"         run_redlines_physical.py
run "外部对拍 (photutils/sep)" crosscheck_photutils.py
run "p1_snr.json 生产盘点"     inventory_p1_snr.py
run "生产分支判别 (gain-free?)" branch_discriminator.py

echo ""
echo "================ 产物 ================"
ls -la "${OUT}" || true
echo ""
if [ "${RC}" -eq 0 ]; then
  echo "ALL SUITES EXIT 0（具体 PASS/FAIL 见各 JSON 的 pass 字段）"
else
  echo "有套件非零退出（RC=${RC}）"
fi
exit "${RC}"
