# P5 加性天光去除与无接缝叠加 · 实验报告

**单元**：`实验/additive-sky-seamless/`（科学链第五点）
**性质**：整合历史正本实验（C1–C7）＋独立重做三路＋两个补实验的定稿实验报告。
纯解析腿（route1/c3、route2/e5…）**逐位复现**固化读数；探针腿 C1/C3/C7 与固化读数
**不一致**，且当前生产构建下 C1 与 C5 各有门为红 ⇒ `code/run_all.sh` 退出码为 1。
逐条见 §6 末条与 §7.2。
**裁决来源**：控制裁决台账所在目录已不在仓内；本稿可复核面为 §8 的三条证据腿
（文献 `refs.md` / 开源 `results/evidence_code.json` / 仓内实测 `results/**`），
仅由台账支撑、未落在三者之上的陈述一律标为登记面。

---

## 1. 假说

- **H1（主假说）**：纯加性天光世界中，UPM 四要素（排除自身的参考面、阻尼 α≈0.5、拟合/堆叠权重同源、末端残差场扣除）使任意覆盖子集上的加权阶跃恒为零，产品保留公共背景 B_ref；帧间乘性差须先由 Phase1 吸收。
- **H2（张力假说）**：猜想失效边界 = 帧间天光差含「参考面不可表示且相干」的分量；残余接缝与该分量 RMS 线性相关，显著尺度由节点间距控制。
- **H3（定权假说）**：control_variance = k_corr·(π/2)·σ_bg²/N_retained 在 N≥65 渐近域内成立；有限 N 处的偏差方向、幅度与 k_corr 的几何依赖可标定。
- **H4（判据假说）**：max|rel_step| ≤ 1e-2 构成「确定性地板＋统计检出」的复合判据，其确定性行为、统计行为与漏检面可被独立复现。

每条假说配「真值无效应⇒度量归零」负例，判据非退化。

## 2. 方法

- **历史正本**（`code/c1–c7*.py`，固定 seed，读数 `results/c1–c7*.json`）：HST 模板＋物理前向仿真世界，生产代码只读探针（`upm_probe.cpp`/`sky_probe.cpp`），证据分级 data/meta。
- **审计重做三路**（归档 `code/audit_rework/{route1,route2,route3}/`，读数 `results/audit_rework/`）：纯 numpy 解析合成，每路独立实现、独立 fixture，负例齐备。
  - 路线1：C1 中位数方差精确积分、C2 k_corr drizzle 前向 MC、C3 接缝门、C4 方差比、C5 Huber 效率、C6 可辨识性/dof、C7 权重两级口径、C8 尺度容差、C9 表示边界、C10 锚取证。
  - 路线2：e1–e10 与路线1 对应面＋smoothing λ 偏差–方差、σ_floor 跨尺度。
  - 路线3：exp01–exp12 十二项独立重做（含零锚权重、权重臂、尺度不变性、质量因子）。
- **补实验**：`supp_507_relstep/`（ea：×5.07 尺度扫描复算；eb：rel_step_max=0.1 标定性）；`supp_control_variance/`（P2 单元，D-07 定稿口径三脚本）。
- **seed**：路线2/路线3/补实验 = 20250926（写死于脚本头/`p5c_common.SEED_BASE`）；路线1 按主题分段（接缝门 20260319、方差比 20260320、表示边界 20260325、其余 20260926 系）。seed 不可经命令行覆盖。

## 3. 数据

1. HST 真实信号模板＋完整物理前向（历史正本）；2. 纯解析合成＋负例（三路＋补实验）；3. testdata 真实数据 M42，**两条腿必须分开读**：(3a) 单元内 C7 = M1/T3 **同一指向的 4 帧** 512² crop（**不是 49 帧**），其 6 条「边界」是 x = 128/192/…/448 的**合成 cell 边界**（`code/c7_realdata.py` 的 BOUND），**不是真实帧足迹边界**；(3b) **真实帧足迹边界**的读数来自**生产 49 帧端到端产品**（逐字冻结记录 `results/production_e2e_seam_record.json`）。详见 REPORT_paper.md §3(3a)/(3b)。

## 4. 结果

（完整数字与来源路线见 `results/audit_rework_summary.json`；此处按假说列主数。）

### 4.1 H1：纯加性世界（成立，域内）

