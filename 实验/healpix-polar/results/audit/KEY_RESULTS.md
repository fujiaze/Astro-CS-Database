# P3 关键结果 JSON 汇总（注明来源路线）

本目录是三路独立审计 + k_corr 补实验的关键机器可读结果存档（2026-09 收编，
原文件名保持原样）。每个条目注明：来源路线、生成脚本、核心读数、对应台账裁决编号。
判读与置信度见 `REPORT_experiment.md`；数值一律以各 JSON 原文为准，本文件是索引不是改写。

## route1/（seed 20050709，code/audit/route1/）

| 文件 | 来源脚本 | 核心读数 | 台账 |
|---|---|---|---|
| e1_leaf_area_and_scale.json/.txt | e1_leaf_area_and_scale.py | |J|/(π/3) 偏差 ≤9.2e-10；N=256 面积互校 9.1e-11；负例 ε=5e-4 注入度量 5.000e-4≠0；211076.28514206142″ 求值、禁抄值相对差 −1.9749e-4、nside 决策窗演示（finest=105527.7″：真值 nside=4，禁抄值 nside=2） | A-P3-01/09 |
| e2_polar_pixel_limit.json/.txt | e2_polar_pixel_limit.py | 极像元亏缺极限 −9.968368384e-2（N=256 差 2.49e-6，O(1/N²)）；deficit×N² 0.058710→0.104386；N≥32 |rel|>1% 恒 16；环带闭合 ≤2.0e-14 | D-09、A-P3-01 |
| e3_circumradius_scan.json/.txt | e3_circumradius_scan.py | 中心=相邻 ring 纬度中点口径：1.13233→1.12839（N=4→64，单调降，最坏 dec +88.97°）——**该值经 D-01 改记账为极冠叶对角常数 2/√π，不是外接半径** | D-01 |
| e4_flux_conservation.json/.txt | e4_flux_conservation.py | drop 归一 Σ_p w=1：max|S/B0−1|=4.4e-16（pf∈{1.0,0.8,0.6,0.5}）；守恒残差 −1.0e-14…−5.2e-15；D_p 分母负例 1/pf² 逐位（1.5625/2.777778/4.0）；方差二次律比值 9.000000；量化 |r|≤0.5/q 全枚举成立（越界 1.86e-17），反方向 S→0.5/255⁺ r→−1 | A-P3-05、D-03 |
| e5_projection_budgets.json/.txt | e5_projection_budgets.py | gnomonic 逐点 sec³ 律精确（pointwise/1.5ρ_max²=1.000000）；方形积分比 0.5000；切平面正交支实测 −0.2512·θ²（θ=1e-3 ⇒ −2.51204e-7，log-log 斜率 2.000） | A-P3-02/03/04 |
| e6_lhuilier_vos.json/.txt | e6_lhuilier_vos.py | 随机 2e5 三角中位相对差 8.1e-16；非归一化 λ=1.064：VOS 位移中位 0.1189（族依赖，D-11）、l'Huilier max 2.98e-7 ⇒ 分歧探测器成立 | D-11 |

## route2/（seed 20260927 或无随机，code/audit/route2/）

| 文件 | 来源脚本 | 核心读数 | 台账 |
|---|---|---|---|
| exp01_leaf_area.json | exp01_leaf_area.py | 三写法恒等逐位（|Δ|=0.0）；|J|/(π/3)−1 ≤2.2e-16；负例 π/(4N²) 度量 0.25 | — |
| exp02_polar_pixel.json | exp02_polar_pixel.py | N=256 弦四边形 rel −9.9681e-2、deficit×N² 0.104386（与订正值 0.1043885 相对残差 −2.5e-5，D-09 改写）；全天弦四边形和闭 4π（8.9e-16，A-P3-10） | D-09、A-P3-10 |
| exp03_flux_conservation.json | exp03_flux_conservation.py | Σ_p w_jp−1 = 0.0（逐位）；Σ_p F/Σ_j x−1 = 0.0；A_pixel 归一 pf²=0.6400000000064；D_p 分母 +56.25% | D-03 |
| exp04_area_operators.json | exp04_area_operators.py | VOS×l'Huilier 全天互校最坏 2.50e-12（N=64）；八分体解析 π/2 逐位；非归一化 1.064：VOS +3.947e-2（解析 2·atan(1.064)），l'Huilier 0.0 | D-11 |
| exp05_projection_budgets.json | exp05_projection_budgets.py | gnomonic：逐点 +3ρ²/2 精确、drop 级 bias/ρ_max² 居中 +0.5…偏置 +1.4（区间 [0.5,1.5]）；正交支 −θ_max²/2（200 例全负，θ_max=1e-3 ⇒ −5.0e-7） | A-P3-02/03/04 |
| exp06_circumradius_margin.json | exp06_circumradius_margin.py | **台账口径外接半径 sup = 0.9941→1.0415**（N=4→64 单调升，最坏 |z|=2/3）；1.25/1.0415=1.2002（裕量 ≥20%）；经纬对照格 4.516 反例 | D-01 |
| exp07_polar_sagitta_ladder.json | exp07_polar_sagitta_ladder.py | 极叶矢高 0.06391/0.06394/0.06394·hp_res（尺度不变）；梯子渐近因子 0.275/层；depth=8 残差 3.15e-6·hp_res（保守上界）；sag0 = 0.08012·ρ₁（单位归属 D-02）；禁抄值整数阶梯演示（211034.6″ ⇒ NSIDE=1，真值 ⇒ NSIDE=2） | D-02、A-P3-09 |
| exp08_coverage_quantization.json | exp08_coverage_quantization.py | S=1/510 ⇒ −0.5；q=254 格 sup 0.5/254=1.9685e-3；SNR² 最坏 −0.75；banker's rounding 同点静默丢像素 | A-P3-05 |
| exp09_sum_vs_per_leaf_criteria.json | exp09_sum_vs_per_leaf_criteria.py | 恰保总量注入：求和判据多例精确 0.0（双精度地板），逐叶 0.30–0.90，保守分离 ≥14.88 个数量级 | A-P3-11 |
| exp10_chain_usecase.json | exp10_chain_usecase.py | 全链贯通：Σw 偏差 4.44e-16、通量闭合精确 0、控制点处重建精确 0；**绝对坐标 shoelace/S-H 舍入放大 ~1e5（实测 1.16e-5），质心平移后 4.4e-16** | A-P3-12 |

