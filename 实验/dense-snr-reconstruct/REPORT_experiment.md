# P4 重建稠密 SNR · 实验报告（实验单元）

**单元目录**: 实验/dense-snr-reconstruct/
**科学链位置**: 第 4 点——由稀疏控制点重建稠密信噪比场及其定权消费（最高设计 §2）
**证据源**: 三路互不通信的独立重做之 code/results，原样收录本单元 code/ 与 results/；全部读数引用既有存档 JSON，并由订正轮在**单元副本**上重跑 `bash code/run_all.sh` 逐叶核对（19/19 JSON 除计时字段 `runtime_s` 外全同，见 `run/FINAL-07/logs/P4_F4_reproducibility.out`）。
**复现**: bash code/run_all.sh（纯 CPU，产物与存档 JSON 逐字段同构；本机实测峰值 RSS 0.35 GB，见 `run/FINAL-07/logs/P4_F4_reproducibility.out`）
**判读口径**: `实验/裁决台账.md` D-xx 与 `docs/DISPUTES.md` A-P4-xx 终裁。

---

## 1 假说（事前声明）与判定总表

| # | 假说 | 判定 | 关键读数 | 证据（实验:脚本 → results/） |
|---|---|---|---|---|
| H1 | w = SNR²/F_ref² = 1/σ_F² 逐位恒等（幂次 2 非自由参数，γ=2 = GLS 逆方差最优） | **成立（四路验证）** | 恒等偏差 5.55e-16 / 4.44e-16 / 3.7e-16 / 4.4e-16（2 ulp）；γ=2 方差最小，γ=0 差 26.3×，γ=1 效率 3.73，等权 104.58 | route1/exp_p4_01 → exp_p4_01_weight_optimality.json；route2/exp_P4R2_01 → exp01_weight_identity_gamma.json；route3/exp01 → exp01_weight_identity.json；P2 路线1 exp12（台账 A-P2-11 转写） |
| H2 | 生产默认算子 = natural_bicubic_spline_clip_v1，节点精确复现、收敛阶 −4、钳制必要、正齐次性可红 | **成立** | 节点残差 ≤3.6e-15（门 1e-9）；样条内部阶 −3.97/−4.18；去钳制 min=−161.5 / 出现负 σ；公理偏差 ≤2.8e-15、收缩反例 0.54 | route1/exp_p4_02 → exp_p4_02_interpolators.json；route3/exp03 → exp03_sparse_dense_reconstruction.json；route3/exp05 → exp05_operator_axioms.json |
| H3 | IDW 默认 idw_power=2.0 有标定支撑 | **推翻 → 按 D-05 终裁默认 1.0（配置级；实现侧未落地）** | 含噪（2%/6%）最优 p≈0.5–1（rmse_rel 6.827e-3 / 1.414e-2），p=2 差 2–3×；无噪 p=2 最优（1.639e-3）；误差面光滑无尖峰、p→∞ 退化为最近点。**实现侧现状（P4-M05）**：drizzle 侧求值器已落地 1.0（`snr_evaluator.h:110`、`snr_evaluator.cpp:212,268`）；仍硬写 2.0 的只有 `snr_estimator.cpp:593,730,833`（Phase1 输出模型路径）。配置键与 p* 日志未实现 | route2/exp_P4R2_02 → exp02_idw_params.json；route3/exp04 → exp04_idw_parameters.json；route1/exp_p4_02；缺口清单见 REPORT_paper.md §7.9 |
| H4 | **逐像素**定权面上控制值必须携带源项亮度，否则下游效率损失 | **成立（适用域 = 场在 Δ 尺度上平滑；帧级常数口径上方向相反，§6 第 14 条）** | Oracle：含源项 E=2.22e-16（机器零）、丢源项 1.31e-3、等权 1.78e-2；极端对比 A_full 0.0301577 vs A_bglimit 0.3804647（12.6×，以 `【复现：bash 实验/dense-snr-reconstruct/code/run_all.sh 重跑后读 exp_p4_04_brightness_forward.json 的 arms/extreme_；该读数需重跑取得，当前不可离线核验】*` 存档为准）；零源负例两臂逐位恒等 | route1/exp_p4_04 → exp_p4_04_brightness_forward.json；route2/exp_P4R2_07 → exp07_luminance_chain_negative.json |
| H5 | 三口径（dense/sparse_reconstruct/frame_reconstruct）是同一物理量的三种还原粒度，不构成三种权 | **口径表示成立（同一权公式）；「dense 一律最优」被本轮对照实验否证（P4-M10 已闭环）** | route1 的 frame 臂按**帧级常数铺满**实现（退化的上界对照）：dex-RMSE 0.0295 vs 样条 0.0058、动态范围保持比 0.7525 vs 0.9888，权重公式不变；w 对 m_ref 不变逐位。`dense` 口径本轮已补对照实验（`code/calibers/exp_P4CAL_02_three_calibers_guarded.py`，三类数据 + 对比度扫描 + 守卫臂 `all_pass = true`）：结构场 `E_cell = 0.00903 < E_dense = 0.577 < E_frame = 1.159`，HST 物理前向 `E_cell = 0.206 < E_frame = 0.416 ≪ E_dense = 2.57e5`（样条欠冲），真实帧 `E_cell = 0.171 ≪ E_frame = 11.8 ≪ E_dense = 6.46e4`；该扫描曾给出的「v 跨幅 1.78–235 窗口」已被 M16 物理前向腿否证（窗口内稠密口径照样失效），**不是有效域判据**；替代口径见 §6 第 13 条 ⇒ **dense 口径不是一律最优**；生产帧级口径（帧级 SNR 逆方差链）在单帧内即空间常数权场 = 本实验 `frame` 臂。 **与 route1/exp04 的关系（关键区分）**：exp04 的控制值是**精确的 cell 模型量**（表示上限测试，无估计量噪声）⇒ 其 `E_full = 7.43e-4` 是**表示上限**（控制值精确时的可达下界）；本实验 `dense` 臂的控制值走**生产配方**（8×8 patch `1.4826·MAD` 平方 + cell 内平面拟合）⇒ 估计量噪声使同一算子的 `E` 升到 `0.577`（平滑场）/`2.57e5`（亮源前向）。两者不矛盾：**真实可达效率由控制值精度支配**，`7.43e-4` 不是端到端可达值。 | route1/exp_p4_04（`::arms/A_frame_recon`、`::arms/A_full`）；route3/exp01（H1d：w 比 = 1.0 逐位） |
| H6 | λlo/λhi ≥ 1/16 是保守良态守卫（κ=√(λhi/λlo)=轴比） | **成立** | 放大 1.007/2.025/3.956/7.980 vs 理论 1/2/4/8（0.5% 内）；共线触发率 1.000；各向同性最小 0.330 永不误红 | route2/exp_P4R2_04 → exp04_plane_conditioning.json；route1/exp_p4_03 → exp_p4_03_plane_geometry.json |
| H7 | 分母方差面缓变：白性 lag-1=−1/2、relspread 散粒 1×/PRNU 2×、指纹斜率 1/2/0；分子纯度（无源⇒臂逐位相等） | **成立（但“无源⇒臂逐位相等”是结构断言、非估计器断言，P4-M03）** | lag-1 −0.5007/−0.4995/−0.4984 与 −0.49963±0.00078；散粒比 **0.9240（route2 实测）**/0.99999999（route3 解析档）、PRNU 比 **1.9113（route2 实测）**/1.99067（route3 解析档）；斜率 1.000/2.000/0.000；无源 SNR≡0 精确（两臂同式构造，任何算子都判绿 ⇒ 判别力改由 `code/fix/fix01_metric_E_and_gates.py` 的逐臂估计器版承担） | route2/exp_P4R2_05 → exp05_whiteness_variance_map.json；route3/exp06 → exp06_white_noise_and_purity.json；【复现：实验/dense-snr-reconstruct/code/fix/fix01_metric_E_and_gates.py 生成的 fix01_metric_E_and_gates.json 的 C_rebuilt_gates/C2_zero_source_per_arm_estimator】 |
| H8 | 判据 `E_eff = Var_w/Var_opt − 1`（**唯一口径** `Var_opt = 1/Σ(1/σ_true²)`）有证据资格（能红）且乘性免疫 | **成立（口径按 P4-B02 定案；被否口径 `1/Σw` 登记为无效定义）** | 乘性 σ̂=3.17σ 时 E=−1.1e-16（尺度不变偏差 0，机器零）；平坦+平坦 E=0 精确；错误臂 0.314 判红；打乱 0.6335；被否口径在同臂上判负（−0.9005）且尺度不变偏差 3.75 | route3/exp02 → exp02_metric_E_properties.json；results/fix/fix01…json::A_metric_definition；定义与证明 [推导:docs/derivations.md §7] |
| H9 | 辅助常数链：1.4826 解析恒等、9216 预算自洽、Moffat4=1.230310 闭式（σ 口径 = 模型参数口径 `σ=α/√2`） | **成立** | 1/Φ⁻¹(3/4)=1.4826022185056023（1.50e-16）；9216=(1.44/0.015)² 精确，n_min 带 8905–9494；闭式 1.2303076526 / 独立数值积分 1.2303076507 vs 登记 1.230310（**1.91e-6** = 六位小数圆整量）。他域口径 `σ_g=α/2` ⇒ 1.7399178，恒差 √2、禁止互换 | route2/exp_P4R2_08 → exp08_mad_sigma_budget.json；route2/exp_P4R2_09 → exp09_moffat4_factor.json；[推导:docs/derivations.md §6] |
| H10 | Δ=64 是可实验证伪的科学量 | **豁免（结构性）** | tile_width=512 冻结、512/8 密度口径自洽；科学后果由 Δ/ℓ 判据承载（零噪偏置 128/64=4.06 vs 理论 4，随 Δ² 增长） | route2/exp_P4R2_03 → exp03_delta_grid.json；台账 A-P4-01 |