| 量 | 读数（含不确定度与正负对照） | 来源 |
|---|---|---|
| 背景接缝中位（未校正→δ_k 臂） | 4.80 → 0.383 e⁻（12.5×）；6 条边界的逐条散布：未校正 \|step\| ∈ [1.384, 6.545] e⁻、中位 4.802；δ_k 臂 \|step\| ∈ [0.041, 0.850] e⁻、中位 0.383（off-locus `excess` 口径：中位 5.108 → 0.355 e⁻，14.4×）。**正负对照**：全减臂 `excess` 中位 0.275 e⁻，比 δ_k 臂更小但背景被减掉 ⇒ 度量更小不等于做法正确 | [实验:c1_additive.py]（`seam_native`，n=6 边界） |
| 产品中位 vs B_ref（**δ 臂**，`product.delta_median`） | 299.17 ≈ 297.33 e⁻（比值 1.0062，保留公共背景）；同 fixture **全减臂** `product.full_median` = 0.711 e⁻（对照） | [实验:c3_public_plane.py] |
| 全减背景退化臂 | 0.845 e⁻、**48.4% 负值像素**（`full_neg_frac = 0.4839820861816406`，出自 `code/c3_public_plane.py:131` ⇒ `results/c3_public_plane.json`）；度量上反而更好（0.275 < 0.383）——必须禁止 | [实验:c3_public_plane.py] |
| 星孔径通量守恒（正负对照之外的第三腿） | n=16 颗最亮星，相对变化中位 −3.50e-8、最大 \|Δ\| 6.60e-6 ⇒ 加性扣除不动星流 | [实验:c1_additive.py]（`star_flux`） |
| 乘性前提（Phase1 残差） | 0.560 → 5.89e-4；未做 Phase1 接缝 27.37 → 做后 6.31 e⁻（4.33×） | [实验:c2_multiplicative.py；P1 单元] |
| 非退化判据分离度 | 退化 0.09σ vs 非退化 36.4σ；样本外假阳性 0/60（双边阈值 \|D−mu\|>5σ，mu=−5.10 e⁻、σ=0.627 e⁻）；A=10 e⁻ 检出率 0.975；5σ 检测限 6.98 e⁻ | [实验:c4_seam_criterion.py]（120 次实现） |
| 稀疏/稠密等价 | max\|Δ\| = 3.11e-15（1,048,576 点）；14,001 B vs 8,389,129 B（0.167%）；峰值 RSS 按需 64² 块 11,688 kB < 稠密物化 512² 14,568 kB | [实验:c6_sparse_dense.py] |
| 真实数据 | `level_ratio` 中位 0.9805（0.9386–1.0503，n=6 对）；乘性斜率中位 0.995（0.806–1.323）、corr(slope,intercept)=−0.9981、动态范围仅 8.5 ADU ⇒ 截距不可辨识 | [实验:c7_realdata.py] |

### 4.2 H3：定权（D-07/D-08 终裁口径）

| 量 | 读数 | 来源 |
|---|---|---|
| 渐近域 | N≥65 时 MC 偏差 <2% | [实验:route1/c1_median_variance.py] |
| N=5 纯公式口径 | 渐近式**高估 ≈9.5%**（精确积分 0.314159 vs 0.286834，9.53%）——保守方向 | [推导]＋[实验:route1/c1、route2/e1（+9.6%）、route3/q3（0.913×formula）] |
| N=5 端到端口径 | ≤±1.5% | [实验:P2 补实验 supp_control_variance] |
| N=5 生产链亮端裁剪臂 | 低估 1.3–3.2% | [实验:P2 补实验 production_chain_control_variance.py] |
| k_corr 身份 | 标定几何专属实测带 1.27–1.43（受控复现 1.3445±0.0416）；1.3883 为带内一次实现 | [实验:p3_kcorr/g1_canonical] |
| k_geo | 紧凑 1.27±0.03 / 全 touched 1.43–1.45 / 分散 1.00 / 扫描全域 1.00–5.0；多帧 1.424/1.338/1.328 | [实验:p3_kcorr/g2–g4] |
| 冻结 1.4 失保守 | N=5 紧凑端低估 32%（k_corr≈2.05±0.09）；源 583–600″/px 端低估约 2× | [实验:p3_kcorr/g6_fit] |
| 口径差 | share/absolute 14.8%（0.14830）；伪 ivar 1.314e26；公共因子消去 1.5e-16 | [实验:route1/c7、route2/e6、route3/q9/q10] |
| 堆叠方差用例 | 1/Σw = 0.05931 | [实验:route2/e6] |

