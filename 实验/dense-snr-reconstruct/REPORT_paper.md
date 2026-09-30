# 稀疏绝对信噪比控制点的稠密重建：自然三次样条钳制算子、逆方差定权恒等式与亮度携带必要性的三路证据综合

**单元**: 实验/dense-snr-reconstruct · ACSD 科学链创新点 P4（最高设计 §2 第 4 点）
**裁决依据**: `实验/裁决台账.md`（D-xx）与 `docs/DISPUTES.md`（A-P4-xx），科学正本同步件 `docs/science/DISPUTE_RESOLUTION.md`；证据源 = 三路互不通信的 report/refs/code/results（路线1/2/3），本单元整合为定稿。
**复现**: 见 REPORT_experiment.md §6 与 code/run_all.sh；全部结果为固定 seed 存档读数，本单元不重跑实验。

---

## 摘要

跨帧绝对信噪比（P2）在稀疏控制格上产出的绝对 SNR 值，需要重建为逐像素的稠密信噪比场才能供下游加性天光去除与无接缝叠加（P5）逐像素定权。本文以三路独立审计证据综合论证该重建环节的科学内容：(1) 定权恒等式 w = SNR²/F_ref² = 1/σ_F² 在 float64 下以机器精度成立（四路独立验证最大相对偏差 4.44e-16–5.55e-16），权重幂次 γ=2 由此恒等式与广义最小二乘最优性唯一确定而非可标定的自由参数；(2) 生产默认重建算子为带值域钳制的自然三次样条（natural_bicubic_spline_clip_v1），其节点复现残差 ≤9.3e-15（订正注记：摘要行按存档 results/route3/exp05_operator_axioms.json A3_node_reproduction.max_abs_deviation.natural_bicubic_spline_clip_v1 = 9.33e-15 与 §2.3/§7 支撑结论的 ≤9.3e-15 口径统一），在可分辨域（Δ/ℓ ≲ 1）内精度最优，且收敛阶实测 −3.97/−4.18 与理论 −4 闭合；(3) 反距离加权（IDW）保留为备选口径，其默认衰减指数按含噪工况终裁为 idw_power = 1.0（无噪/光滑极限下 p=2 最优，含噪最优带 p≈0.5–1，p=2 差 2–3 倍）——该值为**配置级终裁，drizzle 侧求值器已落地为 1.0**，Phase1 估计器路径仍为 2.0（§4.3、§7.9）；(4) 控制值必须携带源项亮度（SNR_c = F_ref/√(σ_slow² + S_src/g)）：在**逐像素**定权面上丢失源项使下游堆叠效率损失在极端对比场景恶化 12.6 倍（适用域 = 场在 Δ 尺度上平滑；帧级常数口径上该机制方向相反，见 §7.18）；稀疏控制格不表示 PSF 尺度结构（偏差中位 3.8 dex）；稠密重建的有效域是**方差场在 Δ 格上可表示**（`J_Δ = E_eff(cell-oracle) ≪ 1`）且不含 cell 内未分辨结构，真方差动态范围**不是**判据（§7.17）。上述每项结论均由至少两路独立实验、解析推导与一手文献相互约束。

---

## 1 引言

在 ACSD 五点科学链中，P4 位于 P2（跨帧绝对 SNR）与 P5（加性天光去除与无接缝叠加）之间：P2 以 sparse_snr_layer 产品在间距 Δ=64 px 的控制格 cell 中心写入无量纲绝对信噪比 SNR_c = F_ref/σ_F,c，P4 将其重建为稠密 SNR 场，P5 按唯一权重公式 w = SNR²/F_ref² = 1/σ_F² 现场派生逐像素权重完成异质方差加权叠加与天光拟合。重建环节的任何系统偏差直接转化为 P5 的效率损失；控制值的信息内容（是否携带源项亮度）则决定权重能否达到最优。

P4 的科学问题因此分解为四个：(i) 权重公式的幂次 γ 是否为自由参数；(ii) 重建算子应取何者、其数值性质有何保证；(iii) 备选取径（IDW）的参数如何配置；(iv) 控制值的信息内容与重建的有效域边界何在。对对抗性审查提出的"三腿缺失与幻觉锚"指控，三路独立审计各自完成了清单覆盖（路线1 8/8、路线2 9/9、路线3 12/12），总编对账对全部分歧作出裁决；本文按裁决后的口径重写，不保留未决争议（所有先前的 UNRESOLVED 或已由台账裁决，或移入 §7 诚实边界）。

## 2 【链条位置】

### 2.1 上游接口（P2 → P4）

- 输入产品：sparse_snr_layer 控制点层，含绝对 SNR(x,y)（与帧级 frame_snr 同口径）、逐帧参考通量 F_ref,k = 10^(−0.4·(m_ref − ZP_k))、控制点几何（Δ=64 px 规则网格、node_placement = cell_center_v1，节点位于 i·Δ + (Δ−1)/2）。
- 量纲：SNR 无量纲；F_ref 单位 ADU；权重 w 单位 [ADU²]⁻¹。
- **前提（P4-M01）**：控制点值 = 生产配方（8×8 patch 稳健方差 `1.4826·MAD` 平方 + cell 内 `var=a+b·x+c·y` 平面拟合取节点值）对 **cell 平均方差**的代表性，是**待验前提而非已知事实**：结构场下实测偏差 E_eff = 0.0232 / dex 0.0424、平坦场对照 1.6e-4 / 0.0038（`results/fix/fix02_boundaries_estimator_phase.json::A_control_point_estimator_bias`），真实 HST 帧上实测 dex 0.0026（`results/realdata/exp_P4RD_01_real_hst_m16.json::A_control_point_representativeness_real`）。
- 结构性锚：Δ = hips.tile_width/8 = 512/8 = 64 为 HiPS 瓦片几何的结构派生量（defaults.json 冻结 tile_width=512；schema 冻结 cell_center_v1）。审查提出的"Δ=4096/8=512≠64"疑点经裁定为对密度口径的算术化误读（按分歧台账 A-P4-01 订正）。

### 2.2 下游消费（P4 → P5）

- 唯一权重换算 w = SNR²/F_ref² = 1/σ_F²（γ=2 恒等式，§4.1）；m_ref 仅设记录参考电平，经恒等式 w 对 m_ref 严格不变（亮端 1 mag ⇒ SNR ×2.512 仅为记录口径；按分歧台账 A-P4-06 将其标定必要性降格为记录口径约定）。
- 亮度携带义务：控制值必须取含源项口径 SNR_c = F_ref/√(σ_slow² + S_src/g)（g = 1.3 e⁻/ADU），否则 P5 效率上限受损（§4.4，最劣 12 倍）。
- 控制点 SNR=0 按不可估计处理；空控制格按最近控制格值填充（nearest_control_point 语义，空格 ≠ 零 SNR）。
- 下游 P5 的天光定权 control_variance 所涉 k_corr 已由分歧台账 D-08 终裁改为 k_gauss(N_retained) × k_geo 两因子几何查表（P3 单元承载<!-- 订正: 检查-跨文档冲突 红1 连带——原 k_shape × k_geo，全域记号统一 -->），P4 接口仅须传递 N_retained 与几何元组，不承载该常数。

### 2.3 精度约定

