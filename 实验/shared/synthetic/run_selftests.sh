#!/usr/bin/env bash
# 合成数据物理链自检 —— 五个组件的独立方法自校验。
#
# 本脚本是实验单元「一键复现」的公共前置步：它先证明合成器链本身可信
# （能红能绿），再让单元去跑真实验。每个组件都自实现独立随机数原语或独立参考
# 实现，与被测实现三方对照，因此「自检通过」证明的是分布与口径正确，而不是
# 代码自洽。
#
# 用法：
#   bash 实验/shared/synthetic/run_selftests.sh          # 跑全部
#   bash 实验/shared/synthetic/run_selftests.sh m16      # 只跑名字含 m16 的
#
# 退出码：全部通过 0；任一组件判红 1（红灯不以 waiver 覆盖）；组件缺失 2。

set -u -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${PYTHON:-python3}"
FILTER="${1:-}"

pass=0
fail=0
missing=0
declare -a FAILED=()

run_one() {
  local name="$1"; shift
  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then return 0; fi
  printf '  %-22s ' "$name"
  local start end
  start=$(date +%s)
  if ( cd "$HERE" && "$@" ) >/tmp/acsd_selftest_"$name".log 2>&1; then
    end=$(date +%s)
    printf 'PASS  (%ss)\n' "$((end - start))"
    pass=$((pass + 1))
  else
    end=$(date +%s)
    printf 'RED   (%ss)  日志 /tmp/acsd_selftest_%s.log\n' "$((end - start))" "$name"
    fail=$((fail + 1))
    FAILED+=("$name")
  fi
}

echo "== 合成数据物理链自检 =="
echo "目录：$HERE"
echo

echo "[1/3] 有效域掩膜生成器"
run_one m16_mask            "$PY" "$HERE/m16_mask.py" --selftest

echo "[2/3] 物理前向渲染（PHOTFLAM 一手标定 → 期望率面）"
run_one m16_scene           "$PY" "$HERE/m16_scene.py" --selftest

echo "[3/3] 真实信号模板 → 仿真采样帧"
run_one m16_sampling        "$PY" "$HERE/m16_sampling.py" --selftest

echo "[独立] 噪声合成器的独立方法自校验（独立 RNG 原语三方对照）"
if [ -f "$HERE/noise_selftest.py" ]; then
  run_one noise_selftest   "$PY" "$HERE/noise_selftest.py"
else
  printf '  %-22s MISSING\n' "noise_selftest.py"
  missing=$((missing + 1))
fi

echo
echo "== 汇总 =="
echo "  通过 $pass / 红灯 $fail / 缺失 $missing"
if [ "$fail" -ne 0 ]; then
  echo "  红灯组件：${FAILED[*]}"
  echo "  结论：物理链自检未通过，实验单元的读数不可作为证据。"
  exit 1
fi
if [ "$missing" -ne 0 ]; then
  echo "  结论：存在缺失组件，自检面不完整。"
  exit 2
fi
echo "  结论：物理链自检全绿，实验读数可作为证据。"
exit 0