### 4.3 H4：接缝门（含漏检面）

| 量 | 读数 | 来源 |
|---|---|---|
| 确定性下限 | 闭式 `gate/(1−gate/2)` = **0.0100502512**（= 1.0050251%，**不是**对数口径 e^{1e-2}−1 = 0.01005017——两者在 1.0050% 精度内巧合一致，口径归属按 P5-15 订正）；固化档：Δ/L = 1.00503%（键名 `0.010050`）读数 0.01000005 **判红**、Δ/L = 1.0050% 解析 0.00999975 **判绿**、1.0049% 判绿 | [推导]＋[实验:route1/c3_seam_gate.py] |
| H₀ 散布 | 1.151e-3 vs 公式 1.187e-3（0.970）；虚警 0/4000 | [实验:route1/c3] |
| 多重性检出 @1.05% | 解析 0.6462/0.8748/0.9843、MC 0.6518/0.8700/0.9828（n_s = 1/2/4；n_s=1 处两口径差 0.006 = MC 波动） | [实验:route1/c3] |
| 平滑过渡漏检面 | 观察台阶 ×2d/w；2% 名义台阶判绿 | [实验:route1/c3] |
| **光滑斜坡伪阳面（新增登记）** | 判据量含梯度项 `rel_step = 2d·ρ + Δ/bg` ⇒ ρ ≥ gate/(2d) = 0.25%/px **单独**即判红而真值无接缝：ρ=0.26%/px ⇒ 0.0104 判红、ρ=0.10%/px ⇒ 0.0040 但 d 扫描量 0.0160 越门 1.6×；纯斜坡 d 扫描比值恒 4、真台阶恒 1 | [实验:code/seam_gate_gradient_scan.py，seed=20260928] |
| 梯度相消漏检面 | 1.005%＋反号梯度 → 0.06% 判绿（漏检面 ≈1.73%，01 D-57） | [实验:route1/c3] |
| 负例 | 无台阶 median rel_step=0，4000 MC 全绿 | [实验:route1/c3] |
| f<1/2 失明限定 | 仅无噪极限；M42 级噪声 f=0.3、Δ/L=5% 读数 1.03e-2 | [实验:route3/exp01]（A-P5-05） |
| 方差比正交性 | 电平台阶下 VR≈0.977/rel_step 判红；方差伪影下 rel_step 判绿/VR≈3.96；VR_pool=1.25/2.01/5.03 @Δ/σ=1/2/4 | [实验:route1/c4、route2/e4、route3/exp02]（A-P5-04） |

### 4.4 H2：失效边界（A-P5-11 改写口径）

| 量 | 读数 | 来源 |
|---|---|---|
| 峰值/长尺度比 | ≈8（原 8.03 vs 复算 8.02、第三实现 9.21）——跨实现稳健 | [实验:supp_507_relstep/ea_507_scale_scan.py] |
| 端点比（原 ×5.07） | 不复现：2.95（带噪）/2.43（无噪）/1.78（gauge）/1.71（RMS） | [实验:ea_507_scale_scan.py] |
| 形状 | 非单调（峰 100 px、50 px 回落）；肘点 s≲2h≈256 px | [实验:ea_507_scale_scan.py] |
| 线性结构 | 成立；斜率 fixture 专属（0.798/0.8964 vs 0.207/0.8006），不迁移 | [实验:c1_additive.py、route1/c9、route3/exp08] |
| 负例 | λ→∞（可表示）残余接缝精确为 0 | [实验:route1/c9] |

### 4.5 判据面收口