- 节点复现自检门 1e-9（P-CST-12）是"精确插值"的浮点性质而非松容差：三路实测残差 1.8e-15（route3/exp03 样条实现）/ 3.55e-15（route1/exp02 全部算子×全部 Δ 最大残差）/ ≤9.33e-15（route3/exp05 算子公理节点复现），距门限六个数量级余量（登记面 = P4 豁免清单：节点容差 1e-9 属浮点性质、实测 ≤3.6e-15，与生产重建算子默认的台账裁决 D-10 无涉。<!-- 订正: 检查-行文逻辑 Y5——原括注把节点容差语义挂到 D-10（其裁决对象是生产默认重建算子 natural_bicubic_spline_clip_v1），改挂正确台账条目。旧对照：语义按分歧台账 D-10 口径写明 -->）
- 三口径（dense / sparse_reconstruct / frame_reconstruct）表示约定：同一物理量 SNR = F_ref/σ_F 的三种还原粒度，差异全部表现为同一权公式下的重建保真度差，不构成三种权（**route1** I6 实测：frame 平铺 dex-RMSE 0.0295 vs 样条重建 0.0058，动态范围保持比 0.7525 vs 0.9888 [results/route1/exp_p4_04_brightness_forward.json::arms/A_frame_recon/dr_ratio_preserved 与 ::arms/A_full/dr_ratio_preserved]，E 0.0181 vs 0.0007；权重公式不变）。其中 `frame_reconstruct` 臂在本单元按**帧级常数铺满**实现（`code/route1/exp_p4_04_brightness_forward.py`，属退化的上界对照，**不是**生产的帧级逆方差链），`dense` 口径在本单元的对照实验见 §7.10。三口径对照给出**粒度-效率**边界（§7.10）：`cell`（Δ=64 格级常值铺满）在含结构 fixture 上优于 `dense`（生产样条逐像素）；`dense` 优于帧级常数口径的**真方差动态范围 1.78–235 窗口已被否证，不是有效域判据**（§7.17）。
- **各层精度档（P4-m13）**：P4 帧域重建算子与其自检为 **float64**（节点复现残差 ~1e-15 量级）；drizzle 侧逐像素 SNR 求值器（`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h`）按接口接受 **float32** 控制点并输出 **float32** SNR 场——"FP64 全链"的表述**不覆盖**该层，跨层传递的量化误差为 f32 的 ~1e-7 相对量级，不进入本单元任何 E/dex 读数。
- 有效域：稠密（逐像素）重建的有效域由「**Δ-格可表示性**」划定——方差场在 Δ 格上可被 cell 均值忠实表示，即 `J_Δ = E_eff(cell-oracle) ≪ 1`（只用真值即可算，不含估计量噪声、不含算子）；`J_Δ` 随 Δ 单调增、与方差动态范围无关。`J_Δ` **只捕捉胞内方差结构**：cell 内未分辨**点源**（源项在分子侧）使 `J_Δ` 几乎不动（点源场 0.046 vs 光滑场 0.0397），故它**不是**完整判据；完整口径是正本的三因子联合判据「`Δ/ℓ` × σ 场幅度 × 未分辨结构污染」，第三因子不可由控制网格自身推断、按数据来源显式开启 mesh 中值档（`docs/science/CONTROL_WEIGHT_SNR.md` §8b、`docs/detail/registry/astrocs.phase1.noise-snr.md` §4.5）。PSF 尺度结构不在本域（§7）。
- IDW 插值设置（idw_power、K）**目标形态**为配置化，并在每次重建的运行日志输出实测最优 p*（argmin）与 K（日志不落盘产品）。**实现侧现状（如实登记）**：drizzle 侧求值器已与规范一致（`snr_evaluator.h:110` 成员初值 `idw_power_ = 1.0`；`snr_evaluator.cpp:212,268` 的 ≤0 兜底回落 1.0）；**仍硬写 2.0 的只有 Phase1 结构感知估计器路径**三处赋值（`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593,730,833`，即 `out_model->idw_power` 初始化）。配置键与 p* 日志均未实现——残余缺口见 §7.9。

## 3 方法

### 3.1 三腿证据模型

每项主张以三腿闭合：文献腿（一手来源逐字/元数据核验，见 refs.md）、实验腿（固定 seed 合成实验含"真值无效应 ⇒ 度量归零"负例）、理论腿（解析推导，见 docs/derivations.md）。三路审计互不通信，其一致读数构成独立复现；分歧由总编对账裁决（分歧台账 D-xx/A-P4-xx），本文一律采用裁决后口径。**数据面边界（如实登记）**：本单元数据全部为**合成**，覆盖最高设计 §12.2 的第①类（Moffat 源面 + 物理前向）与第②类（纯解析代数合成 + 负例），**不含第③类 testdata 真实数据腿**（单元内无 `data/`，真实帧实验归 P1/P2 单元），见 §7.11。

### 3.2 实验设计

路线1 四实验（EXP-P4-01…04）：定权最优性 MC（n=2000 随机 σ、4 万次试验）、512² 四算子 × Δ∈{16,32,64,128} 对比、控制点几何条件数、256² 亮度正向端到端链。路线2 九实验（P4R2-01…09）：γ 扫描（24 帧、2 万次试验）、IDW 参数全扫（1024²、p∈{0.5…3}×K∈{4,8,16,32,all}×噪声{0,2%,6%}）、Δ 偏置律、条件数放大、方差图缓变律、判据臂、亮度链、MAD/预算链、Moffat4 闭式。路线3 六实验（EXP-P4-01…06）：恒等式与 m_ref 传播、判据 E 性质、收敛阶、IDW 参数与极限、算子公理、白性与分子纯度。合成数据三类齐备（Moffat 源面物理前向仿真、纯解析代数合成、含"真值无效应⇒归零"负例）。

## 4 数据与结果

### 4.1 定权恒等式与 γ=2（非自由参数）

恒等式 w = SNR²/F_ref² = 1/σ_F² 由定义 SNR ≡ F_ref/σ_F 代数给出（推导腿，docs/derivations.md §1）。四路独立实验在 float64 下复现其机器精度成立：路线2 A 腿 max 相对偏差 4.44e-16（含逐帧不同 F_ref 变体 5.55e-16）[实验:route2/exp_P4R2_01_weight_identity_gamma.py]；路线1 5.55e-16 [实验:route1/exp_p4_01_weight_optimality.py]；路线3 3.7e-16，且"上游给 SNR、下游取权重"两条路径的偏差 3.4e-16 [实验:route3/exp01_weight_identity_gamma_optimality.py]；P2 路线1 恒等式实验偏差 4.4e-16 = 2 ulp [实验:P2 exp12，分歧台账 A-P2-11 转写]。