负例纪律：19 个实验全部含"真值无效应 ⇒ 度量归零/判据失效"负例（等 σ 方案差归零、平坦场全算子归零、零源稠密场 max|SNR|=0、常数数据 MAD=0、高斯轮廓对照 1.6651 等），对应错误臂均判红。**例外与订正（P4-M03）**："无源两臂逐位恒等"属**同式构造的结构断言**（任何算子都判绿），不具判别力；**该门 `no_source_benefit_collapses`（`code/route1/exp_p4_04_brightness_forward.py:282-285`）已在 `REPORT_paper.md` §7.23b 与 `oracle_full_is_optimal` 同级登记为结构恒真门（G08-04 G2 整改 P0-1c）**；另两条被点名的门（亮度跟随门、有源对照）经反例检验同样对算子不敏感。替代判据（算子敏感亮度门 + 逐臂估计器版零源门 + 偏 10% 源模型对照）见 `code/fix/fix01_metric_E_and_gates.py` → `【复现：实验/dense-snr-reconstruct/code/fix/fix01_metric_E_and_gates.py 生成的 fix01_metric_E_and_gates.json】`（正例绿、全局常量/打乱臂红）。

### ⚠ G08-05 R2 B2 订正：本单元**并非**「无恒真门」——「无恒真门」这句话是错的

原文写「对应错误臂均判红——**无恒真门**」，随后只把**两条 exp_p4_04 的门**列为例外。该例外表**严重不全**。经 `code/calibers/`、`code/sim/`、`code/route1–3/` 全量逐处核实，本单元的**结构性恒真门（永不判红）实测计数为 22 处**，其中 **14 处**就在 `calibers/`（7）与 `sim/`（7）——比第二轮复核给出的「7 处」**多一倍**。三条实测恒等式（/tmp 内数值验证，仓内未改动）：

| 恒等式 | 断言 | 实测上界 |
|---|---|---|
| `E_eff(1/v, v) ≡ 0` | 对**任意**正数组 `v` 成立 | 2000 例随机 `v`，`max|E| = 4.44e-16` |
| `E_eff(c·w, v) = E_eff(w, v)` | `E_eff` 对 `w` **按构造齐次** | 3000 例，`worst rel_dev = 1.8e-15`（门容差 1e-8） |
| `E_eff ≥ 0` | Cauchy–Schwarz，`v>0` 恒成立 | 50000 例对抗抽样，`min = +5.8e-7`，从未为负 |