- Huber δ=1.345：δ(0.95)=1.3449975（闭式二分）；MC 效率 0.948/0.9507；σ_floor 主导域退化为 median [实验:route1/c5、route2/e5/e10]。
- rel_step_max=0.1：前提改正（非 IRLS 步长上限）＋不可标定（B0=5 假红 45–60%、B0=1000 永不红）＋结构性失明（≤4/112 邻对）＋松 20× ⇒ **判据面除名**（A-P5-10）[实验:supp_507_relstep/eb_relstep_calibration.py]。
- 幻觉锚：修复规格 12 项证伪 vs 3 项证实 ⇒ §9/§10 整段回炉（A-P5-03）[实验:route1/c10_anchor_forensics.py]。
- dof：E[χ²]=n_obs−r_eff（Andrae 式(9) [1]）；秩亏时 n_params 分母使 χ²_red 高估 1.0714/1.0581（方向词按 A-P5-01 订正）[实验:route1/c6、route3/exp06]。
- H_solve 恒真门：判决面 = `H_red` 条件数、唯一阈值 = `rank_rtol` τ，数值岭只剩派生角色 `λ_eff = τ·mean(diag(H_red))`；`κ(H_solve)` 只作求解稳定性诊断（κ = 23.14 的「生产 0.1·mean(diag)」是**已退休的 fixture 假定**，不得再当生产值引用；同 fixture 取 λ = τ·mean(diag) 时 κ = 2.214e10 而 `r_eff = 23 < n_free = 24` 逐位不变 ⇒ 恒真门）（A-P5-08）[实验:route3/exp07、route2/e8]。
- 尺度容差：scale_obs=5.26e13 处 ULP=0.0078125；「≈1 ulp」为实现依赖声明（A-P5-02）[实验:route1/c8]。
- quality_factor_initial=0.5：基准值精确抵消（max|Δθ|=0.0）；0.1/0.5 豁免为项目约定（A-P5-12）[实验:route2/e6、route3/exp12]。

### 4.6 归零负例（真值无效应 ⇒ 度量归零或报警）

固化读数 `results/sky_plane_zero_negative.json`，脚本 `code/sky_plane_zero_negative.py`，
7 门全过、退出码 0。已有的归零负例（接缝门 `rel_step`、方差比、`k_corr`、可辨识性、
表示边界）都作用在解析 fixture 上；本表补的是**生产天光面链路**
（`p2_sky_plane_build` → `δ_k` → 加权叠加 → 接缝度量）此前没有的那一条。

| 门 | 构造 | 读数 | 档 |
|---|---|---|---|
| `N1a_zero_effect_zeroes_seam` | 无噪声、信号与天光逐帧相同且沿 x 平滑 | `max\|excess\|` = **5.68e-14 e⁻**（6 条边界全测）⇒ 精确归零 | 归零 |
| `N1b_zero_injection_reads_zero` | 注入幅度 0 的同一构造 | **0.0 e⁻** | 归零 |
| `N1b_injected_step_is_detected` | 逐帧一致注入 Δ = 12 e⁻ 盒装电平台阶（x = 256） | 该边界 `excess` = **10.893 e⁻**（≥ 注入量的 50%） | 能红 |
| `N1b_metric_has_scale` | 注入幅度扫描 {0, 6, 12, 24} e⁻ | 读数单调增，斜率 **0.908**（≥ 0.3）⇒ 非恒真门 | 有标度 |
| `N2a_zero_effect_zeroes_delta` | 生产 `p2_sky_plane_build`，帧间天光差 ≡ 0 | `max\|δ_k\|` = **2.79e-10 e⁻** ≤ 1e-9 ⇒ 逐帧扣除量归零 | 归零 |
| `N2b_zero_effect_zeroes_seam` | 同上，δ_k 臂残余接缝 | `max\|excess\|` = **9.46e-12 e⁻** | 归零 |
| `N2c_injected_step_is_detected` | 同上，注入 Δ = 12 e⁻ | `max\|δ_k\|` = **60.66 e⁻**（≠ 0）且该边界 `excess` 翻红 | 能红 |

**读法**：`N2a` 与 `N2b` 合起来说明「多退少补把接缝压下去」这条正向结论**不是**度量的
恒真表现——在帧间天光差为零时，生产求解器给出的扣除量与残余接缝同时归零；
`N2c` 说明同一构造下注入缺陷必被捕获。`N1b` 的斜率 0.908（而非 1.0）是度量自身的
低读偏置：基线拟合窗 `base_win = 64 px` 与 cell 边界间距 64 px 同量级，偏置窗必然跨过
相邻边界 ⇒ `excess` 对孤立盒装台阶按下界解释，检测结论不受影响。

## 5. 结论