## route3/（seed 20260926，code/audit/route3/）

| 文件 | 来源脚本 | 核心读数 | 台账 |
|---|---|---|---|
| exp01_leaf_area.json | exp01_leaf_area.py | VOS×l'Huilier 全天穷举（N=2…256，786,432 像元）最大相对差 3.135e-12；ΣA/4π−1 ≤4.4e-16；负例（×1.5 注入/非单位化/序反转）全红 | A-P3-01 |
| exp02_polar_limit.json | exp02_polar_limit.py | 亏缺律系数 0.1043891/N²（解析 π/3·(1−2√2/π)；N=256 实测 0.104386）；|rel|>1% 恒 16（N≥32）；赤道带 7.56e-6 定位负例 | D-09 |
| exp03_weight_conservation.json | exp03_weight_conservation.py | 逐像元完备性 max|Σ_p a_jp/A_drop−1|=6.6e-12；全局 8.2e-15；逐对注入 +9.97e-2（红）vs 全局和 8.2e-15（绿）⇒ 验收必须逐叶 | A-P3-11 |
| exp04_circumradius.json | exp04_circumradius.py | 全天穷举 max(R_c/hp_res)=0.9941/1.0322/1.0415（N=4/16/64，真曲线边 65 点/边采样）；负例经纬格 1.6934 | D-01 |
| exp05_sagitta_subdiv.json | exp05_sagitta_subdiv.py | 极冠 max 矢高 6.383e-2/6.393e-2/6.393e-2·hp_res（Górski §5.3 曲线族口径，尺度不变 0.15%）；细分比渐近 4.0；depth=8 残差 1.153e-6·hp_res（超 15%）；depth_needed=7.971；子午线负例 1.6e-14 | D-02 |
| exp06_projection_budget.json | exp06_projection_budget.py | gnomonic 精确闭合 ratio−1=(1+ρ_c²)^{3/2}−1+θ²/4+O(ρ²θ²)（残差 ≤1.4e-8）；正交支 dev=0.4167·h²（h=1e-3 ⇒ 4.163e-7，注释 "4e-8" 错 10 倍） | A-P3-02/04 |
| exp07_quantization.json | exp07_quantization.py | r_sup=S/(q/255)−1 约定下 |r|≤0.5/q（违反 2.15e-16）；−0.5@1/510；S=1.0 负例 r=0 逐位；q=0 域（S<1/510）须特判 | A-P3-05 |
| exp08_scale_constant.json | exp08_scale_constant.py | 211076.28514206142″ 与逐 N 公式 0 ulp 恒等；传播值 −1.9749e-4；决策翻转窗 (105517.3, 105538.14] 宽 20.84″ | A-P3-09 |

## kcorr/（SEED_BASE 20260816，code/audit/kcorr/）

| 文件 | 来源脚本 | 核心读数 | 台账 |
|---|---|---|---|
| g0_sanity.json / g0b_sanity_fixed.json | run_scan.py / run_extra.py | 恒等几何自检：N=289 k=0.9995±0.0278；N=5 收敛 1.6339±0.007（装备有效，k_gauss(N)≠1 是被测对象性质） | D-08 |
| g1_canonical.json | run_scan.py | 正本几何复现：16 相位×8 seed，k_corr=1.3445±0.0416，range [1.2734,1.4254] 含 1.3883（上沿） | D-08 |
| g2_decomposition.json | run_scan.py | k_corr=1.4146±0.0387 = k_shape(0.9762)×k_geom(1.4492)；最近邻平均相关 0.1275 | D-08 |
| g3_nscan.json | run_scan.py | N∈{5…225} 扫描：N=5 k=2.0497±0.0864（紧凑）/1.5434（远散）；N≥9 shape ≤±5% | D-08 |
| g3b_gauss_ref.json / g3b_n5_hiprec.json / direct_char.json | run_extra.py / direct_char.py | k_gauss(5)=1.6370（400k 直接定征）；Var(median,5)/渐近式=0.9113（渐近式**高估** 9.6%，D-07 方向）；median MAD/σ=0.7461 | D-07/D-08 |
| g4_geometry_scan.json | run_scan.py | ρ×pixfrac 20 档：k_corr 全域 1.00–5.0；ρ=0.707（源≈583″）端 2.5–3.0 ⇒ 冻结 1.4 在声明域内低估约 2 倍 | D-08 |
| g5_frames.json | run_scan.py | 1/2/4 帧 k_corr=1.424/1.338/1.328（多帧不消除相关） | D-08 |
| g6_fit.json | run_scan.py | k_corr(N)=k_inf(1+c/(N−1)) 拟合 k_inf=1.286、c=2.92、残差 8.8%；推荐物理分解式 k_gauss(N)×k_geo | D-08 |
| g7_fh_ratio.json | run_extra.py | F&H 式(8) 算子级复核：R_median=1.273 [1.120,1.586] vs 闭式(10) 1.2408（同量级） | D-03/D-08 |
| summary.csv / tables.md | read_tables.py / read_summary.py | 全部组的机器汇总（由 JSON 重生成） | D-08 |