幂次的统计内容为广义最小二乘（GLS/Aitken）逆方差最优性 [文献:refs.md#Aitken1935，标注级] [推导]：对独立异方差观测，w ∝ 1/σ² 使组合方差达到解析下界 1/Σ(1/σ_k²)。实验闭环：γ 扫描中 γ=2 方差最小（6.458e-6），最差 γ=0 比 γ=2 差 26.3 倍 [实验:route2/exp_P4R2_01]；γ=1 相对最优效率 3.73、等权 104.58（2 dex 动态范围）[实验:route3/exp01]；等权 E=84.84、w∝SNR E=0.956 [实验:route1/exp_p4_01]。负例：全部 σ 相等时各方案方差精确相等（E 归零，MC 噪声内）[实验:route1/exp_p4_01, route3/exp01]。

**结论**：γ=2 由恒等式 + GLS 最优性唯一确定；任何 γ≠2 等价于明确放弃逆方差最优性（γ=1 = 以约 26 倍组合方差损失换保守）。据此，审查件"γ 需标定/改 γ=1 保守"的要求按分歧台账 A-P4-02 改写为"γ=2 理论腿导出 + EXP01 型非退化实验腿"；审查建议"改 γ=1"不成立。

### 4.2 重建算子：自然三次样条钳制为生产默认

生产默认算子为 natural_bicubic_spline_clip_v1（规范 07 §4.5、05 P-ALG-09 正本口径）；双线性为对照/回退档（**按分歧台账 D-10 订正**：旧稿总览"现行默认 bilinear_regular_grid_v1"及其旧 EXP-04 对比数字弃用，本文对比表统一采用路线1 EXP-P4-02 同 fixture 数据）。三路读数如下。

dex-RMSE（fixture = 路线1：512²、40 高斯斑、相关长度 ℓ≈48 px；**与下表 E 表不是同一 fixture，两表不可逐格对比**）[实验:route1/exp_p4_02_interpolators.py]：

| Δ (px) | 样条+钳制 | 双线性 | IDW p=2 | IDW p=1 |
|---|---|---|---|---|
| 16 | 0.0024 | 0.0057 | 0.0148 | 0.0258 |
| 32 | 0.0135 | 0.0215 | 0.0430 | 0.0687 |
| 64 | 0.0892 | 0.0853 | 0.0985 | 0.1311 |
| 128 | 0.1878 | 0.1798 | 0.1730 | 0.1853 |

效率判据 `E_eff = Var_w/Var_opt − 1`（**唯一权威口径**，显式定义与 Cauchy–Schwarz 证明见 [推导:docs/derivations.md §7]：`Var_opt = 1/Σ(1/σ_true²)`；被否口径 `1/Σw` 只在负例中使用）；fixture = 路线3：1024²、GRF ℓ=128 px（smooth 域）[实验:route3/exp03_sparse_dense_reconstruction.py]：

| Δ (px) | 样条+钳制 E | 双线性 E | IDW p=2 E |
|---|---|---|---|
| 16 | 6.9e-11 | 2.9e-5 | 3.0e-4 |
| 32 | 2.9e-8 | 4.5e-4 | 3.4e-3 |
| 64 | 4.7e-5 | 6.5e-3 | 3.2e-2 |
| 128 | 2.2e-2 | 7.6e-2 | 1.1e-1 |
| 256 | 2.98e-1 | 1.94e-1 | 1.48e-1 |

表内 E 的不确定度：E 由 25×20 子网格（约 500 个加权点）估计，按 Δχ² 近似其相对不确定度约 ±5%——因此**同一 Δ 行内相差 <10% 的两个算子不可判优劣**（如 smooth Δ=64 的 3.15e-2 与 3.62e-2），表中"最优/次优"只按点估计排序陈述；跨 Δ 的 1 个数量级以上差异不受影响。

支撑性结论：

1. **节点复现**：全部算子 × 全部 Δ 最大绝对残差 3.55e-15（路线1）/ 3.6e-15（路线2）/ 1.8e-15 样条实现（路线3），对 1e-9 门六个数量级余量 [实验:route1/exp_p4_02, route2/exp_P4R2_02, route3/exp03]。
2. **收敛阶**：双线性内部实测 −1.93/−1.98（理论 −2）、样条 −3.97/−4.18（理论 −4）；含自然端点边界层时样条阶退化（−3.88/−4.13），端点效应为规范相关真实性质 [实验:route3/exp03] [推导:docs/derivations.md §3] [文献:refs.md#Keys1981, refs.md#Franke1982]。
3. **钳制必要**：对抗钉刺网格下无钳制样条重建 min = −161.5（负 SNR）、3.2% 像素非正、5.7% 越出值域；钳制后 overshoot = 0 [实验:route1/exp_p4_02]。独立复现：无钳制范围 [−0.148, 4.160] 越出控制值域且出现负 σ，钳制后回到值域 [实验:route3/exp05_operator_axioms.py]。负 SNR 非物理，钳制不可省。
4. **算子公理可红**：正齐次性对全部冻结算子最大相对偏差 ≤2.8e-15，构造的向先验均值收缩算子违反之（偏差 0.54）——排除"线性算子必然满足故检验恒真"的疑义 [实验:route3/exp05]。
5. **可分辨域与外推边界**：样条在 Δ/ℓ ≲ 1 的可分辨域内精度最优；Δ=256 处被双线性/IDW 反超——"默认档总是最优"不成立，跨 Δ 外推须重验（**按分歧台账 A-P4-07 采信路线3 发现**）[实验:route3/exp03]。高对比域（cell 内未分辨尖峰）自 Δ=32 起全部五个算子 E>0.5（Δ=32 最小 0.5122、Δ=64 最小 1.2964），**算子间极差 2.71 倍（Δ=32）/ 2.81 倍（Δ=64）/ 2.27 倍（Δ=128）**（旧稿"<2.5 倍"与逐 Δ 读数不符，按 [results/route3/exp03_sparse_dense_reconstruction.json::domains/highcontrast] 重算订正；注意 Δ=16 未被该结论覆盖，其样条值 0.0410 < 0.5）：未分辨结构域中算子选型不是主要矛盾，控制点估计量才是 [实验:route3/exp03]。
6. **节点相位约定**：cell_center_v1 节点位于 i·Δ+(Δ−1)/2；误读为 corner 约定即整体错位 (Δ−1)/2（Δ=32 即 15.5 px），实测使 dex-RMSE 0.0135 → 0.0397（2.9 倍）、E 0.0030 → 0.0348（11 倍）[实验:route1/exp_p4_02]。这是 Δ=64 需要"冻结生效方式而非仅冻结数值"的量化理由。

### 4.3 IDW：备选口径与默认 idw_power = 1.0

IDW 保留为备选口径（生产算子词表无 IDW 档）；其参数按分歧台账 D-05 终裁（**订正**：原登记 idw_power=2.0 无任何标定记录，A-P4-03 判"不动 2.0"缺实验腿）。路线2 全扫（1024²、Δ=64、真值 = 平面 + 弱曲率、p∈{0.5,1,1.5,2,2.5,3}×K∈{4,8,16,32,all}×噪声{0,2%,6%}）[实验:route2/exp_P4R2_02_idw_params.py]：

| 控制值噪声 | 最优 (p, K) | rmse_rel | 说明 |
|---|---|---|---|
| 0（无噪） | p=2.0, K=4 | 1.639e-3 | 无噪/光滑极限 p=2 最优 |
| 2% | p=0.5, K=16 | 6.827e-3 | 含噪最优带 p≈0.5–1 |
| 6% | p=0.5, K=32 | 1.414e-2 | p=2 差 2–3 倍 |

含噪（生产现实：控制点携带 ±2–6% 测量噪声）最优带 p≈0.5–1，p=2 差 2–3 倍——更平的权重（小 p）是更强的邻域平均压噪。路线1 在无噪光滑场上独立确认 p=1 显著劣于 p=2/p=4 [实验:route1/exp_p4_02]；路线3 确认误差面对 (p,K) 光滑无尖峰最优、p→∞ 极限退化为最近点算子（偏差 0.302 → 3.3e-3 → 5.5e-9 → 0）、γ 门 γ<1e-10 在 γ ≪ d^p 时算子逐位不变（数值守卫，非科学量）[实验:route3/exp04_idw_parameters.py] [推导:docs/derivations.md §4]。

**终裁表述**（分歧台账 D-05）：p=2 是无噪/光滑极限最优（路线1）；生产含噪默认 **idw_power = 1.0**（取含噪最优带 0.5–1 的上端：保留邻域平均压噪收益，同时为源核陡梯度把 p* 推大留偏移余量）；K=16 为可辩护折中、γ<1e-10 为数值守卫，均为配置级。p* 随噪声档与场曲率变化、不冻结单一"最优值"——idw_power/K 配置化，每次重建在日志输出实测 p*、K 与噪声档（**目标形态；实现侧"配置键 + p* 日志"均未实现**，见 §2.3 与 §7.9）；IDW 整体保持备选口径，生产默认算子为 natural_bicubic_spline_clip_v1（D-10）。文献腿：IDW 形式的一手出处为 Shepard 1968（原文引入幂次 p 为应用参数、未锁定普适值）[文献:refs.md#Shepard1968]；幂次敏感性方向与 Lu & Wong 2008 一致 [文献:refs.md#LuWong2008]。（按分歧台账 A-P4-05 订正：旧稿 Shepard 出处 "SIAM J. Numer. Anal. 5, 372, DOI 10.1137/0705029" 错误，正确为 ACM '68 会议录 517–524、DOI 10.1145/800186.810616。）

### 4.4 亮度携带义务：丢源项的最劣效率损失 12 倍

控制值的方差面口径决定权重是否最优：σ_w² = σ_slow² + S_src/g（含源项）⇒ w = 1/σ_w² 为代数最优；丢源项（背景受限）⇒ 亮源处权重被系统性高估。路线1 亮度正向实验（256²、Moffat β=2.5 共 25 源、σ_slow² 平面缓变、8×8 控制格 Δ=32；**本表轮廓族 = Moffat β=2.5**，与 §4.4 末路线2 亮度链（β=4）不是同族，逐处已标注族与口径；控制量口径 = F_ref/√σ_w²）[实验:route1/exp_p4_04_brightness_forward.py]：

| 权来源 | E（Oracle 信息检验，真方差含源项） |
|---|---|
| 含源项 SNR ⇒ w = 1/σ_w² | **2.22e-16（机器零）** |
| 丢源项（背景受限）⇒ w = 1/σ_slow² | 1.31e-3 |
| 等权 | 1.78e-2 |

重建臂（2-dex cell-SNR 对比度主场景）：A_full E = 7.4e-4、μ̂ 相对误差 4.5e-3、动态范围保持 0.989；丢源项 A_bglimit E = 1.7e-3；帧级常数口径 A_frame_recon E = 1.8e-2、动态范围保持比 0.99 → 0.75。极端对比场景（4-dex）：A_full E = 0.037 vs A_bglimit E = 0.433——**丢源项的最劣效率损失 12 倍**，且对比度越大代价越爆炸。负例（零源）：含源项/背景受限两控制格逐像素恒等（max dev = 0.0）、两臂 E 精确相等——须注意这是**同式构造的结构断言**（真值无源时两臂表达式恒等，任何算子都会判绿），不能单独承担"度量非退化"的证据；改成两臂各自经真实 S_src 估计流程的**估计器版零源判据**（零源帧逐臂逐位相等判绿 / 有源帧丢源项判红）与**算子敏感**的亮度跟随判据（全局常量臂必须判红）见 [实验:code/fix/fix01_metric_E_and_gates.py → results/fix/fix01_metric_E_and_gates.json::C_rebuilt_gates/C1_luminance_gate_rebuilt]。

端到端独立佐证（路线2 迷你链，512²、8 源、Δ=64 控制点 ±2% 噪声）[实验:route2/exp_P4R2_07_luminance_chain.py]：重建 RMSE 0.0795 dex、Pearson r = 0.9952；亮度跟随（通量 ×10 ⇒ 真 SNR 比 3.1968、重建场比 3.19698，理论 √10 = 3.1623，+1.1% 来自控制点噪声）——重建场跟随亮度而非复制方差图；零源负例稠密场 max|SNR| = 0.0 精确归零；P5 消费段 P4-SNR 定权堆叠方差/等权 = 1.0428（实测）vs 同口径理论 1.0444（0.15% 内闭合）；同存档第三个口径（逐像素均值）= 1.1199，三值并列见 [results/route2/exp07_luminance_chain_negative.json::P5_consumption_inverse_variance]，本文取**与实测同归约方式**的合并口径（逐像素均值口径对同一量换了归约）。

P5 侧下游佐证（路线1）：SNR 派生权在天区样本上自动复现背景逆方差（w = SNR²/F_ref² = 1/σ_slow²，b0 相对误差 0.0020 vs 真 ivar 0.0028）——"两方差面不混用"的实质是源项开关，而非"SNR 权不能用于天区"。

### 4.5 方差图缓变律与分子纯度

P4 把分母当缓变方差面处理的物理前提获得解析与实验双支撑 [实验:route2/exp_P4R2_05_whiteness_variance_map.py, route3/exp06_white_noise_and_purity.py]：白噪声一阶差分 lag-1 自相关 = −1/2（解析恒等式 [推导]），实测 −0.5007/−0.4995/−0.4984（三种随机项）与 −0.49963 ± 0.00078；方差图相对散布跟随电平场——散粒项比 **0.9240（route2 实测）/ 0.99999999（route3 解析档）**（律预言 1）、PRNU 比 **1.9113（route2 实测）/ 1.99067（route3 解析档）**（律预言 2）——括注里的两个数是**两条路线**的同名量，不是同一实验的两臂；指纹 log-log 斜率实测 1.000/2.000/0.000（散粒/乘性/常数）。分子纯度三臂：无源帧完整臂与漏源项臂逐位相等、SNR ≡ 0（精确归零），天光经其散粒噪声只进分母 [实验:route3/exp06]。

### 4.6 控制点几何判据与辅助常数链

- **平面条件数判据 λlo/λhi ≥ 1/16**：作用于中心化 2D 点云散布矩阵，κ = √(λhi/λlo) = 点云长短轴比（纯几何恒等式，实测偏差 ≤0.03% [实验:route1/exp_p4_03_plane_geometry.py]）；梯度 SE 沿特征向量方向的最坏放大 √(λhi/λlo)，实测 1.007/2.025/3.956/7.980 vs 理论 1/2/4/8（0.5% 内）[实验:route2/exp_P4R2_04_plane_conditioning.py]。精确共线负例触发率 1.000（判据能红）；各向同性随机云最小 λlo/λhi = 0.330 ≫ 1/16（永不误红）。1/16 是带余量的保守良态守卫（fail-closed 门），不是锐利物理边界 [推导:docs/derivations.md §2]。
- **度量 `E_eff = Var_w/Var_opt − 1`（`Var_opt = 1/Σ(1/σ_true²)`，唯一口径见 [推导:docs/derivations.md §7]）的性质**：乘性偏差完全免疫（σ̂ = 3.17·σ_true 时 E = −3.3e-16）、平坦真值 + 平坦臂精确归零、错误臂判红（E = 0.314，非恒真门）、对空间指派敏感（打乱 E = 0.729）；E 必须与 dex 水平偏差判据成对使用（水平偏差差 38 倍的两臂 E 仅"近似同水平"）[实验:route3/exp02_metric_E_properties.py]。
- **MAD→σ 与预算链**：1/Φ⁻¹(3/4) = 1.4826022185056023，与登记值相对差 1.50e-16 [实验:route2/exp_P4R2_08_mad_sigma_budget.py]；9216 = (1.44/0.015)² 算术精确，1.44 = 1.152 × 1.2533（两个因子取自台账，非本单元实测）。组成常数 c(n=64) = 1.152 的出处是**跨单元分歧台账 D-04**（20 万次 MC，"+0.19%" 的基准在台账内），**其产物不在本单元、本单元不可复算**——正文引用 1.152 时一律标注"台账 D-04（跨单元）"。本单元可复算的是自身存档同一常数的实测值 k = **1.1614111**（n_rep = 2000 × 4000 px，理论 1.1664237）：**比 1.152 高 0.82%**、比理论低 0.43% [results/route2/exp08_mad_sigma_budget.json::part_B_mad_sigma_estimator]。**订正**：路线2 报告"1.152 来源未定"的 UNRESOLVED-1 据此闭环（登记义务由 P2 单元在 02 §3 承担），但"本单元实测支撑 1.152"这一读法不成立。
- **Moffat4 FWHM/σ（σ 口径是本域的**冻结约定**，不是通用物理量）**：本域唯一 σ 口径 = **模型参数口径** `Q = 0.5·r²/σ² ⇒ α = √2·σ ⇒ σ = √⟨r²⟩ = α/√2`（[docs/science/PSF.md §16]、[推导:docs/derivations.md §6]）。该口径下闭式 `2√2·√(2^{1/4}−1) = 1.2303076526` [推导]（α↔FWHM 关系的一手文献腿：[文献:refs.md#Trujillo2001]，逐字 Eq.(1)），独立数值积分（梯形 + 解析尾项 + 二分求半高）1.2303076507，登记值 1.230310（相对差 1.91e-6 = 该常数六位小数圆整量）；同为**本域口径**时高斯轮廓为 1.6651。**他域口径** `σ_g = α/2`（同二阶矩高斯逐轴）给出 `4·√(2^{1/4}−1) = 1.7399178`，与本域口径**恒差 √2、禁止互换**：把 1.7399178 代入本域参数化，剖面在 r=FWHM/2 处为 0.2770（反解 FWHM 短 29.3%，判红）。写法差异不等于常数错误——常数随轮廓族与 σ 口径双重变化，判据非退化（正例残差 1.2e-6 判绿）[实验:route2/exp_P4R2_09_moffat4_factor.py]。

### 4.7 误差预算（逐项列出处；审查件 P4-M06/M07/M08 的订正要求）

| 项 | 物理来源 | 归档量级 | 出处 |
|---|---|---|---|
| E1 插值确定性偏置 | Δ/ℓ 与场曲率（采样相位 + 算子阶） | smooth 域 E_eff 从 6.9e-11（Δ=16）到 2.98e-1（Δ=256）；highcontrast 域 0.0410（Δ=16）到 >1.5（Δ≥64） | results/route3/exp03…json::domains/{smooth,highcontrast} |
| E2 控制点测量噪声 | 控制值 ±2–6% 噪声经 IDW 放大/平均 | 含噪最优 rmse_rel 6.827e-3（2%）/ 1.414e-2（6%）；p=2 差 2–3 倍 | results/route2/exp02_idw_params.json |
| E3 过冲与钳制 | 无钳制样条在钉刺（spike）网格上的振荡 | 钉刺档（Δ=32）：无钳制 min = −161.474、3.24% 像素非正、5.66% 越界；钳制后越界/非正 = 0。常规档最大非正比 0.387%（Δ=64）、最大越界比 9.76%（Δ=128） | results/route1/exp_p4_02_interpolators.json::adversarial_spike（`unclamped_min`/`unclamped_nonpos_frac`/`unclamped_overshoot_frac`）；`::runs/delta_64/spline_noclip`、`::runs/delta_128/spline_noclip` |
| E4 节点相位 | 生产 cell_center_v1（31.5）与实验约定（31/32）差半像素 | 审查件独立复算 Δ=64：E 0.12017（生产 31.5）/0.12076（31）/0.48910（角点）⇒ 半像素差 0.49%、角点差 4 倍；本单元同构复核（`results/fix/fix02_boundaries_estimator_phase.json::C_node_phase`）：rmse-dex 0.15751/0.15791/0.15712/0.19364 ⇒ 同结论 | run/FINAL-07/审核包/科研审查/evidence-P4/R7_phase.out；本单元 results/fix/fix02…json |
| E5 辅助常数 | Moffat4 FWHM 因子六位小数圆整 | 相对 1.91e-6（对 E 的二阶影响 <4e-6） | results/route2/exp09_moffat4_factor.json；§4.6 |
| E6 控制值估计量偏差 | ① 丢源项（背景受限口径）；② 生产配方（8×8 patch 稳健方差 + 平面拟合取节点值）与 cell 均值的偏差（P4-M01） | ① Oracle 隔离：`E_oracle_full = 2.2e-16`（机器零）、`E_oracle_bglim = 1.3138e-3`；2-dex 重建臂 `A_bglimit = 1.6842e-3`；4-dex 极端档 `extreme_A_bglimit = 0.38046` vs `extreme_A_full = 0.030158`（12.6 倍）；② 已量化（`results/fix/fix02_boundaries_estimator_phase.json`）：结构场合并偏差 E_eff = 0.0232 / dex 0.0424，平坦场对照 1.6e-4 / 0.0038（≪0.02 门）⇒ 控制点无偏是**前提**而非事实（§2.1）。**读数口径**：极端档以 `results/route1/exp_p4_04_brightness_forward.json::arms/extreme_*` 存档为准（A_full 0.0301577、A_bglimit 0.3804647，比值 12.6）；`results/summary.json::key_results/luminance_carrying/extreme_4dex` 与之一致 | results/route1/exp_p4_04_brightness_forward.json::oracle_information_test；`::arms/extreme_A_bglimit/stack_usecase/E`；`::arms/extreme_A_full/stack_usecase/E`；results/fix/fix02_boundaries_estimator_phase.json::A_control_point_estimator_bias |
| E7 MC 统计 | 有限子网格/有限重复 | 审查件按有效样本量（25×20 ≈ 500 点）估 E 相对不确定度 ~±5%；k 常数本单元实测 n_rep = 2000 × 4000 px | run/FINAL-07/审核包/科研审查/SCI-704…md::§7.3-P4-m06；results/route2/exp08_mad_sigma_budget.json |
| E8 增益误设 | g 未知或错设（真值 g = 1.3 e⁻/ADU） | **已独立复算并与审查件一致（差 ≤0.7%）**：+10% ⇒ 6.04e-4（审查 6.07e-4）、−10% ⇒ 7.18e-4（7.22e-4）、−23% ⇒ 4.36e-3、+54% ⇒ 1.30e-2、0.5× ⇒ 2.89e-2、2× ⇒ 3.50e-2；δ=0 ⇒ 0（机器零）。**解析律**：E_eff = ⟨a⟩_s⟨1/a⟩_s−1（a = v/v̂、s = 1/v̂）⇒ E_eff = 0 ⟺ v̂ ∝ v；增益误设下 v̂ ∝ v ⟺ σ_slow = 0 ⇒ **源主导区增益误设 E 中性**（实测 σ_slow=0 时 ≤4.4e-16），代价只来自 σ_slow² 与 S/g 的混合比离散度；|δ|≤0.15 时 E ≈ 0.06·δ² | run/FINAL-07/审核包/科研审查/P4_订正/evidence/F6_gain_sensitivity.json；[推导:docs/derivations.md §7]｜审查侧对照 evidence-P4/R8_redteam.out::R10_gain_misspecification_sensitivity |
| E9 判据盲区（非误差源） | E_eff 对 σ̂ 乘性缩放严格免疫 | 免疫偏差 ≤1.3e-14 ⇒ 乘性偏差只能由 dex 判据发现 | [推导:docs/derivations.md §7]；results/route3/exp02_metric_E_properties.json |

未量化项（如实登记，不假装闭合）：E8（增益误设）、E4（半像素相位，仅有审查件复算）、饱和像素/天光梯度/拥挤星场（本单元无实验）。

## 5 讨论

### 5.1 与旧稿内容的对账

历史正本中仍成立并吸收的部分：链条位置框架、三口径"同一物理量"表示约定、负例纪律、上游合同禁令（控制值冻结为绝对 SNR、不得以帧级 SNR 作尺度基准）。按台账订正的失效部分：默认算子陈述（D-10）、IDW 默认幂次与登记依据（D-05、A-P4-03）、Shepard/Keys 引用出处（A-P4-05）、旧稿 EXP-04 对比表（D-10 弃用）、Δ=64 的"4096/8"疑点（A-P4-01）。旧稿"mesh 档高对比域胜出"的定位按路线3 澄清：其成立依赖"控制点被 cell 内未分辨结构抬偏"的估计器偏差前提，mesh 收益必须与结构感知估计器绑定论证，单独算子对比无结论力。**跨单元数字标注（P4-m01）**：该定位所引的 HST M16 域读数 E=0.0490（mesh 中值前置）vs 0.0530（帧级）出自**另一单元**的正本 `实验/absolute-snr` EXP-04，**本单元未复核该数字**（本单元域内同结论证据 = route3 highcontrast 五算子 E 表）；引用时须连同其 results 路径一并标注。

### 5.2 审查锚修复结论（三路合判）

v6_clause_registry_v1.json:2015–2024 内容实存（条款 FZ-AP1-GLS-QW-RTOL），"幻觉锚"指控不成立，行号锚应改 JSON Pointer 属工程批评；PSF_SIGNAL_WEIGHT.md 的 26/26 定位符 miss 为希腊/ASCII 字符形式断裂（断锚非捏造，内容实存 :55）；《已确立》§3 常数节实存（行 108 起）。P-CST-21 四指数与 P4 逆方差指数 2 为范围错配，非冲突。三口径差异被误读为"三种权"的文档清晰性批评成立，已按 §2.3 表述收口。

## 6 结论

(1) 定权恒等式 w = SNR²/F_ref² = 1/σ_F² 以机器精度成立且 γ=2 由定义唯一确定，不是可标定参数；(2) 生产默认重建算子为 natural_bicubic_spline_clip_v1（节点复现 ≤3.6e-15、可分辨域内最优、收敛阶 −4 闭合），双线性为对照/回退档；(3) IDW 为备选口径，终裁默认 idw_power = 1.0（含噪最优带上端），目标形态为配置化并日志输出实测 p*；drizzle 侧求值器已落地 1.0、Phase1 估计器路径仍为 2.0（现状 §7.9）；(4) **逐像素**定权面上控制值必须携带源项亮度，丢源项使下游效率最劣损失 12.6 倍（适用域 = 场在 Δ 尺度上平滑；帧级常数口径上方向相反，§7.18）；(5) 稀疏控制格不表示 PSF 尺度结构（偏差中位 3.8 dex）；稠密重建的有效域是**方差场在 Δ 格上可表示**（`J_Δ ≪ 1`）且**不含 cell 内未分辨结构**的场，两者合起来即正本 §8b 的三因子联合判据；PSF 尺度与胞内未分辨结构归逐源测光（P1）与结构感知估计器管辖。真方差动态范围**不是**有效域判据（§7.17）。P4 的重建质量决定 P5 叠加与天光拟合的效率上限。

## 7 诚实边界

1. **稀疏控制格不表示 PSF 尺度**：逐像素 PSF 尺度 SNR 与 cell 聚合目标的偏差中位 3.8 dex（p99 5.7 dex）[实验:route1/exp_p4_04]。全部度量在 cell 表示域内定义；PSF 尺度归 P1 逐源测光与 07 §4.5 mesh_median 分支的结构感知估计器管辖。这是有效域边界，不是可修复的精度缺陷。
2. **跨 Δ 外推与 Δ 的可用上界（P4-M02）**：样条默认档在 Δ=256 被反超（A-P4-07）；高对比域 Δ=256 时五个算子 E 全为 1.52–1.89（bilinear 1.635 / spline 1.752 / mesh 1.515 / idw 1.582 / nearest 1.894，[results/route3/exp03_sparse_dense_reconstruction.json::domains/highcontrast]）⇒ 但**该读数不能定出「可用上界」**：同一张高对比域表里 Δ=128 的五算子 E 为 1.497–3.391（nearest 3.391 为最大值，range 2.27），**比 Δ=256 的 1.52–1.89 更差**；且 Δ=32 起五算子已全部 E>0.5（Δ=32 最小 0.5122）。⇒ 高对比域的可用上界是 **Δ ≤ 16**（Δ=16 时 spline E=0.0410），不是 Δ ≤ 128；Δ ≤ 128 只在**光滑域**成立（smooth 域 Δ=128 五算子 E ≤ 0.169）。跨域共用一个 Δ 上界不成立，上界必须与域一起声明。零噪偏置随 Δ² 增长（128/64 = 4.06，理论 4）。**守卫现状**：schema 对 Δ 只约束 ≥1、无与 Δ/ℓ 判据挂钩的 fail-closed ⇒ 本单元给出参考实现 `code/fix/p4_delta_guard.py`（正例 Δ=64 平滑域 PASS / 负例 Δ=256 高对比域 RED，`results/fix/fix02_boundaries_estimator_phase.json::B_delta_guard`），**落地到 schema/消费侧属域外交接**。
3. **IDW p***：本扫描真值面为平滑平面 + 弱曲率；真实 SNR 场的源核陡梯度会使 p* 向大偏移。p* 重标定为配置化后随日志积累的任务（台账 §4.2 开放项）。
4. **判据 E 的两条例题**：乘性免疫与不能替代水平判据（§4.6），E 必须与 dex 判据成对使用。
5. **γ=2 最优性的前提**：测量 SNR 无偏且其平方 ∝ 1/σ²；帧内 SNR 估计有系统偏差时，最优性转移到偏差修正后的量 [实验:route2/exp_P4R2_01 诚实边界]。恒等式检验假设高斯性与独立性，未测相关噪声（相关噪声经 P2/P5 的 k_corr 面处理，D-08）。
6. **端到端链规模**：亮度链为 8 源/512² 缩小场景、P5 段两帧；Oracle 检验隔离信息内容、无重建噪声。路线2 初版曾把空控制格置零（污染重建），已按 nearest_control_point 语义修正——空格 ≠ 零是接口义务而非实现细节。
7. **无钳制病态网格数字**：05 规格宣称的去钳制 E 1.007→1.44e4 属其自设病态网格读数，本单元未独立复刻该数字，钳制必要性以负 SNR/越界机制佐证（路线1 诚实边界）。
8. **文献锚形式**：Aitken 1935（DOI 未解析，卷期页 + INSPIRE 锚）、Moffat 1969（ADS bibcode 锚）、de Boor 2001（教科书未在线逐字核验，其性质由实验独立证实）均为标注级引用，任何数值判据不依赖它们成立（见 refs.md）。
9. **IDW 默认幂次的残余缺口（P4-M05）**：终裁默认 `idw_power = 1.0` 在 **drizzle 逐像素求值器侧已生效**（`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h:110` 成员初值 1.0；`snr_evaluator.cpp:212,268` 的 ≤0 兜底回落 1.0），与 `docs/detail/registry/astrocs.phase1.noise-snr.md` 的登记句一致。**仍与终裁不一致的只有 Phase1 结构感知估计器路径**：`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:593,730,833` 三处 `out_model->idw_power = 2.0`。该赋值属于 Phase1 输出模型的初始化，不是 Phase2 逐像素消费面的入参，**两条链不是同一处默认值**，须由 Phase1 单元单独裁决归属后再改；本单元只登记不落码（`lib/**` 不在本单元文件域内）。配置键与 p* 日志仍未实现。
10. **三口径对照实验（P4-M10，本轮闭环）**：已补 `frame`（帧级常数）／`cell`（Δ=64 格级常值铺满）／`dense`（生产样条逐像素）三口径对照实验（`code/calibers/exp_P4CAL_02_three_calibers_guarded.py` → `results/calibers/exp_P4CAL_02_three_calibers_guarded.json`；三类数据 = 解析合成 + HST 真实信号模板物理前向 + 真实帧代理；含平坦负例、6 档对比度扫描与三条守卫臂；`all_pass = true`）。**结论否证本单元原前提**：`dense` 口径并非一律最优——逐像素样条还原的自身噪声与欠冲使其在全部含结构 fixture 上劣于 `cell` 粒度（结构场 `E_dense = 0.577` vs `E_cell = 0.00903`；HST 前向 `2.57e5` vs `0.206`；真实帧 `6.46e4` vs `0.171`），该扫描曾给出的「v 跨幅 1.78–235 窗口」**已被否证，不是有效域判据**（§7.17）：它只是 6 档扫描中两条「dense 胜出」行的 v 跨幅极值，窗口内稠密口径在 M16 物理前向场上照样失效。守卫（按节点场夹取 + 散粒地板 `v̂ ≥ d_ADU/g`）可把 HST 前向腿 `E_dense` 由 `2.57e5` 降到 `43.2`，但仍**不优于 `cell`** ⇒ 消费侧若需逐像素稠密权重，应优先取 `cell` 粒度或强制夹取（P5/集成侧落地）。生产帧级口径 = 帧级 SNR 逆方差链 `w = SNR²/F_ref²`（`lib/infrastructure/scheduler/src/module_adapters.cpp:12595` 取 `snr_weights[slot]`，该链在 `use_snr_chain` 且无稀疏层时生效），单帧内即空间常数权场，与本实验 `frame` 臂同一泛函形式（`E_eff` 对 `w` 乘性缩放免疫 ⇒ 逐帧标量取值不改变单帧读数）。`snr_path=dense` 具名 fail-closed（`module_adapters.cpp:11967`，`SNR_PATH_DENSE_UNAVAILABLE`）⇒ **只有 `dense` 口径**「用生产产品跑一遍」不可执行，按标准记 **BLOCKED** 并给出复跑命令；`sparse_reconstruct` 口径的逐像素消费面已接通（§7.12），阻塞原因是缺绝对真方差参照而非接口。 **与 route1/exp04 的关系（关键区分）**：exp04 的控制值是**精确的 cell 模型量**（表示上限测试，无估计量噪声）⇒ 其 `E_full = 7.43e-4` 是**表示上限**（控制值精确时的可达下界）；本实验 `dense` 臂的控制值走**生产配方**（8×8 patch `1.4826·MAD` 平方 + cell 内平面拟合）⇒ 估计量噪声使同一算子的 `E` 升到 `0.577`（平滑场）/`2.57e5`（亮源前向）。两者不矛盾：**真实可达效率由控制值精度支配**，`7.43e-4` 不是端到端可达值。
11. **真实数据腿的范围与限制（P4-M09）**：主读数仍为合成数据（§3.2 已按此收口）；订正轮补了一条**受限真实数据腿**（`results/realdata/exp_P4RD_01_real_hst_m16.json`，真实 HST M16 f502n 帧 6144² 裁切、生产控制点配方 + 样条重建），但其参照量只能是局部 MAD² 代理（该 drizzle 产品无 ERR/WHT 扩展、噪声相关）⇒ 只能回答"控制点能否代表 cell 尺度局部二阶矩"与"真实结构上稀疏→稠密的可表示性"，**不能**回答绝对方差精度；完整的真实数据腿（含绝对参照）仍由 P1/P2 单元承担。
12. **生产可达性（P4-M06，跨单元）**：**P4 的逐像素重建面在 Phase2 已接通且是默认值**——`snr_path` 冻结枚举 `{dense, sparse_reconstruct, frame_reconstruct}`、`sparse_reconstruct` 为默认，模块适配层在 `snr_path_effective == "sparse_reconstruct"` 分支逐输出像素调用生产 API `p2weight::weight_from_sparse_layer_pixel_prepared`（`lib/infrastructure/scheduler/src/module_adapters.cpp:12581`），其重建器 `SparseSnrReconstructor` 的算子词表即冻结的四档（`phase2_integrate/weight_chain.h`）。帧级链的 `in.sparse = nullptr`（同文件 `:12112`）只表示**稀疏层不经帧级标量链**（该行上方的注释明写「稀疏层的真实消费面 = 下方逐像素权重」），**不是**逐像素面不可达。
    - 仍 fail-closed 的是 `snr_path=dense`（`module_adapters.cpp:11967`，`SNR_PATH_DENSE_UNAVAILABLE`）：Phase1 不产逐像素稠密 SNR 面，**只有这一条口径不可兑现**。
    - 因此 §4.4 的「丢源项 ⇒ 最劣 12 倍」在生产上**有可执行的消费面**（逐像素路径），其成立条件是有效域（§7.17）；帧级标量路径上该机制方向相反（§7.18），两者不可混用。
    - 未做：带**绝对真方差参照**的生产端到端试跑（需要真方差参照产品，本单元无），故 12 倍仍是组件级读数而非端到端实测。
13. **节点相位未按生产相位取数（P4-M04）**：生产 cell_center_v1 相位 = i·Δ+(Δ−1)/2（Δ=64 ⇒ 31.5，取整 32），实验侧路线1 用 `(Δ−1)//2`（31）、路线3 用 `Δ/2`（32）；审查件独立复算该半像素差对 Δ=64 的 E 影响约 0.5%（可接受），但本单元**未**在生产真实相位下逐点重跑。
14. **红队判据改造的落点（P4-M03 部分闭环）**：亮度跟随门、零源负例、"有源对照 37.31"三项经反例证明判别力不足；订正不修改已归档实验（改脚本会使存档与脚本脱钩），而是**新增**非退化判据——[code/fix/fix01_metric_E_and_gates.py] → [results/fix/fix01_metric_E_and_gates.json]（算子敏感亮度门 + 逐臂估计器版零源门 + 偏 10% 源模型对照），并把 37.31 登记为"反解设定的场景读数"，不再作为独立复算陈述。
15. **求值范围与内存表述（P4-M07，跨单元）**：本单元只主张"帧域重建算子按点求值"（自检与 Oracle 均按点调用），**不**主张球面 drizzle 侧不整场驻留——`hp_drizzle_api.cpp` 的 `rebuild_snr` 一次性物化 xy/radec/snr 三组数组（单帧 512² ≈ 10 MB、4096² ≈ 670 MB）后交给 drizzle。审查另指出的"`weight_chain.cpp` 每像素重做 `SparseSnrReconstructor::prepare`"（与其自身注释的性能禁令冲突）同样在 `lib/**` 域外，登记为 P4→P5 接口交接项。


**项目约定豁免**（文献腿 UNRESOLVED，登记为约定、不注文献出处）：quality_factor 0.1/0.5 及其比值口径（负责人已批豁免为项目约定；科学量是比值 0.1/0.5 与 share/absolute 口径差，不豁免）；γ<1e-10 与节点容差 1e-9 为数值卫生；Δ=64 为结构派生量。

16. **M16 物理前向仿真腿的适用边界**：帧视场 12.8″×12.8″、Δ = 64 px，真方差跨幅由源项标度 α 在
    75.8–2.3e4 之间扫描；本腿**不含** IDW 对照档、**不含** 相关长度敏感的任何定标（理由见 §4.8 末段），
    也**不含** M16 三波段同天区（只跑 F657N）与不同指向/滚转角的多帧几何。
17. **「真方差动态范围 1.78–235」不是有效域判据**（被否证假设）：该窗口内稠密口径照样失效，窗口外失效量级相当 ⇒ 它**既非充分也非必要**；它只是 `calibers/exp_P4CAL_02` 6 档对比度扫描里两条「dense 胜出」行的 v 跨幅极值，属 fixture 偶然量。替代口径 = **Δ-格可表示性 `J_Δ = E_eff(cell-oracle)`**（只用真值、与算子无关、与方差动态范围无关、随 Δ 单调增）。盲复算（M16 物理前向、生产默认算子含值域钳制）：`J_Δ` = 0.161(Δ=16) / 0.177(Δ=32) / 0.204(Δ=64) / 0.250(Δ=128) / 0.283(Δ=256)，而 `v_true` 跨幅在五档恒为 2.29e4（不随 α 变）⇒ 门随 Δ 走、不随动态范围走。**`J_Δ` 的已知盲区**：cell 内未分辨**点源**（源项在分子侧）几乎不抬高 `J_Δ`（点源场 0.046 vs 光滑场 0.0397），胞内阶跃边缘则抬高到 0.419 ⇒ `J_Δ` 单独**不是**完整判据，完整口径是正本 §8b 的三因子联合判据「`Δ/ℓ` × σ 场幅度 × 未分辨结构污染」，第三因子按数据来源显式开启 mesh 中值档、不可由控制网格自身推断。
18. **H4 的适用域是「场在 Δ 尺度上平滑」**（本单元读数）：M16 物理前向场上帧级口径丢源项反而更好（E = 1.77e−4 vs 3.54e−1，方向相反），归为 informative-only 分歧。机理是同一腿的排序读数——真实结构场上 `E_frame < E_cell < E_dense`，**跟踪得越少越好**；而 H4 的 12.6 倍结论是在有效域内的光滑解析场上证的（丢源项 1.31e−3 vs 机器零 2.22e−16、极端对比 12.6 倍）。⇒ H4 的**机制陈述**（权形状须与 `σ_w² = σ_slow² + S_src/g` 一致）在其适用域内成立；**「控制值必须携带源项亮度」的普适性**只在逐像素口径成立，在帧级常数口径下**不成立**（帧级口径下源项变化只改标量，形状不受影响）。论文与 README 不宣称 H4 跨口径普遍成立。

## 参考文献

1. D. Shepard. A two-dimensional interpolation function for irregularly-spaced data. *Proc. 1968 23rd ACM National Conference*, 517–524, 1968. DOI 10.1145/800186.810616.（核验：Crossref + ACM DL 条目页，路线1/3 逐字；按分歧台账 A-P4-05 订正旧稿出处）
2. R. Franke. Scattered data interpolation: tests of some method. *Mathematics of Computation* 38, 181–200, 1982. DOI 10.2307/2007474.（核验：Crossref 逐条）
3. G. Y. Lu & D. W. Wong. An adaptive inverse-distance weighting spatial interpolation technique. *Computers & Geosciences* 34, 1044–1055, 2008. DOI 10.1016/j.cageo.2007.07.010.（核验：Crossref）
4. R. G. Keys. Cubic convolution interpolation for digital image processing. *IEEE Trans. Acoust., Speech, Signal Process.* ASSP-29(6), 1153–1160, 1981. DOI 10.1109/TASSP.1981.1163711.（核验：IEEE Xplore 文号页；按分歧台账 A-P4-05 订正：通行 DOI 10.1109/29.90969 解析 404，引用须用本条）
5. B. Zackay & E. O. Ofek. How to coadd images? I. Optimal source detection and photometry using ensembles of images. *ApJ* 836, 187, 2017. DOI 10.3847/1538-4357/836/2/187, arXiv:1512.06872；II. *ApJ* 836, 188, 2017. DOI 10.3847/1538-4357/836/2/188, arXiv:1512.06879.（核验：Crossref + arXiv abs 逐字）
6. K. Horne. An optimal extraction algorithm for CCD spectroscopy. *PASP* 98, 609, 1986. DOI 10.1086/131801.（核验：Crossref 逐条）
7. T. Naylor. An optimal extraction algorithm for imaging photometry. *MNRAS* 296, 339–346, 1998. DOI 10.1046/j.1365-8711.1998.01314.x.（核验：Crossref 逐字段，本单元复核裁定——路线3 台账所记 01333.x 实为 Cropper 别文，判误订正）
8. A. C. Aitken. IV.—On Least Squares and Linear Combination of Observations. *Proc. R. Soc. Edinburgh* 55, 42–48, 1935/1936. DOI 10.1017/S0370164600014346.（DOI 订正：Crossref 逐字段可解析——作者 Aitken A. C.、题名 "IV.—On Least Squares and Linear Combination of Observations"、Proc. R. Soc. Edinb. 55, 42–48、issued 1936；本行原注的 “DOI 未解析” 来自对 10.1017/S0080456800012684 的实测，该 DOI 确为无效（404），但它不是本文的 DOI。承担逆方差定权理论腿）
9. K. M. Górski et al. HEALPix: a framework for high-resolution discretization and fast analysis of data distributed on the sphere. *ApJ* 622, 759–771, 2005. DOI 10.1086/427976, arXiv:astro-ph/0409513.（核验：arXiv + IOP；球面域几何语境）
10. A. F. J. Moffat. A theoretical investigation of focal stellar images in the photographic emulsion and application to photographic photometry. *A&A* 3, 455, 1969.（**未验证**：无 DOI；Crossref 按题名查询无该卷记录；ADS 在线页要求人机校验未能通过——仅以 bibcode 1969A&A.....3..455M 锚定。仅作轮廓族"smooth 幂律核"的出处陈述，**不承担 σ/FWHM 的任何数值判据**）
11. C. de Boor. *A Practical Guide to Splines*, Revised ed., Springer, 2001.（标注级：教科书未在线逐字核验；节点复现/C² 性质由实验独立证实，不依赖本引用）
12. G. Akinshin. *Finite-sample bias-correction factors for the median absolute deviation based on the Harrell–Davis quantile estimator and its trimmed modification*. arXiv:2207.12005, 2022.（核验：arXiv abs 页题名逐字[2026-09 本单元复核]，旧稿题名 "Finite-sample bias correction for the Mean Absolute Deviation" 系转述失真，按 P4-m12 逐字订正；MAD 有限样本偏差语境）
13. P. Astier & P. Antilogus. The shape of the Photon Transfer Curve of CCD sensors. arXiv:1905.08677.（核验：PDF 首页题名逐字——原写的 "of area array CCDs" 非该文题名；PTC 斜率语境）
14. P. J. Rousseeuw & C. Croux. Alternatives to the median absolute deviation. *JASA* 88(424), 1273–1283, 1993. DOI 10.1080/01621459.1993.10476408.（核验：Crossref 元数据；仅作 MAD 效率语境，数值判据以实验闭环为准）
15. I. Trujillo, J. A. L. Aguerri, J. Cepa & C. M. Gutiérrez. The effects of seeing on Sérsic profiles – II. The Moffat PSF. *MNRAS* 328, 977–985, 2001. DOI 10.1046/j.1365-8711.2001.04937.x, arXiv:astro-ph/0109067.（核验：Crossref 题名/卷/页逐字段 + arXiv abs 页 journal-ref；**正文 Eq.(1) 逐字**："PSF(r) = (β−1)/(πα²)[1+(r/α)²]^{−β}, with the full width at half maximum, FWHM = 2α√(2^{1/β}−1)"——Moffat 轮廓的 α↔FWHM 关系即为本单元 §4.6 σ 约定讨论的一手文献腿；该文只给 α，未定义 σ）