1. H1 成立（可表示域内），背景保留与接缝压缩均有生产代码实测；退化做法被非退化判据锁定。
2. H3 定稿：公式本体成立；N=5 三口径（+9.5%/±1.5%/−1.3~−3.2%，D-07）；k_corr 两因子查表＋引用义务（D-08）。
3. H4 成立且漏检面齐备；判据面仅存接缝门 1e-2 与 Huber δ=1.345；rel_step_max=0.1 除名。
4. H2 结构性主张成立、标度统计量改写为峰值比 ≈8＋非单调＋肘点 2h；斜率为 fixture 专属。

## 6. 诚实边界

- 补实验实验 A 为独立 Python 求解器（非生产二进制）：50 px 端点 2.60 vs 已发布 4.52、800 px 中段 1.03 vs 1.84；1600 px 端点与峰值点吻合；已发布序列本身非单调——任何端点比都不稳健，这正是 A-P5-11 结论的一部分。
- 信号模板为解析弱场替代 HST 真实模板（cell 中位数泄漏已压至 ~0.9 e⁻ 并登记）；RMS 伪影口径带噪臂被两次独立 Poisson 实现之差主导。
- 实验 B 为 2 帧玩具几何；步长上限是反事实注入参数，生产无对应物；κ 扫描单一场景单一污染率。
- 接缝门 fixture 未含双线性亚像素方差因子与足迹多边形几何（二阶效应 4.5e-3）。
- **三类数据的分歧（P5-05）**：②类（解析合成）绿、③a（4 帧单指向）绿，而 **③b（生产 49 帧全帧）就「无接缝」主张红**——门只覆盖 114/196 条边界（82 条 `not_interior` 不进判据），且同一读数块内三个独立诊断量全部越门（`rel_step_net_max = 1.2948e-02`、`rel_step_d4x_max = 2.1915e-02`、`legacy_rel_max = 2.6110e-02`）。本报告不主张「三类一致」。
- **生产读数取证性质**：③b 是**冻结记录**（源 run 树为过程产物、产品 FITS 已被回收）⇒ 可核对面 = 记录内 sha256（4/4 逐字一致）＋自检器 `code/production_e2e_record_check.py`（17 项检查，能红能绿），**不构成可现场重跑的端到端证据**。
- **像素尺度订正（P5-04）**：全稿撤下无来源的 `0.989″/px`；实验网格 = 1.0″/px（`code/c3_public_plane.py:27`），生产 = 0.9404″/px（记录 `pixel_scale_arcsec_derived`）⇒ h = 0.0355° = 127.8 px、肘点 2h = 255.6 px。
- **噪声相关长度敏感度（判据级）**：真实 drz 噪声已相关化，`m16_sampling`/`m16_scene` 生成的是逐像素独立噪声 ⇒ 对相关长度敏感的判据不得用合成帧定标。逐条判定见 `README.md` §6.A：被判**敏感**的 6 组（接缝门虚警/检出曲线、`C4` 检测限与假阳性、`C4 N1`、`control_ivar` 与 `C5 W1/W2`、`k_geo`、`A10` 代理量）**全部**用逐像素独立噪声的解析合成或 Poisson 实现定标，**没有一条**用 M16 合成帧定标；M16 物理腿（方差闭合、seeing/孔径、base-rate 传输）只承担生成器实现自洽的验证，不承载任何定标量。接缝判据的地板与漏检面、天光平面的节点间距肘点两条主线落在「不敏感/弱敏感」档，不受该边界约束。
- **共享链阻塞（影响可跑面，不影响读数）**：`实验/shared/synthetic/m16_scene.py` 的 `render_m16_frame` 内置的 base-rate 传输不变量，在**未经改动的** `scenes/m16_nebula_core.json` 上即以 `max|implied/base − 1| = 0.9997 > tol` 抛 `AssertionError` ⇒ 本单元三条 `m16_scene` 腿（`exp_variance_closure` / `exp_realbase_consistency` / `exp_a6_seeing_aperture`）当前不可跑。共享链的自检面（`m16_scene --selftest`）调用 `base_rate_transport_residual` 但**不调用** `render_m16_frame`，故该阻塞不被自检覆盖，自检仍全绿。
- **纯解析腿与探针腿的可复现性不同**：纯解析腿逐位复现——`route1/c3_seam_gate.json`、`route2/e5_huber_delta_1345.json`、`route2/e3_seam_gate_1e-2.json` 与固化拷贝 **byte-identical**；探针腿不可逐位复现——同一 fixture 下（`C1` 未校正臂接缝 `un_med`、`un_max` 与固化值逐位相同）生产求解器输出已变（`n_params` 58→49、`rank` 49→49、`kappa` 3.16e7→4.93e3、`iterations` 15→20、`model_hash` 变），读数差 1e-3–1e-1。
- **退出码 1 的准确归因**：七条腿逐条状态 = C1 **FAILED**（`A6_subset_invariance` 0.0236→0.4078，门 `< 0.05`；`A9_final_gauge_near_noop` 0.0037→0.1327，门 `< 0.01`）、C2 OK、C3 OK、C4 OK、**C5 FAILED**（`W1_control_ivar_best`：`margin_snr2` 0.2592→0.1753 < 门槛 0.20；漏入 0.0245/0.0651/0.3203→0.0761/0.1216/0.3590，**13× 优势降为 4.7×**）、C6 OK、C7 OK。⇒ **rc=1 是三条门阈值被跨越，既不是脚本报错、也不是路径断裂或缺件**。
- **三条红门的共同前提**：历史档 `n_params = 58 > rank = 49`，即求解系统在 9 维零空间上取值（最小范数代表）；当前档 `n_params = 49 = rank`，零空间消失。`A6`（任意覆盖子集 cell 级加权均值不变）与 `A9`（`final_gauge` 在 `m_full_frame=1` 上为 no-op）都是**依赖零空间吸收规范自由度**的严格不变性，它们在秩亏最小范数解下近乎恒真（0.0236 e⁻ / 0.0037 e⁻），在满秩解下必须重新推导。**这三门的重新裁决属生产侧**：按新模型重推阈值（须给依据）、改写判据含义、或登记为「当前实现下该要素不可检验」——三者择一，不由实验单元单方面放宽。
- **`results/c1–c7*.json` 的效力**：只作历史读数，不得当作当前生产构建的行为证据；C3/C7 的定性结论（背景保留、δ 臂压缩接缝、全减臂 48.4% 负值）在新读数下方向不变，**C5 的定量优势不可复现**。C1–C7 原始运行的代码版本在仓内**不可判定**（`run/SCI-403/` 只有日志与探针输出，无运行记录文件）。

