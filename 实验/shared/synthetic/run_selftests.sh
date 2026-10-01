#!/usr/bin/env bash
# 合成数据物理链自检 —— 四个组件（m16_mask / m16_scene / m16_sampling / noise_selftest）。
#
# 本脚本是实验单元「一键复现」的公共前置步：它先证明合成器链本身可信
# （能红能绿），再让单元去跑真实验。
#
# 各组件「自检通过」各自意味着什么（口径随判据面变化，勿沿用旧说法）：
#   * noise_selftest = 18 个用例。**A/B 系列用独立随机数原语重实现**，验的是**方法**
#     （分布/口径是否正确），**不经过生产代码**；**C1–C6 直接调生产实现**
#     （NM.expose / NM.sky_surface_e_per_s / 解析一阶矩），验的是**代码**。
#     两者不可互相替代 ⇒ 全绿**不等于**「生产代码已被验证」，生产面的证据只来自
#     C 系列与两个 m16 组件。
#   * m16_mask / m16_scene / m16_sampling --selftest 直接跑本模块的生产函数；其中
#     m16_sampling 的 **V6（存活底图残差预算）走生产平滑 `_smooth_canvas`**、
#     **V10 走真实 FITS 加载路径 `load_canvas`**；m16_scene 的 **V7 覆盖面亮度出口**。
#   * 故「自检通过」= 生产面与独立面同时自洽，不是「代码自洽」这一 weaker 命题。
#
# 运行前提：m16_sampling 的 V6/V10 读真实模板 testdata/HST_M16/*.drz.fits，该目录被
# .gitignore 排除。模板缺失时这两条**记红**（不静默跳过、不设 waiver 开关）——
# 这是「前提不成立」的正确读数，由运行者补齐数据。
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

echo "[独立+生产] 噪声合成器自校验（A/B 验方法 12 例 + C1–C6 验代码 6 例）"
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