⇒ 凡形如 `E(1/v_true)`、`E(c·w, v)`、`E_dense ≥ E_frame`（`E_frame ≡ 0`）的门**在数学上不可能判红**。代表门（完整 24 处清单见 `docs/TAUTOLOGY_REGISTER.md`）：

- `calibers/exp_P4CAL_01_three_calibers.py:160` `G1_oracle_dense_zero`、`:191` `G3_flat_no_fake_advantage`
- `calibers/exp_P4CAL_02_three_calibers_guarded.py:158,165,173` `G1_oracle_dense_zero`（3 处）、`:166` `G3_flat_frame_zero`、`:167` `G3_flat_dense_no_fake_advantage`
- `sim/exp_sim01_m16_forward_snr_truth.py:303,317` `G1_oracle_dense_zero(_outside)`、`:310` `R2_metric_scale_invariant`、`:337` **`NC-A1_all_calibers_zero`**、`:338,350,351` `NC-A1/A2_oracle_zero`、`NC-A2_frame_zero`
- `route1/exp_p4_01:88` `identity_machine_precision`；`route2/exp_P4R2_01:93`、`exp_P4R2_06:87`、`exp_P4R2_07:126`、`exp_P4R2_08:109`；`route3/exp01:37,80,106,120`、`route3/exp04:106`

**其中 `sim/exp_sim01_m16_forward_snr_truth.py:337` `NC-A1_all_calibers_zero` 最严重**：该臂从不调用 `calibers()`／`patch_mad_var`／`cell_nodes`／`spline2d`，`:331-332` 算出**一个**数 `z = E_eff(1/v_flat, v_flat)`，`:334` 把**同一个变量 `z`** 赋给 `E_eff_frame / E_eff_cell / E_eff_dense / E_eff_shuffled` 四个字段——所谓「四口径对照」是一个数与自己比。估计器在该门下**不可达**。

**实测（10 个注入缺陷：MAD ×1.35／×0.02、nodes +40／×7／符号翻转、spline ×1000／+1e9／常数填充）**：上述恒真门集合 **10/10 全绿**（对注入结构性免疫）；同文件中真正有判别力的门 `G2_dense_beats_frame` **7/10 判红**，`G3_flat_spline_no_undershoot` 在符号翻转下判红——证明这不是「实现有 bug」，而是「这些门按定义就没有判别力」。

**处置（不在本轮代劳，登记为待办）**：把上述 22 处**移出判决、登记为诊断项**，或改接**真实参照量**（对 `calibers()`/`spline2d()`/`patch_mad_var()` 的输出做独立重算，而不是让门去比 `E_eff` 的恒等式）。本轮**未删除任何一条真实读数**，也**未放宽任何阈值**；本节只把「无恒真门」这句不实表述改成如实计数。**删除这些门不是本单能做的**——它们被多条报告引作判别力证据，删除属跨篇改动，须由前台裁决后统一执行。完整 24 处清单（含逐处 file:line、型别与判别力实测）见 `docs/TAUTOLOGY_REGISTER.md`。

## 2 方法

- **三腿模型**：文献腿（一手核验，见 refs.md）/ 实验腿（固定 seed、含负例）/ 理论腿（docs/derivations.md：定权恒等式与 Cauchy–Schwarz 最优性、条件数放大、收敛阶、IDW p→∞ 极限、Δ² 偏置律、白性 −1/2 恒等式）。
- **算子实现**：样条按标准分段基独立实现（路线3，节点复现 1.8e-15 + 收敛阶双重验证）；IDW 含 K 近邻截断与 γ=1e-10 重合点守卫、大 p 对数归一化防溢出；双线性为规则网格对照档。
- **合成数据**：HST 信号模板性质的 Moffat(β=2.5/4) 源面 + 完整物理前向（散粒/读噪/PRNU 项构成同 NOISE_MODEL §5b）、纯解析代数合成（平面 + 弱曲率、GRF）、负例（平坦场、零源、等 σ、常数数据）。
- **度量**：`E_eff = Var_w/Var_opt − 1`（**唯一口径** `Var_opt = 1/Σ(1/σ_true²)`；被否口径 `1/Σw` 登记为无效定义，见 [推导:docs/derivations.md §7] 与 `【复现：实验/dense-snr-reconstruct/code/fix/fix01_metric_E_and_gates.py 生成的 fix01_metric_E_and_gates.json】`）、dex-RMSE、动态范围保持比、log-log 指纹斜率；E 对 σ̂ 的乘性缩放严格免疫 ⇒ 必须与 dex 水平判据成对使用（H8）。

## 3 数据来源