- 生产链被估量 y 的合同语义（裁剪后中位数 vs 全样本中位数）待裁决（P2 链，不属本单元）。
- support_min=0.2 出处未定（降级为实现默认）；min_cluster_size=3 随回炉删除；Tikhonov 1963 题录未核不作依据；Serfling/Kendall pinpoint 书目级标注。
- w∝SNR² 与正本 §5/§10 互斥，禁令维持；本几何泄漏 18% 不得迁移（A-P5-06）。

## 7. 复现命令

全部命令**从仓库根执行**，路径相对仓库根，seed 固定在脚本内，无网络，零 git 写。
逐条入口、依赖与当前状态见 `README.md` §7.2；本节给出三段式骨架。

```bash
# ① 公共前置步：合成数据物理链自检（四个组件全跑，约 2.5 min）
bash 实验/shared/synthetic/run_selftests.sh

# ② 审计重做三路 + 两个补实验（纯 python3+numpy，37 个脚本，退出码 0）
bash 实验/additive-sky-seamless/code/audit_rework/run_all.sh

# ③ 历史正本 C1–C7 + 归零负例 + 冻结记录自检 + 出图（已含 ① 作为第 0 步）
bash 实验/additive-sky-seamless/code/run_all.sh
```

### 7.1 单项入口

```bash
python3 实验/additive-sky-seamless/code/sky_plane_zero_negative.py
python3 实验/additive-sky-seamless/code/production_e2e_record_check.py
python3 实验/additive-sky-seamless/code/production_e2e_record_check.py --no-source   # 只判内部断言
python3 实验/additive-sky-seamless/code/seam_gate_gradient_scan.py
python3 实验/additive-sky-seamless/code/c1_additive.py        # 其余 c2..c7 同理
python3 实验/additive-sky-seamless/code/make_figures.py
python3 实验/additive-sky-seamless/code/reverse_verify/data_matrix/exp1_sky_poisson_snr.py
```

`code/run_all.sh` 是**非破坏性**的：跑前备份固化读数到 `run/SCI-403/results_prior/`，
跑后把本次新读数另存到 `run/SCI-403/results_head/`，再把固化读数原样放回 `results/`。
重跑不写仓库证据面。单独跑 `c1..c7` 仍会覆写 `results/c{1..7}_*.json`
（脚本直写该路径），需要保留现场时先自行备份。

