#!/usr/bin/env bash
# ============================================================================
# P3 守恒映射算子 · 三路独立审计 + k_corr 补实验 · 一键复现
# 来源：独立审计/实验重做/P3守恒映射算子/{路线1,路线2,路线3,补实验-k_corr}/code
#       （2026-09 收编入本单元，文件名保持原样；判读见 REPORT_experiment.md）
#
# 固定 seed（写死在各脚本内，无命令行覆盖）：
#   route1  e1..e6        SEED = 20050709
#   route2  exp05/exp08/exp09/exp10  seed = 20260927（exp01/02/03/04/06/07 无随机）
#   route3  exp01..exp08  seed = 20260926（写在脚本头部常数）
#   kcorr   run_scan/run_extra/direct_char  SEED_BASE = 20260816（与正本
#           control_median_mc_test.cpp 同基；各实验组独立偏移见各脚本头）
#
# 依赖：Python >= 3.10 + numpy；单脚本 CPU <= 5 分钟（exp05_sagitta_subdiv.py 约 4 分钟）；
#       全部脚本不 import 仓库任何模块、不联网、不写 results/（只打印 JSON）。
# 既有结果存档于 results/audit/<路线>/（与本脚本输出逐位可对照）。
# ============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "$HERE/../../.." && pwd)/run/healpix-polar-audit-logs"
mkdir -p "$OUT"

run() { name="$1"; script="$2"; timeout 600 python3 "$script" > "$OUT/$name.out" 2>&1 \
        && echo "[ok]  $name" || { echo "[FAIL] $name (see $OUT/$name.out)"; return 1; }; }

# --- 路线 1（seed 20050709）---
run r1_e1 "$HERE/route1/e1_leaf_area_and_scale.py"
run r1_e2 "$HERE/route1/e2_polar_pixel_limit.py"     # ~3-4 min
run r1_e3 "$HERE/route1/e3_circumradius_scan.py"
run r1_e4 "$HERE/route1/e4_flux_conservation.py"
run r1_e5 "$HERE/route1/e5_projection_budgets.py"
run r1_e6 "$HERE/route1/e6_lhuilier_vos.py"

# --- 路线 2（seed 20260927 / 无随机）---
for e in exp01_leaf_area exp02_polar_pixel exp03_flux_conservation exp04_area_operators \
         exp05_projection_budgets exp06_circumradius_margin exp07_polar_sagitta_ladder \
         exp08_coverage_quantization exp09_sum_vs_per_leaf_criteria exp10_chain_usecase; do
  run "r2_$e" "$HERE/route2/$e.py" || true
done

# --- 路线 3（seed 20260926）---
for e in exp01_leaf_area exp02_polar_limit exp03_weight_conservation exp04_circumradius \
         exp05_sagitta_subdiv exp06_projection_budget exp07_quantization exp08_scale_constant; do
  run "r3_$e" "$HERE/route3/$e.py" || true
done

# --- k_corr 补实验（SEED_BASE 20260816）---
run k_run_scan   "$HERE/kcorr/run_scan.py"       # G0-G6 主组 ~20 s
run k_run_extra  "$HERE/kcorr/run_extra.py"      # G0b/G3b/G7 ~60 s
run k_direct     "$HERE/kcorr/direct_char.py"    # 400k 直接定征 ~10 s
run k_tables     "$HERE/kcorr/read_tables.py"    # 由 JSON 重生成汇总表

echo "---- 全部日志：$OUT ----"
echo "对照存档：实验/healpix-polar/results/audit/{route1,route2,route3,kcorr}/"