| 路线 | 脚本（code/ 下） | 存档结果（results/ 下） | seed |
|---|---|---|---|
| route1 | exp_p4_01_weight_optimality.py … exp_p4_04_brightness_forward.py（4） | route1/*.json（4） | 20260926 |
| route2 | exp_P4R2_01…09（9） | route2/*.json（9） | 20260926–20261003（每实验一档） |
| route3 | exp01…06（6） | route3/*.json（6） | 20260926 |

| sim | sim/exp_sim01_m16_forward_snr_truth.py（1） | sim/*.json（1） | 20261010（共享场景配方内） |

三路互不通信、全仓库只读、纯 Python+numpy、单实验 CPU ≤5 min；seed 写死于脚本（route1 统一 SEED=20260926 + 偏移派生；route2 按实验日递增；route3 统一 20260926）。【复现：eng/tools/quality/resource_monitor.py 生成的 summary.json】 为关键读数汇总（注明来源路线与字段）。

**哈勃物理仿真腿（第 1 类数据）**：真实 HST M16 F657N drz 帧作**纯信号模板**，经共享物理链
`实验/shared/synthetic/m16_sampling.py → noise_model.expose()` 生成仿真采样帧（768² 帧、0.2″/px、
曝光 300 s、seeing FWHM 3.2 px、天光 0.5 e⁻/s、增益 1.5 e⁻/ADU、读出 5 e⁻、平场 PRNU 1%/低阶 2%/渐晕 5%）。
该链的六环节（源/天光/暗流电子域 Poisson、读出电子域 Gaussian、增益+饱和+量化、平场、天空梯度）自洽性由
**公共前置步** `bash 实验/shared/synthetic/run_selftests.sh` 承担：四个组件独立自校验全跑（m16_mask /
m16_scene / m16_sampling / noise_selftest），实测 4/4 PASS、rc=0、约 2 分 32 秒；**不得只调
`noise_selftest.py`**——它的 12 个用例中只有 A2 调用生产实现且生产臂不进入判词，对生产实现零判别力，
生产面的门禁责任在 `m16_scene --selftest` 与 `m16_sampling --selftest`。本腿依赖该自检为绿。

**真值定义**：SNR_true = (S_e/g)/√((S_e+B_e+D_e)/g² + RN²/g² + 1/12)，S_e/B_e/D_e 取自物理链的逐像素
期望量（与 `noise_model.predicted_variance_adu2` 同式），饱和像元剔除。这是本单元第一次拿到
**逐像素绝对 SNR 真值**——此前 C 臂用的是局部 MAD² 代理。

## 4 结果（判读按台账终裁）

1. **定权恒等式（H1）**：四路独立验证机器精度成立；γ=2 唯一确定（A-P4-02 三路同判，审查"γ=1 保守"不成立）。γ 扫描负例（同方差场景 gain≈1.0125）证明判据非退化。
2. **默认算子（H2）**：natural_bicubic_spline_clip_v1 为生产默认（**按台账 D-10 订正**旧稿"bilinear 是默认"），双线性对照/回退；节点复现 ≤3.6e-15 对 1e-9 门为浮点性质余量而非松容差；"默认档总是最优"不成立——Δ=256 被双线性/IDW 反超，跨 Δ 外推须重验（A-P4-07）。
3. **IDW 默认值（H3）**：**按台账 D-05 终裁 idw_power=1.0**（含噪最优带 0.5–1 上端），K=16 折中、γ<1e-10 数值守卫；**目标形态**为配置化 + 运行日志输出实测 p*（不落盘产品）。**实现侧现状（P4-M05，如实登记）**：drizzle 侧求值器**已与终裁一致**（`snr_evaluator.h:110` 成员初值 1.0；`snr_evaluator.cpp:212,268` 的 ≤0 兜底回落 1.0）；仍硬写 2.0 的只有 `snr_estimator.cpp:593,730,833` 三处 `out_model->idw_power` 初始化（Phase1 输出模型路径，与 Phase2 逐像素消费面不是同一条链）。配置键与 p* 日志未实现；`lib/**` 不在本单元文件域内，只登记不落码。p=2 降级为无噪/光滑极限最优读数。
4. **亮度携带（H4）**：丢源项 Oracle E=1.31e-3（机器零对照 2.22e-16），极端对比 12 倍效率损失；端到端链亮度跟随 √10 闭合（3.1970 vs 3.1623，+1.1% 来自 2% 控制噪声）；P5 定权增益 1.0428 vs 理论 1.0444。
5. **三口径（H5）**：同一物理量表示约定（台账 X3/A-P4 口径）；m_ref 降格为记录参考电平（A-P4-06）。
6. **几何与常数链（H6/H9）**：1/16 判据为带余量保守守卫；组成常数 `c(n=64)=1.152` 的出处是**跨单元分歧台账 D-04**（其 MC 落盘件未随本单元收录，**本单元不可复算**）；本单元可复算的自身读数为同一常数的实测 `k = 1.1614111`（n_rep=2000×4000 px，理论 1.1664237），**比 1.152 高 0.82%**、比理论低 0.43%（`【复现：实验/dense-snr-reconstruct/code/route2/exp_P4R2_08_mad_sigma_budget.py 生成的 exp08_mad_sigma_budget.json 的 part_B_mad_sigma_estimator】`）。引用 1.152 时须一并标注其为台账值。
7. **诚实边界主数**：稀疏控制格不表示 PSF 尺度——偏差中位 3.8 dex（p99 5.7 dex）[实验:route1/exp_p4_04]；有效域为 ≥Δ 尺度平滑场。

### 4.1 M16 物理前向仿真腿（H-sim）—— 12 门全绿，实测耗时 11.5 s

探测器 0.2″/px，Δ = 64 px = 12.8″，768² 帧，12×12 = 144 个控制点。源项标度扫描（α 与真方差动态范围
v_dr 一一对应）：

| α | v_dr | E_eff_frame | E_eff_cell | E_eff_dense | 样条负值像素占比 |
|---:|---:|---:|---:|---:|---:|
| 0.003 | 75.8 | 7.69e-03 | 3.20e-02 | 7.89e+04 | 2.5% |
| 0.01 | 236.2 | 1.52e-02 | 4.60e-02 | 3.72e+03 | 8.3% |
| 0.03 | 694.5 | 3.52e-02 | 7.03e-02 | 1.11e+06 | 14.7% |
| 0.1 | 2299 | 9.22e-02 | 1.78e-01 | 1.13e+05 | 21.9% |
| 0.3 | 6882 | 1.85e-01 | 4.23e-01 | 9.52e+04 | 22.6% |
| 1.0 | 2.29e+04 | 3.54e-01 | 9.20e-01 | 2.58e+04 | 18.3% |

| 判据 | 读数 | 结论 |
|---|---:|---|
| G1 Oracle 逐像素真方差臂 E_eff | −1.11e-16 | 机器零 |
| G2 排序 E_frame < E_cell < E_dense | 6/6 档成立 | PASS |
| G3 值域钳制守卫改善稠密口径（α=1：2.58e4 → 4.54） | 改善 5700 倍 | PASS（**注**：本腿 dense 臂是**未钳制**样条，守卫臂用 `clip(0.25·min, 4·max)` 启发式钳制，**两者都不是**生产默认算子 `natural_bicubic_spline_clip_v1` 的「钳到控制值值域 [min,max]」语义；见 §6 第 16 条） |
| R2 E_eff 对 w 的乘性缩放不变（×3.17） | ≤1e-8 相对 | PASS |
| R1 打乱控制点节点（α=1） | 2.58e4 → 2.17e5（8.4 倍） | 判红 |
| dex-RMSE(α=1) frame / cell / dense | 14.06 / 14.08 / 66.74 | 与排序一致 |

**归零负例**：
- **NC-A1 平坦真值 + 理想控制值**：三口径 E_eff 全部 **−0.0**（≤1e-12）⇒ **归零**。
- **NC-A2 平坦真值 + 估计器控制值**（物理前向：均匀源、均匀天光、无平场）：帧级口径 E_eff = **−0.0** 精确归零，
  Oracle = −1.1e-16，稠密口径 2.47e-3 **不小于** 帧级 ⇒ 稠密不得凭空占优。**归零 + 不占优**。
- **NC-B 打乱控制点节点**：8.4 倍变差 ⇒ **判红**。
- **NC-C 丢源项臂**：**分歧，不入门禁**——帧级口径下丢源项反而更好（1.77e-4 < 3.54e-1）。见 §6 第 14 条。

**噪声相关性边界（本腿的判据取舍）**：真实 drz 噪声经 32 次曝光 + drizzle 已相关化，本链生成逐像素独立
噪声 ⇒ 对相关长度敏感的判据**不得**用合成帧定标。现场探针给出定量理由：同一控制点配方（8×8 patch
1.4826·MAD 平方）下，合成帧控制点噪声 lag-1 = **+0.195**，真实 drz = **−1.1e-4**，相差 0.195。据此：

| 编号 | 被排除的判据 | 留在原腿的出处 | 排除理由 |
|---|---|---|---|
| E1 | IDW 最优幂 p* 与 K 近邻截断的定标 | route2/exp_P4R2_02、route3/exp04 | 最优 p 由噪声空间相关长度决定 |
| E2 | Δ² 偏置律系数（零噪偏置 128/64 = 4.06 vs 理论 4） | route2/exp_P4R2_03 | 偏置按有效独立样本数 n 走，n 由相关长度决定 |
| E3 | 白性 lag-1 = −1/2、散粒/PRNU 相对散布比、指纹斜率 | route2/exp_P4R2_05、route3/exp06 | 本链噪声按构造独立，该类判据在本腿恒真、无判别力 |
| E4 | E_eff 绝对效率数值向真实帧的外推 | calibers/exp_P4CAL_02 的 C 臂（MAD² 代理） | Var_opt = 1/Σ(1/v_true) 以噪声独立为前提 |

本腿**上**的判据：真值下三口径的相对序、dex-RMSE、算子性质（节点复现、钳制、覆盖域）、
亮度携带义务的端到端代价、四条归零负例、有效域边界。

## 5 结论

假说判定：H1/H2/H4/H6/H7/H8/H9 成立，H5 口径表示成立、但「dense 一律最优」经对照实验否证（见 §2 H5），H3 推翻并按 D-05 换默认值（drizzle 侧已落地，Phase1 估计器路径待裁决），H10 豁免为结构常数（科学后果另由 Δ/ℓ 判据承载）。P4 单元的科学内容收敛为一句话：**稀疏控制点到稠密 SNR 场的重建以自然三次样条钳制算子为生产默认（节点复现 3.6e-15、可分辨域内最优），定权恒等式 w = SNR²/F_ref² = 1/σ_F² 机器精度成立且 γ=2 由定义唯一确定；逐像素定权面上控制值是否携带亮度决定 P5 权重是否最优（最劣 12.6 倍，适用域 = 场在 Δ 尺度上平滑），IDW 的终裁默认 idw_power=1.0 已在 drizzle 侧生效、Phase1 估计器路径待裁决。**

## 6 诚实边界

1. 稀疏控制格不表示 PSF 尺度结构（3.8 dex 中位）：有效域边界，非精度缺陷；PSF 尺度归 P1 与结构感知估计器。
2. 跨 Δ 外推须重验（Δ=256 反超）；场曲率显著增大时 Δ 偏置上升须复评。
3. IDW p* 依赖场形态与噪声档，配置化后随日志积累重标定（台账 §4.2 开放项）。
4. E 判据乘性免疫，必须与 dex 水平判据成对使用。
5. γ=2 最优性以 SNR 估计无偏为前提；恒等式检验未测相关噪声（k_corr 面归 P2/P5，D-08 两因子查表由 P3 单元承载）。
6. 端到端链为缩小场景（8 源/512²、两帧）；Oracle 检验隔离信息内容。
7. 05 规格去钳制 E 1.007→1.44e4 数字未独立复刻，钳制必要性以机制佐证。
8. 标注级文献锚（Aitken/Moffat/de Boor）不承担任何数值判据。
9. 项目约定豁免：quality_factor 0.1/0.5（比值 0.1/0.5 与 share/absolute 口径差不豁免）、γ<1e-10、节点容差 1e-9、Δ=64。
10. **σ 口径是本项目定义而非通用物理量**：Moffat4 只有 α，本域 σ 由冻结参数化 `Q = 0.5r²/σ²` 定义（= √⟨r²⟩ = α/√2 ⇒ FWHM/σ = 1.2303076526）；他域逐轴口径 σ_g = α/2 ⇒ 1.7399178，二者恒差 √2、禁止互换（[推导:docs/derivations.md §6]、`docs/science/PSF.md §16`）。
11. **跨单元数字与缺口（交接）**：① 组成常数 1.152 与 mesh 高对比域读数（E 0.0490/0.0530）均出自**其他单元**，本单元未复核；② `idw_power=1.0` 实现侧落地属跨域改动（`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.{h,cpp}` 与 `docs/detail/registry/astrocs.phase1.noise-snr.md`「稀疏帧内层几何与重建算子」）；③ 逐像素重建面在 Phase2 已接通（`sparse_reconstruct` 为 `snr_path` 默认，`module_adapters.cpp:12581` 逐像素调 `weight_from_sparse_layer_pixel_prepared`）；`in.sparse = nullptr`（`:12112`）只表示稀疏层不经帧级标量链。真正 fail-closed 的是 `snr_path=dense`（`:11967`）。带绝对真方差参照的生产端到端试跑仍缺参照产品。详见 REPORT_paper.md §7.9–§7.14。
12. **判据与门禁的订正落点**：零源负例/亮度跟随门/有源对照三项原判据判别力不足（P4-M03）；订正不修改已归档实验（避免脚本与存档脱钩），而是新增判据实验 `code/fix/fix01_metric_E_and_gates.py`；Δ 上界守卫与节点相位量化为 `code/fix/fix02_boundaries_estimator_phase.py`（正例 Δ=64 平滑域 PASS、负例 Δ=256 高对比域 RED；相位半像素差 ≤0.5%、角点约定 1.23× 退化）。

13. **「真方差动态范围 1.78–235」不是有效域判据**（被否证假设）：窗口**内**（v_dr = 75.8）稠密口径照样失效
    （E = 7.9e4），窗口外（v_dr = 2.3e4）失效量级相当（E = 2.6e4）⇒ **v_dr 窗口既不充分也不必要**；它只是
    `calibers/exp_P4CAL_02` 6 档扫描中两条「dense 胜出」行的 v 跨幅极值，属 fixture 偶然量，不得留在任何
    有效域表述里。替代口径 = **Δ-格可表示性 `J_Δ = E_eff(cell-oracle)`**：只用真值即可算，不含估计量噪声、
    不含算子、与 v 跨幅无关、随 Δ 单调增。盲复算（M16 物理前向、**生产默认算子含值域钳制**）：
    `J_Δ` = 0.161(Δ=16) / 0.177(Δ=32) / 0.204(Δ=64) / 0.250(Δ=128) / 0.283(Δ=256)，同五档 `v_true` 跨幅恒为
    2.29e4（不随 α 变）⇒ 门随 Δ 走、不随动态范围走。**`J_Δ` 的已知盲区**：cell 内未分辨**点源**几乎不抬高
    `J_Δ`（点源场 0.046 vs 光滑场 0.0397），胞内阶跃边缘则抬到 0.419 ⇒ `J_Δ` 单独不是完整判据；完整口径是
    正本 `docs/science/CONTROL_WEIGHT_SNR.md` §8b 的三因子联合判据「`Δ/ℓ` × σ 场幅度 × 未分辨结构污染」，
    第三因子不可由控制网格自身推断、按数据来源显式开启 mesh 中值档。
14. **H4 的适用域是「场在 Δ 尺度上平滑」**：M16 物理前向场上帧级口径丢源项反而更好（E = 1.77e-4
    **小于**带源项的 3.54e-1），与 H4 的机制方向相反。机理与同一腿的排序读数一致——真实结构场上
    `E_frame < E_cell < E_dense`，**跟踪得越少越好**。判读：H4 的**机制陈述**（权形状须与
    `σ_w² = σ_slow² + S_src/g` 一致）在其适用域内成立（有效域内光滑解析场：丢源项 1.31e-3 vs 机器零
    2.22e-16、极端对比 12.6 倍）；**跨口径的普适性不成立**——帧级常数口径下源项只改标量、不改权形状，
    故 H4 只约束**逐像素**定权面。本腿据此把 NC-C 标为 informative-only（不入门禁），分歧原文登记在
    结果 JSON 的 `negative_control.NC-C_lost_source_term.discrepancy`。
15. **M16 仿真腿的覆盖范围**：只跑 F657N 单帧、单一指向、无滚转角、无 IDW 对照档、无 M16 三波段同天区、
    无不同指向/滚转角的多帧几何；不含任何相关长度敏感量的定标。
15b. **掩膜静默降级（本轮新披露的边界，G08-04 整改补披露）**：本腿期望率面取自
    `code/sim/exp_sim01_m16_forward_snr_truth.py:243` 的 `MS.load_canvas(scene, verbose=False)`。其掩膜目录
    `DEFAULT_MASK_DIR = "run/reverse_verify/m16_scene/masks"`（`m16_sampling.py:105`）在当前树**不存在**
    （`run/` 被 `.gitignore` 忽略；实测 `ls` 报「没有那个文件或目录」）。缺省路径为
    `m16_sampling.py:189` 的 `valid = np.ones(sci.shape, dtype=bool)`，`:203-204` 捕获 `FileNotFoundError`
    后仅置 `mask_meta = {"used": False, "reason": ...}`，告警**只在 `verbose=True` 时打印**，而本腿显式传 `verbose=False`
    ⇒ **掩膜缺失 ⇒ 静默降级为全像素有效（`valid_fraction = 1.00000`），无 fail-closed、无告警**。
    同构模式见 `m16_scene.py:246`（缺省 `np.ones`）与 `:261-264`（`[m16_scene] WARNING: mask not found ... -> all pixels treated valid`，同样受 `verbose` 门控）。
    后果：真实帧的饱和/坏像元与星云外缘**未被排除**，全部进入期望率面。
    另：`:247` 虽把 `cmeta` 收进内存态 `canvas["meta"]`，**未写入存档**——实测
    `【复现：实验/dense-snr-reconstruct/code/sim/exp_sim01_m16_forward_snr_truth.py 生成的 exp_sim01_m16_forward_snr_truth.json】` 的键集为
    `[experiment, data_class, scene, frame_id, seed, band, line, exposure_s, detector_scale_arcsec_per_px, delta_px, patch_px, delta_arcsec, alphas, truth_definition, domain_criterion, physical_arm, poisson_terms, quantization, saturation, flat_applied_to, contrast_sweep, A_in_domain, A_out_of_domain, negative_control, noise_correlation_probe, excluded_criteria, elapsed_s, verdict]`，
    **无 `mask` / `valid` / `cmeta` / `valid_fraction` 任何键** ⇒ 读存档无法回溯掩膜是否生效。
    ⇒ 本腿全部读数（α 扫描、`E_frame < E_cell < E_dense`、v_dr 窗口否证）按「全像素有效」口径解释；
    **在掩膜生效复跑前，不得表述为「已排除坏像元」**。复跑前置：由 G1 车道用 `m16_mask.py:572` 的 `--outdir`
    生成 `run/reverse_verify/m16_scene/masks/` 后重跑本腿；修法（缺掩膜即 fail-closed 或至少无条件告警）属
    `实验/shared/synthetic/**` 代码面，不在本单元文件域内，登记移交。
15c. **泊松实现与文档失真（同轮登记，代码侧移交）**：`实验/shared/synthetic/noise_model.py:10` 写
    「源 / 天光 / 暗电流**各自独立**」、`:281` provenance 记 `"poisson_terms": ["src","sky","dark"]`、
    `m16_sampling.py:29` 写「**各自 Poisson**（电子域）」；实现却是 `:270` 先求和
    `lam_e = src_e + sky_e + dark_e` 再 `:296` **单次抽样** `n_e = rng.poisson(lam_e)`，非三次独立抽样。
    统计等价（独立同参 Poisson 之和仍为 Poisson）⇒ **本腿任何方差/散粒读数不受影响**，失真的只是文档表述与
    provenance 字段；存档 `results/sim/…json::poisson_terms = ["src","sky","dark"]` 原样转录了该 docstring，
    易被读成「三次独立抽样」的实现事实。⇒ 本单元一律按「电子域对合速率**单次** Poisson 抽样（与逐项独立抽样统计等价）」引用；
    文档与 provenance 字段的订正属 `实验/shared/synthetic/**` 域，登记移交。
16. **本腿的 dense 臂不是生产默认算子（算子口径缺口）**：`calibers()` 里 `rec = spline2d(npl, Δ, q)` 后直接
    `w_dense = 1/rec`，**未施加生产默认算子 `natural_bicubic_spline_clip_v1` 的「钳到控制值值域 [min,max]」**；
    `dense_guard_clamp` 臂用 `clip(0.25·min(rec), 4·max(node))` 启发式钳制（因 `min(rec) < 0`，其下界为负、
    不构成下侧保护），**与生产钳制语义不同**。后果：本腿的稠密失效量级（E = 7.9e4…2.6e4、负值像素 2.5–22.6%、
    「钳制改善 5700 倍」）是**未钳制变体**的读数。以独立实现按生产语义（钳到 [min(node), max(node)]）复算，
    同场景 E_dense(Δ=64) = **1.95**（未钳制 2.58e4）⇒「稠密在 M16 上劣于 cell 与 frame」的定性结论不变，
    量级差四个数量级。本单元以不修改已归档实验为纪律（改脚本会使存档与脚本脱钩），该口径缺口登记为待补。
17. **本腿未执行 fail-closed 覆盖域与有效域判据**：`code/sim/exp_sim01_m16_forward_snr_truth.py` 定义了
    `valid_domain_mask()`（patch 级 v_patch / 真方差之比 ∈ [1/2,2]）但**全程未被调用**（结果 JSON 无对应字段），
    且 `calibers()` 的 `mask` 参数在本腿四次调用中均传 `None` ⇒「本腿把有效域变成可计算判据」与
    `kept_here` 中的「fail-closed 覆盖域」两条**没有实测读数**。对照：生产重建器对该项有明确门
    （`phase2_integrate/src/weight_chain.cpp`：`nx>=2 && ny>=2`、查询点越出层 cell 并集即 fail-closed 不外推、
    层值域非正即判红），本腿脚本无对应检查。
18. **控制点退化族 ⇒ 稠密臂与帧级臂逐位相等且无错误信号**（`code/redteam/rt06_reconstruction_redteam.py` 的 C1/C2/C3 组）：
    单节点轴 1×3（`E_dense = E_frame = 1.083e−1`）、16 点共线（`1.230e−1`）、控制点全落噪声主导区
    （`4.382e−1`，真值含 0.82 dex 峰值点源）三族均**逐位相等**，`J_Δ` 分别报 0.0397/0.0397/0.0462，
    **不告警**；2×2 与 3×3 的域外查询点占比 0.750/0.438 而 `J_Δ` 仍是 0.0397。生产的 `nx>=2 && ny>=2`
    只挡单节点轴，**挡不住 2 节点轴的强各向异性与共线**；本单元脚本是无守卫本地重实现，未覆盖全幅的配置被静默计分。
19. **胞内结构**：`J_Δ` 对胞内点源无判别力（点源场 0.0462 vs 光滑场 0.0397），胞内 100:1 阶跃抬到 0.4190；
    效率读数 `E_dense` 胞内点源 1.859 vs 帧级 0.438、胞内阶跃 **154.4 vs 24.5**（权重形状差 6.3 倍）。
20. **胞内点源下默认算子把重建面压成常数平台，误差方向乐观**（`code/redteam/rt_intra_cell_source.py`）：
    Δ = 64 px、控制点取真值、胞内 FWHM 3–20 px 点源下重建峰**恒 = `max(node)`**，`SNR_rec/true` = 0.675…0.008
    ⇒ **权重被高估 2×–1.5×10⁴ 倍**（亮像素**过度加权**），与「丢源项 ⇒ 最劣 12.6 倍」的保守方向**相反**；
    钳制 ON/OFF 的重建峰相对差仅 7.8×10⁻⁴ ⇒ **钳制不是机制**（机制是可分离自然样条不在胞内外插出控制点之上的值）。
21. **重建量自身预测方差的低估**：`Σa_k²·Var_k` 同样以噪声独立为前提——等权 K 邻居平均下 真值/预测 = `(1−ρ)+ρK`，
    本单元实测 ρ = 0.195 时 **K=16 低估 3.93×、K=64 低估 13.3×**；控制格边缘有效邻居更少、权更尖（`Σw²` 1.98×）
    再叠加 **1.4–2.0×**，边缘区合计 5–8 倍。⇒ 重建量预测方差的**绝对值不可用**，只可用相对序。
22. **`w` 对 `m_ref` 的不变性有适用域**：不变性是代数恒等，**天光受限臂精确成立（≈4e-16）**，
    **生产组成臂只近似成立**（`m_ref` 档实测漂移 1.9%–4.2%、源主导臂 3.83%）⇒ **跨帧比较必须限同一 `m_ref` 档**。

## 7 复现命令

```bash
cd "实验/dense-snr-reconstruct"
# (0) 公共前置步：合成数据物理链自检（四个组件全跑）
#     不得只调 noise_selftest.py —— 它只验独立重实现，对生产实现零判别力
bash 实验/shared/synthetic/run_selftests.sh          # 实测 4/4 PASS、rc=0、约 2 分 32 秒

bash code/run_all.sh                 # 前置步 + 19 个原实验 + M16 仿真腿
python3 code/sim/exp_sim01_m16_forward_snr_truth.py   # 只跑 M16 物理前向仿真腿（~12 s）
# 单项示例（输出与 results/ 存档同构，固定 seed）：
python3 code/route1/exp_p4_01_weight_optimality.py
python3 code/route2/exp_P4R2_02_idw_params.py
python3 code/route3/exp03_sparse_dense_reconstruction.py
# 订正轮的补充实验（不改动已归档的 19 个原实验）：
python3 code/fix/fix01_metric_E_and_gates.py        # E 唯一口径定案 + 三处无判别力判据改造（M03/B02）
python3 code/fix/fix02_boundaries_estimator_phase.py # 估计量偏差（M01）+ Δ 上界守卫（M02）+ 节点相位（M04）
python3 code/calibers/exp_P4CAL_01_three_calibers.py     # 三口径对照 v1（过程记录）
python3 code/calibers/exp_P4CAL_02_three_calibers_guarded.py  # 三口径对照定稿（M10）：三类数据 + 对比度扫描 + 守卫臂，all_pass=true → results/calibers/exp_P4CAL_02_three_calibers_guarded.json
# 两脚本输出与 results/fix/*.json 逐字段同构（本机实测 all_pass=true）。
```

依赖：Python3 + numpy（stdlib json），无网络、无时间/环境随机源。19 个原实验不 import 仓内任何模块，
输出以脚本自身位置锚定，统一落 results/{route1,route2,route3}/（与运行时工作目录无关），与存档逐字段同构。
M16 仿真腿另需 astropy + scipy 与 `testdata/HST_M16/`（唯一仓外只读输入），并只读 import
`实验/shared/synthetic/` 的生成器（本单元不写、不改共享层），输出落 results/sim/。

## 8 佐证来源

按最高设计 §12.1「每个科学结论具备三类一手证据」：

| 一级断言 | 一手论文/标准 | 开源实现（项目 + 版本 + 文件:行） | 仓内实测（复现命令 + 读数） |
|---|---|---|---|
| 定权恒等式 w = SNR²/F_ref² = 1/σ_F²，γ = 2 由定义唯一（GLS 逆方差最优） | **Aitken 1935/36, Proc. R. Soc. Edinb. 55, 42–48（DOI 10.1017/S0370164600014346，逆方差定权理论腿）**；Rousseeuw & Croux 1993, JASA 88(424), 1273（参考文献 14，MAD 效率语境）。Keys 1981 是插值算子出处，**不承担定权理论腿** | `lib/algorithms/drizzle/healpix_drizzle/snr_estimator.{h,cpp}`（本单元只登记不落码，见 §6 第 11 条②） | `bash code/run_all.sh` → route1/exp_p4_01、route2/exp_P4R2_01、route3/exp01、fix/fix01：恒等偏差 3.7e-16…5.55e-16（2 ulp），γ=0 差 26.3×、γ=1 效率 3.73、等权 104.58 |
| 生产默认算子 natural_bicubic_spline_clip_v1：节点复现 ≤3.6e-15、收敛阶 −4、钳制必要 | de Boor 2001, *A Practical Guide to Splines*（标注级，不承担数值判据，refs.md 参考文献 11） | 同单元 `code/route3/exp03_sparse_dense_reconstruction.py` 的独立分段基实现 | route1/exp_p4_02、route3/exp03、exp05；**M16 仿真腿 G3：钳制把 E 从 2.58e4 压到 4.54** |
| 稀疏控制格不表示 PSF 尺度、有效域为 ≥Δ 尺度平滑场 | Trujillo et al. 2001, MNRAS 328, 977（参考文献 15，Moffat 轮廓 Eq.(1) 逐字锚）；Moffat 1969, A&A 3, 455（参考文献 10，未验证、不承担数值） | 共享物理链 `实验/shared/synthetic/{noise_model,m16_scene,m16_sampling}.py`（自检面全四项） | route1/exp_p4_04 偏差中位 3.8 dex；**M16 仿真腿：α 扫描 6/6 档 E_frame < E_cell < E_dense，v_dr 窗口判据被否证** |
| 判据 E_eff = Var_w/Var_opt − 1 唯一口径、有证据资格、乘性免疫 | Astier & Antilogus 2019, arXiv:1905.08677（参考文献 13，PTC 斜率语境） | 仓内 `code/fix/fix01_metric_E_and_gates.py`（判据改造的唯一落点） | route3/exp02、fix/fix01：乘性 σ̂=3.17σ 时 E = −1.1e-16、错误臂 0.314 判红、打乱 0.6335；**M16 仿真腿 R2：×3.17 后 ≤1e-8 相对** |
| 物理前向仿真数据本身 | Merline & Howell 1995；Howell 2006（CCD 噪声模型），逐条文献锚见 `实验/shared/synthetic/noise_model.py` 模块 docstring | 共享链 `实验/shared/synthetic/`（本单元只读 import） | `bash 实验/shared/synthetic/run_selftests.sh` → 4/4 PASS、rc=0、约 2 分 32 秒；判别力已由注入缺陷实测 |
| 真实数据腿 | — | `testdata/HST_M16/*.drz`（HST WFC3/UVIS 窄带 drizzled，PHOTFLAM 带头，NDRIZIM=32）；本地 MAD² 代理参照 | `python3 code/realdata/exp_P4RD_01_real_hst_m16.py`（见 §6 第 5 条：HLA drz 无 ERR/WHT，参照量是局部 MAD² 代理，不是绝对真方差） |
| 辅助常数链 1.4826 / 9216 预算 / Moffat4 = 1.230310 | Rousseeuw & Croux 1993（参考文献 14）；Trujillo et al. 2001（参考文献 15） | `code/route2/exp_P4R2_08_mad_sigma_budget.py`、`exp_P4R2_09_moffat4_factor.py` | 1.4826022185056023（1.50e-16）；9216 = (1.44/0.015)² 精确；闭式 1.2303076526 vs 独立数值积分 1.2303076507（差 1.91e-6） |