### 7.2 退出码的读法

`run_all.sh` 的退出码是**各腿退出码的逻辑或**，而每条实验脚本的退出码
= `该腿判据失败数是否为 0`（`Gates.summary()` → `n_fail`）。它**不表示**
「读数与 `results/*.json` 逐位一致」。因此：

- 退出码 0 ⇏ 固化读数可复现；
- 退出码 1 有两种成因，必须分开读：
  （a）**判据层**：某门阈值被跨越（当前生产构建下为 `C1 A6` / `C1 A9` / `C5 W1`）；
  （b）**依赖层**：探针构建失败或某腿因缺输入报错。

两类成因的区分办法：看 `run/SCI-403/logs/` 下该腿日志末行的
`[mem_guard] … exit=N` 与门行；依赖层失败的日志里没有门行。

### 7.3 产物落点

| 产物 | 落点 | 入库 |
|---|---|:-:|
| 三路脚本的中间结果 | `code/audit_rework/results/*.json` | 否（该目录自带 `.gitignore`，重跑不覆盖固化拷贝 `results/audit_rework/<route>/`） |
| 历史正本固化读数 | `results/c{1..7}_*.json` | 是（代码直写，重跑前须备份） |
| 归零负例读数 | `results/sky_plane_zero_negative.json` | 是 |
| 冻结记录自检 / 出图 | `results/production_e2e_seam_record.json`（只读）、`results/figs/*.png` | 是 |
| 探针二进制、逐腿日志、备份与本次读数 | `run/SCI-403/**` | 否（gitignore） |
| M16 重建链与 data_matrix 产物 | `run/reverse_verify/**` | 否（gitignore） |

单脚本 CPU ≤ 90 s（`C4` 例外：本机负载下可达 30 min），历史正本全量 < 60 min，
审计重做全量 ≈ 10 min。

## 8. 佐证来源

规范 03 §1 要求每条科学结论具备三类一手证据。三条腿的完整台账、逐字段核对状态与
条目在下列位置；本节给出「哪条结论落在哪条腿上」的对应关系，并标明每条腿的可核面。

### 8.1 文献腿（论文或标准：DOI/arXiv 可解析，被引内容与原文一致）

条目台账与核验状态：`refs.md`（只收一手 VERIFIED 条目，标注级条目单列且不进正文引用面）。
进入正文引用面的 9 条（编号连续，格式按规范 02 §4）：

| 编号 | 文献 | 本稿引用面 |
|:-:|---|---|
| [1] | Andrae et al. 2010, arXiv:1012.3754 | 自由度公式 E[χ²] = n_obs − r_eff，式(9) 逐字 |
| [2] | Casertano et al. 2000, AJ 120, 2747 | 背景测光/定权的行业先例 |
| [3] | Clopper & Pearson 1934, Biometrika 26(4), 404 | 检出率区间的口径 |
| [4] | Fruchter & Hook 2002, PASP 114, 144 | 仅引「输出像素非独立」论断（`k_corr` 的来源），不含 k_corr 数值 |
| [5] | Gruen et al. 2014, PASP 126, 158 | 仅作稳健叠加伪迹剔除先例，不作背景匹配依据 |
| [6] | Holland & Welsch 1977, Comm. Statist. A6, 813 | δ = 1.345 的**归属**（取值由本稿闭式复算） |
| [7] | Huber 1964, Ann. Math. Statist. 35(1), 73 | M-估计框架 |
| [8] | Padmanabhan et al. 2008, ApJ 674, 1217 | SDSS ubercalibration 方法学先例（乘性刻度） |
| [9] | Serfling 1980 | 中位数方差渐近式（书目级标注） |

**未能核验者不进正文引用面**：drizzle 类会议论文集条目无可解析 DOI，
改以 [4]（同行评议、含该实现输出噪声性质的推导）承担文献侧佐证。

### 8.2 开源科学代码腿（项目 + 版本 + 文件:行，并回到其引用文献核对）

台账：`results/evidence_code.json`（35 条，逐条带 `project` / `path:line` / 做法 / 核验状态）。
本稿实际据以支撑结论的四条：

| 项目 | 位置 | 做法 | 支撑本稿哪条结论 |
|---|---|---|---|
| SWarp | `src/back.c:59,317,719,801`、`src/coadd.c:427-430,1295-1296` | 背景**加性**减除（`*data -= *convert_backdata`），coadd 处按权重叠加 | 「加性天光须显式建模」与「全减背景不是唯一做法」 |
| SExtractor | `src/back.c:55,675,751,833,1096,1098` | 网格背景 + 中位滤波 + 稳健估计 | `σ_bg = 1.4826·MAD` 的行业口径 |
| Siril | `src/algos/background_extraction.c:1019,1025,1389-1390,1397,1989,2229`（GitLab `free-astro/siril`，master@48ceaa3，GPL-3.0，**只读不复制**） | `img[i] -= background[i]; img[i] += background_mean;` —— 减背景后**加回公共均值** | 「保留一个公共面」是工业实现的选择 ⇒ 支持 §4.1 的 `B_ref` 保留结论 |
| photutils | `photutils/background/background_2d.py:64,244-246,311-312,875`（BSD-3） | 2D 背景网格 + 样条/中值滤波 + sigma 裁剪 | 天光面作为**稀疏面 + 局部稳健估计**的表示方式 |

### 8.3 仓内实测腿（固定 seed + 复现命令 + 实测输出）

| 面 | 位置 | 可核性 |
|---|---|---|
| 审计重做三路 + 两个补实验 | `code/audit_rework/{route1,route2,route3,supp_*}/*.py`；固化读数 `results/audit_rework/`；汇总 `results/audit_rework_summary.json`（每个数字注明来源路线与固化文件） | **可现场重跑**：`bash 实验/additive-sky-seamless/code/audit_rework/run_all.sh`，37 个脚本、退出码 0 |
| 历史正本 C1–C7 | `code/c{1..7}_*.py`；固化读数 `results/c{1..7}_*.json`（每份含 `gates.rows`，逐门带 id/判据/实测值/是否通过） | **可重跑但读数不逐位复现**（见 §6 末条与 §7）；引用须连同该限定 |
| 生产 49 帧接缝读数 | `results/production_e2e_seam_record.json`（逐字冻结记录）+ 自检器 `code/production_e2e_record_check.py` | 源产品 FITS 已回收 ⇒ 可核面 = 记录内哈希 + 自检器（13 项内部断言，4 项源文件核对；注入缺陷 4/4 判红） |
| 天光平面链路归零负例 | `code/sky_plane_zero_negative.py`；固化读数 `results/sky_plane_zero_negative.json` | **可现场重跑**；7 门，含解析臂与生产链臂 |
| 上游证据 | `实验/absolute-snr/results/*.json`（帧 SNR = F_signal/σ_F；天光扫描斜率 −0.4879/−0.4972 vs 理论 −0.5；三孔径域图；`SNR_comb² = ΣSNR_k²` 闭合 2.2e-16；天光采样权重必须用 `control_ivar`） | 跨单元引用；本单元 C5 独立复现了该权重结论 |

### 8.4 三腿缺一不可的地方

| 结论 | 文献腿 | 开源腿 | 仓内实测腿 |
|---|:-:|:-:|:-:|
| 加性天光须显式建模、且**保留公共面** | [1][5][8] | SWarp / Siril / photutils | C1 `A4`、C3 `B3/B4`、C7 `R3` |
| `control_ivar` 定权优于均匀与 SNR² | [3][9] | SExtractor | C5 `W1/W2`、route1 `c1`、route2 `e1` |
| 接缝门 1e-2 有确定性地板 1.0050% | [3] | —（纯解析，无开源对应物，按规范 03 §1 给出自洽推导） | route1 `c3`、route2 `e3`、route3 `exp01` |
| 无接缝 ⟺ 公共面可表示 | [4] | SWarp | C1 `A10`、route1 `c9`、route3 `exp08`、`supp_507_relstep/ea` |
| 判据面仅存两级锚 | [6][7] | — | route1 `c5`/`c10`、route2 `e5`、`supp_507_relstep/eb` |

**裁决台账的可复核面**：本稿的裁决台账与各路 `report.md` 原件不在仓内。
可复核面因此只保留上表三条腿；
凡仅由台账支撑、未落在仓内读数或 `refs.md` 条目上的陈述，本稿一律标为登记面
（见 §6），不作证据。
