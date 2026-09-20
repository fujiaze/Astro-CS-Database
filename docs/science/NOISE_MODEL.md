# Noise / Variance / Ivar / SNR Science (SCI-NOISE)

> ID: SCI-NOISE-001  范围: SCI-NOISE-001..015 (legacy SNR-001..015)  状态: FROZEN (T104 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-NOISE-001..  模块: snr_estimator (NoiseWeightModelV1)

## 1 目的与非目标

- **目的**：估计校准后空背景随机分量的逐像素方差 `variance` 及其倒数 `ivar=1/variance`，作为 Phase2 逐像素科学权重入 `var(x,y)=a+b·x+c·y` 空间场 + 全局兜底，用于 UPM 控制光度拟合与加权积分。
- **非目标**：不估计测光零点残差散度（SCI-PHOT `sigma_residual` QA）；不输出 PSF 拟合质量 `q_psf`（SCI-PSF）；不生产完整协方差矩阵（Drizzle 后相邻像素相关见 `UNCERTAINTY_AND_COVARIANCE.md`）；生产权重不融合 gain/readnoise 诊断模型（见 §5/§10）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x` | 像素值（校准后 ADU/e⁻ 空背景） | 输入 |
| `variance` | 随机分量方差 `σ_bg²` 或平面预测 `a+b·x+c·y` | `NoiseWeightModelV1.var` |
| `ivar` | `1/variance` (ADU⁻²) | `variance_bg_global` 倒数 / `fill` |
| `σ_bg` | `1.482602218505602·MAD(|x−median|)` | `noise_model.cpp:robust_sigma` |
| `MAD` | `median(|x−median(x)|)` | 同上 |
| `rmax` | 掩膜半径**硬上界** `max(1,r0)·max(1,scale)`（默认 60 px）；实际逐星半径 `r_i` 见 §5a | 掩膜 |
| `a,b,c` | 最小二乘平面 `var(x,y)=a+b·x+c·y` | `snr_noise_model_v1` |
| `variance_floor` | 方差下界 `1e-12`（`max(var,floor)` clamp） | `default_config` |
| `g_model_floor` | 以 `model*` 为 key 的 floor 注册表 | `noise_model.cpp:32,306,764` |
| `gain, read_noise_e` | 诊断模型参数 e-/ADU, e- | `snr_noise_gain_variance` |
| `r_inliers` | Tukey 权重>0 的内点集（SCI-PHOT 复用符号，不混） | QA |
| `degenerate, has_spatial_field` | 退化/空间场标志 | `NoiseWeightModelV1` |
| `r_local(F,FWHM,σ_bg)` | 由「掩膜边缘残余面亮度 = k·σ_bg」导出的无偏所需半径（§5a） | 掩膜 |
| `r_i` | 逐星实际掩膜半径 `clip(r_local(F_i,FWHM_i,k·σ_bg), r_min, rmax)` 经天空预算收缩 | 掩膜 |
| `k` | 掩膜边缘残余面亮度系数，默认 `0.1`（⇒ 残余方差污染 < 1%·σ_bg²） | `noise.mask_k_sigma` |
| `r_min` | 逐星半径下界 `max(1.5 px, 0.75·FWHM_i)` | 掩膜 |
| `N_sky` | 未被掩膜且合法（有限、未饱和）的像素数 | 天空预算 |
| `MASK_LEGACY / MASK_DEGRADED` | 掩膜降级诊断标（信息缺失 / 天空预算收缩后仍退化） | `NoiseWeightModelV1.mask_degraded` |

## 3 物理量和单位

- `x, σ_bg, √variance`: ADU（或 e⁻，同输入标度）；`variance`: ADU²；`ivar`: ADU⁻²；`a`: ADU², `b,c`: ADU²/pixel；`gain`: e-/ADU；`read_noise_e`: e-；`signal`: ADU；掩膜半径/坐标: pixel；`floor`: ADU²。

## 3a 坐标 frame

方差估计在**像素域**进行（8×8 patch 网格与平面场 `var(x,y)=a+b·x+c·y` 的 x,y 均为内部 0-based 像素坐标，GLOSSARY `pixel_coordinate`）；无 WCS/天球参与；帧身份沿用 `frame_id`（DATA_SEMANTICS §5），估计结果随帧 payload 唯一。

## 4 输入有效域

- 维度 `h>0,w>0`，`data` 非空且含有限值；`min_samples`（patch 样本数阈）默认 64；`rmax` 是**逐星掩膜半径的硬上界**（默认 60 px），实际半径 `r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)` 由源通量、PSF 尺度与天空预算导出（§5/§5a）；调用方应提供逐星通量与 FWHM（生产调用点 psf 块 `row[2]=flux` / `row[5]=fwhm`，见 §6），未提供时按 §5a 回调规则降级并置 `MASK_LEGACY` 诊断标。**饱和域（claim SC-008 / SAT-001）**：`x ≥ saturation_level` 的像素为**饱和像素**，**不参与 blank-sky 统计**（输入有效域规则，**无条件生效**；§5 的 5σ 裁剪不是它的替代品——裁剪的崩溃点是样本中位数，饱和核+源翼一旦占 patch 多数即双双失效）。电平来源优先级 = 显式 `cfg.saturation_level>0` > 帧元数据 FITS `SATURATE` > `DATAMAX`。**`0`/负/非有限 = 「未提供电平」（unset），不等于「无饱和」**；未提供时调用方**必须**在帧产品写显式降级声明 `NOISE_SATURATION_FILTER=DISABLED_NO_METADATA`（禁止静默），并由 §11 饱和域 oracle 的负例约束。<!-- (SAT-001 订正 2026-09-17；claim SC-008；依据 DATA_SEMANTICS §13.1「data 行：饱和像素过滤不统计」、NOISE_ESTIMATION §13.4「饱和电平以上像素排除」；外部标准 LSST `ip_isr.IsrTaskConfig.doSaturation` 默认 `True` 且置 `SAT` 面（lsst/ip_isr isrTask.py L431-442，2026-09-17 抓取）；复跑 run/PROJECT-GOVERNANCE-01/SAT-001/expS1_saturation.py) --><!-- (MASK-002 订正 2026-09-17；claim SC-009；依据 reports/PROJECT-GOVERNANCE-01/research/MASK-001_掩膜语义重新推导.md §3.3/§3.4/§5.2(d)；复跑 run/PROJECT-GOVERNANCE-01/MASK-001/{expB_radius_bias.py,expD_domain.py}) -->
  <!-- (SCI-FIX-NOISE 订正 2026-09-16；claim SC-002；依据 reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md §2 EXP-1/2/3/9 与 run/PROJECT-GOVERNANCE-01/R-5/exp1..exp9，生产 API 零改动复跑) -->
- 平面场仅 `enable_spatial_field==1 && n_control_points>=4` **且控制点几何张成二维**时启用，否则退化为全局常量场（`has_spatial_field=0`）；几何判据 = 中心化控制点点云 Gram 矩阵特征值比 `λlo/λhi ≥ 1/16`（等价点云条件数 `κ=√(λhi/λlo) ≤ 4`，无量纲；绝对阈值 `|det|>1e-24` 已废除，claim SC-002 / DISP-NOISE-010）。
- `variance_floor>0` 时 `max(var,floor)` clamp 生效；`<=0` 时 `fill` 内部回退 `1e-12`（`fill` 阶段）。
- `gain<=0` 时 `snr_noise_gain_variance` 返回 0（诊断路径，不入生产）。

## 5 连续定义

```text
patch grid 8×8；星点掩膜**逐星半径** r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)，再按天空预算收缩（§5a）
σ_bg = 1.482602218505602 · median(|x − median(x)|)   # MAD→σ，Gaussian 假设
稳健裁剪: cosmic/hot 5σ 阈，≤2 轮
控制点: 合格 patch（样本数 ≥ min_samples）的 patch variance
空间场: 最小二乘平面 var(x,y) = a + b·x + c·y；负预测 clamp 至 variance_floor (1e-12)
几何退化: 控制点近共线 (λlo/λhi < 1/16) ⇒ has_spatial_field=0，fill 走全局常量场
全局兜底: 合格 patch variance 的稳健中位数 vmed
variance_bg_global = max(vmed, variance_floor)      # 合格 patch 支: vmed 已是方差 [ADU²]
variance_bg_global = max(sig², variance_floor)      # 无合格 patch 的全帧退化支: sig 是 σ [ADU]
g_model_floor: 以 model 指针为 key 注册 floor，snr_noise_model_v1_free 时按指针擦除，无全局共享
ivar = 1 / max(variance, floor)   # fill 阶段 max(a+b·x+c·y, floor)；control 点亦 max(patch_var, floor)
```

```text
Gain/Readnoise 诊断模型 (仅 diagnostic, NOT FOR PRODUCTION):
  var_ADU = max(signal,0)/gain + (read_noise_e / gain)²   # signal: ADU, gain: e-/ADU, rn: e-
  用途仅 SNR-005 诊断交叉验证 (noise_model_science_test.cpp:238-272)，生产 source==0 empirical 不融合
```

与 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:32-546,553-796` 一致。

### 5a 掩膜半径的物理导出（claim SC-009 / MASK-002）

掩膜的唯一目的是让天空样本「无源」。「掩膜半径与星亮度解耦」**作为物理陈述是错的**：同一 `r=10 px` 下把最亮星通量从 `10³` 提到 `10⁶` ADU（FWHM=3 px，Moffat β=2.5，8 seeds），`σ_bg` 偏差从 `+0.02%` 升到 `+2.29%`，无偏所需半径 `6.6 → 28.1 px`（对数律）。正确口径 = 逐星半径由「掩膜边缘残余面亮度 ≤ k·σ_bg」导出（取 `k=0.1` ⇒ 残余方差污染 < 1%·σ_bg² ⇒ `σ_bg` 偏差 ≤ 0.5%，即本文件 §11 冻结 5% oracle 的 1/10 余量）：

```text
Gaussian : r_local = σ_p·sqrt(2·ln(F/(2π σ_p²·k·σ_bg)))
Moffat β : r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),   α = FWHM/(2·sqrt(2^(1/β)−1))

r_i = clip( r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax )
  k     = 0.1                        # noise.mask_k_sigma
  r_min = max(1.5 px, 0.75·FWHM_i)   # noise.mask_r_min_px / noise.mask_fwhm_floor_scale（至少覆盖 PSF 核心）
  rmax  = max(1,r0)·max(1,scale) = 60 px   # **硬上界**（保大 PSF 重翼），不是操作默认半径
天空预算收缩: 取最大 s∈(0,1] 使 n_qualified(s) ≥ 8 且 N_sky(s) ≥ 9216；无可行 s ⇒ rc=1（§7 空 support 不传播不变）
  预算推导（a priori）: 单 patch 相对误差 c ≈ 1.152/√N，中位数效率 1.25 ⇒ SE(σ̂)/σ ≈ 1.44/√N_sky；取 SE ≤ 1.5% ⇒ N_sky ≥ 9216
回调（信息缺失，按序生效）:
  F_i 缺失        ⇒ r_i = 4·FWHM_i（实测 F ≤ 10⁵ ADU 时残余偏差 ≤ 0.70%），置 MASK_LEGACY
  FWHM_i 亦缺失   ⇒ 退回统一 rmax，置 MASK_LEGACY
  σ_bg 两遍法     ⇒ 第一遍 4·FWHM 估 σ_bg，第二遍回代（σ_bg 偏差 1% ⇒ r_local 偏差 ≈ 0.2%，可忽略）
```

实测（12 seeds；1024²/320 星；Moffat β=2.5；FWHM=3 px）：`r=10 px` ⇒ 偏差 `+0.13%`、RMSE `0.17%`、`n_qualified=64`；`r=60 px` ⇒ 偏差 `−0.28%`、RMSE `0.76%`（**4.6×**）、`n_qualified=35` ⇒ 60 px 在该帧是纯损失。可用域（λ = N_s·π·rmax²/A）：固定 60 px 要 `N_s ≤ A/7540`（4K² 2 225 星、1K² **139** 星、256² **8.7** 星），远窄于天文帧实际星密度 ⇒ 60 px 只能作**硬上界**。策略对照（54 帧/策略 = {256²,1K²,4K²}×{稀疏,中等,密集}×6 seeds，真值 σ_bg=5 ADU）：worst |σ 偏差| 现行统一 60 px **6.93%** / 显式失败 2.63% / 降级打标 8.44% / 仅按预算收缩 4.77% / **本节逐星自适应 1.11%**；平均零权重像素占比 14.8% → **0**；权场效率损失（含失权帧）13.28% → **0.013%**；方差梯度帧恢复比 **1.489–1.505**（真值 1.5）vs 现行常量场 1.000。来源 `reports/PROJECT-GOVERNANCE-01/research/MASK-001_掩膜语义重新推导.md` §3.3/§3.4/§5.3。

## 6 假设

- 空背景在 patch 尺度局部平稳；源星点可被**逐星半径掩膜**（§5a）与 5σ 裁剪分离；
- 掩膜半径**随源通量与 PSF 尺度变化**（§5a）：`r_i = r_local(F_i, FWHM_i, k=0.1·σ_bg)` 经天空预算收缩。模块 C ABI 此前只收坐标属**接口现状**，不是物理约束——生产调用点（orchestrator psf 块 9 列行：`row[2]=flux`、`row[5]=fwhm`、`row[6]=amplitude`，字段语义锚 `orchestrator.cpp:4558-4562`）本已持有该信息，ABI 已扩展为可选逐星数组（`star_flux`/`star_fwhm`，claim SC-009）；缺省时按 §5a 回调规则降级并置 `MASK_LEGACY`；
- 增益/读出噪声诊断公式仅在 `signal≈μ` 的 Poisson+读出噪声假设下有意义，不替代经验 `variance`。

## 7 独立不变量

- **常量场不变量**：常数输入 `x=C` 时 `σ_bg=0` ⇒ `has_spatial_field=0, degenerate` 全局常量场，不产生伪梯度。
- **掩膜无偏性不变量**：默认掩膜下残余源污染对 `σ_bg` 的偏差 **≤ 2%**（§11 源污染 oracle 验收阈，12 seeds；`k=0.1σ_bg` 是设计取值，EXP-C 54 帧实测 worst 1.11%，MASK-002 门 512²/3 seeds 实测 worst 0.15%）；逐星半径 `r_i` 对 `F_i` 与 `FWHM_i` **单调不减**（同 `(F_i,FWHM_i,σ_bg)` 逐位可复现）。「半径与亮度解耦」**不是**不变量（claim SC-009）。
- **天空预算不变量**：任何掩膜方案必须留下 `n_qualified ≥ 8` 且 `N_sky ≥ 9216`；不满足 ⇒ 按 §5a 收缩半径；收缩到 `r_min` 仍不满足 ⇒ `ivar=0, r=1` 拒绝加权（「空 support 不传播」保持）。
- **空 support 不传播**：无合格 patch 时 `ivar=0, r=1` 拒绝加权，不产生伪有效权重；**对外产品面即 `variance=0 ∧ ivar=0`（禁写 NaN：NaN 保留给产品损坏，见 DATA_SEMANTICS §4a F-UNC-001 裁决）。**
- **Floor 夹逼不变量**：任意**可用** `variance` 经 `max(...,1e-12)` 后 `ivar` 有限、`variance≥1e-12`；`variance_floor` 单位为 **ADU²**、默认 1e-12 属冻结项（变更须走工程变更），clamp **只作用于可用方差**；**不可用一律 `ivar=0`（不得由 clamp 产生）**；每帧生效 floor 及其来源（默认/注册表 key）必须随帧产品登记。
- **量纲一致**：`variance` [ADU²] → `ivar` [ADU⁻²] 倒数关系精确，`gain` 模型量纲 `max(signal,0)/gain` [ADU²] 无量纲混。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 掩膜覆盖过大（收缩后仍 `n_qualified < 8` 或 `N_sky` 不足） | 先按 §5a 天空预算收缩逐星半径；收缩到 `r_min` 仍不可行 ⇒ `degenerate=1, ivar=0, r=1` 拒（不产生伪权重）；收缩生效但 `n_qualified < 8` ⇒ 置 `MASK_DEGRADED` | EXP-A/EXP-C/EXP-D（MASK-001 §3.2/§3.4/§5.3） |
| 无合格 patch（收缩后仍无） | `degenerate=1`；若**全部未掩膜** sky 样本 < `max(min_samples, 9216)` ⇒ `ivar=0,r=1` 拒（**收紧**：现行 `min_samples/2=32` 像素可为整帧定权重，现按 `SE(σ̂)/σ ≈ 1.144/√N_sky ≤ 1.5%` 要求 `N_sky ≥ 9216`，与 §5a 同一预算常数，claim SC-009）；否则 `degenerate=1` 全局常量场 `has_spatial_field=0, r=0` fallback 并置 `MASK_DEGRADED` 诊断标 | `noise_model.cpp:471-511` |
| 全帧 NaN/饱和 | 饱和像素按 §4「饱和域」剔除；**全帧无任何合法 sky 样本**（NaN+饱和全剔，或样本 < §5a 预算）⇒ `degenerate=1, ivar_bg_global=0, r=1`；**电平未提供（unset）时本行的饱和支不可达**——这正是 §4 强制显式降级声明的理由（claim SC-008） | noise_model.cpp:64-68,471-511（valid_pixel/collect_patch_sky）；SAT-001 复跑 run/PROJECT-GOVERNANCE-01/SAT-001/expS1_saturation.py |
| `variance_floor<=0` | `fill` 回退 `1e-12` clamp | `noise_model.cpp:731-733` |
| `gain<=0` | `snr_noise_gain_variance` 返回 0 | `noise_model.cpp:790` |
| `star_x/y` 非有限 | 掩膜跳过该星，不污染统计 | 参数校验 |
| `MAD=0` | `σ_bg=0` ⇒ 退化路径（见上） | `robust_sigma` |

## 9 精度策略

- FP64 全链路；MAD 常数**唯一权威写法** `1.482602218505602`（= 1/Φ⁻¹(3/4) 的 double 字面量；11 位简写 `1.4826022185` 只能出现在"约等于"语境，与之绝对差 **5.602e-12**、相对差 **3.779e-12**）；`q_psf` 的 `0.7316727929211932`（10–90% trimmed mean \|residual\| →σ）与上述 MAD 常数不可互换（前者 trimmed mean，后者 MAD）。
- `variance_floor=1e-12` 保证 `ivar` 有限；平面预测负值 clamp 至 floor。
- 5σ 裁剪 ≤2 轮，避免过度剔除。

## 9a 专属问题回答（SCI-003 指定问题逐项）

- **signal/noise/blank sky**：`x`=校准后空背景像素值（ADU 同标度）；noise=空背景随机分量；blank sky 样本域=星点**逐星半径掩膜**（§5a，硬上界 `rmax`）+ 5σ≤2 轮裁剪后的合格 patch（§5）。
- **sigma_cal_rel / 零点标准误（消费侧口径，SCI-PHOT 引用）**：`sigma_cal_rel = ln10·sigma_residual` 是**逐星定标散度**（dex → 相对），**不是**零点（median 位置）的不确定度；零点统计标准误为 `sigma_location_se_dex ≈ 1.253·sigma_residual/√N_eff`（`1.253=√(π/2)`，median 的位置标准误），`sigma_location_se_mag = 2.5·sigma_location_se_dex`（实现 `snr_phot_cal_quality`；代码已按此实现，本行仅补文档口径，claim SC-002）。
- **SNR**：本合同不产出 SNR 图。消费侧可以构成**逐像素探测显著性** `signal/√variance`，但该量**不含源泊松项**（`variance` 仅空背景），不是源的通量信噪比；源通量 SNR 必须另行定义（逐源 `σ_F`，见 CONTROL_WEIGHT_SNR.md §1）。本层唯一产出为 `variance/ivar`（GLOSSARY `variance/ivar`）。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S5) -->
- **variance/ivar**：`variance`=ADU²（平面场或全局兜底），`ivar=1/max(variance,floor)` 精确倒数（§7 量纲不变量）；零/负/NaN 条件：负平面预测 clamp 至 floor、`floor<=0` 回退 `1e-12`、非有限输入在参数域拒绝（§8）。
- **Poisson+read noise**：`var_ADU=max(signal,0)/gain+(read_noise_e/gain)²` **仅诊断路径**（SNR-005 交叉验证），生产唯一基线为 empirical MAD（`source==0`，NO-01 P0，§10）。
- **权重归一与适用域**：`ivar` 作为 Phase2 逐像素科学权重直接入加权（归一在消费侧 `Σw/Σ`），适用域=空背景随机分量；不含源泊松项/系统项/协方差（Drizzle 后相关见 UNCERTAINTY_AND_COVARIANCE.md）；`ivar=0` 显式表示不可用，禁止伪装（§7 空 support 不传播）。

## 10 不可接受变化

- 将 `snr_noise_gain_variance` 结果融合至生产 `variance/ivar`（`source==0 empirical` 为唯一生产基线，即使 header 有 gain 亦不融合，`NO-01 P0`）；
- 在**未按 §5a 扩展 ABI 提供逐星通量/FWHM 且未重冻结**的前提下，擅自把半径改为按亮度自适应（**正确做法是 §5a**——禁止的是无依据的自适应，不是自适应本身）；
- 取消或绕过天空预算判据（`n_qualified ≥ 8`、`N_sky ≥ 9216`）而直接产出权重场；
- 改变 `variance_floor` 默认值 `1e-12` 或 `g_model_floor` 的指针 key 隔离语义；
- 将 `q_psf`/`photometric scatter` 混为逐像素 `variance`；
- 改变 `8×8` patch 网格或 `5σ ≤2 轮` 裁剪策略而无 SCI 变更。

## 11 验证 Oracle

- **Gaussian 合成**：`N(0,σ²)` 空背景合成帧（`σ=5 ADU`），经验 `σ_bg` 在 `5%` 内复现（`SNR-004`）。
- **Poisson 诊断交叉**：`μ/gain + rn²/gain²` 的 `var_th` 与经验 `variance_bg_global` 在 5% 内一致（`SNR-005, 238-272`），仅诊断通过，不入生产。
- **平面场恢复**：注入线性梯度 `var(x,y)=a+b·x+c·y` 场，拟合 `a,b,c` 在 10% 内复现（`SNR-006`）。**逐像素真值口径（EXP-206 订正，2026-09-20）**：`max|v̂/v_true−1|` 的**解析真值必须显式含 patch 内天光梯度项** `((dμ/dx)²+(dμ/dy)²)·P²/12`（`P` = patch 边长），否则对强梯度帧**过严**——E12 实测**漏项时判 +49.9%（假红）**、含项（本实验解析项 = 26.04 ADU²）后 **+3.56%/+3.67%/+4.05%**（≤10% 门）；等价可接受口径 = 「**同几何独立参考**」。系数恢复门（`|b̂/b−1|`、`|ĉ/c−1|` ≤10%）不受影响。
- **不变性门**：常量场、空 patch 拒绝、`floor` 夹逼、量纲 `ivar=1/var` 四门（`TST-NOISE-INV-*`）。
- **源污染 oracle（claim SC-009 / MASK-001；补结构性缺口）**：合成帧 = `N(0,5²)` 空背景 + `N_s` 颗 Moffat(β=2.5) 星（幂律亮度 `dN/dF ∝ F⁻²`，`F∈[2×10²,10⁵]` ADU，FWHM=3 px，星位随机）；默认掩膜下 `|σ̂_bg/σ_bg − 1| ≤ 2%`（12 seeds）且 `n_qualified ≥ 8`、`N_sky ≥ 9216`。**负例（门必须能红）**：① 半径固定 60 px 且 256²/50 星 ⇒ 必须 `rc=1`（整帧退化）；② 半径固定 2 px 且 `F_max=10⁶` ADU ⇒ 必须检出 `|σ 偏差| > 2%`。③ 半径与 `F_i`/`FWHM_i` 不单调 ⇒ 必须红（`o2_mask_radius_monotone`）。复跑 `run/PROJECT-GOVERNANCE-01/MASK-001/expB_radius_bias.py`、`expC_policies.py`、`expD_domain.py`；生产门见 ALG §13.4 TEST-NOISE-DESIGN-001 的 FIX-NOISE-D/H。
- **饱和域 oracle（claim SC-008 / SAT-001；补结构性缺口）**：校准后饱和平台不是常数——平台电平 `SAT` 经平场除法带响应残差 `ε`（相对 1% ⇒ 平台散布 `0.01·SAT`，对 sky 仅 `0.01·μ`），故「平台像素」在值域上是**有噪声的整段总体**，不能指望 5σ 裁剪兜底。判据：**正例**（电平已提供）⇒ 污染控制点数不增加、权场中位数方差与平台 patch `ctrl_variance` **严格下降**、帧平均 `ivar` ≥ 3× 未过滤臂；**负例（门必须能红）** ① 电平未提供（`saturation_level=0`）且帧含亮星饱和核（512²，平台半径 44 px，掩膜 6 px，`F=5×10¹³ ADU`）⇒ 平台 patch `ctrl_variance` `6.87×10⁸ → 1.74×10⁸ ADU²`（提供电平后仍受源翼限制，故只判严格下降）、帧平均 `ivar` 比 `0.218`（丢失 4.6× 权重）、`sigma_bg_global` **两臂均 5.08 ADU（差 0%）**——缺陷只在逐像素权重场可见，这正是它的静默性；**域与限制**：现行实现以**校准后**值判饱和，平台经平场响应残差 `ε` 抹平后 `x ≥ 电平` 只能移除平台总体的一部分，全平台 patch 的干净恢复必须靠 §5a 掩膜半径（MASK-002），本过滤**不是**掩膜的替代品；② 帧无 `SATURATE`/`DATAMAX` 时必须存在显式降级声明且**不得**被读成「无饱和」。复跑 `run/PROJECT-GOVERNANCE-01/SAT-001/expS1_saturation.py`；生产门 `ctest -R p1noise_saturation`（含 `p1noise_saturation_wiring`）。
- **Python 参考**：NumPy 对同 `data` 的 `median/MAD/5σ裁剪/平面最小二乘` 复算 `variance/ivar`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-NOISE-001` `snr_noise_model_v1` 空背景方差估计（8×8 patch + MAD + 平面场）
- `ALG-NOISE-002` `snr_noise_model_v1_fill` 平面 clamp + `ivar=1/var` 填充
- `ALG-NOISE-003` `snr_noise_gain_variance` 诊断模型（仅 SNR-005）

## 13 追溯与测试

- 权威文件: `docs/science/NOISE_MODEL.md` (SCI-NOISE-001..015)
- 实现: `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` (`snr_noise_model_v1, _f64, _fill, _free, snr_noise_gain_variance, g_model_floor`), `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`
- 公开 API: `snr_noise_model_v1, snr_noise_model_v1_f64, snr_noise_model_v1_fill, snr_noise_model_v1_free, snr_noise_gain_variance, snr_noise_model_v1_default_config`
- 测试: `TST-NOISE-001..015` (`noise_model_science_test.cpp`), `TST-NOISE-INV-*` 四门、不变量，`TST-NOISE-FAIL-*` 空 patch/NaN 拒绝（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Newberry, M. V. 1991, PASP, 103, 122（DOI 10.1086/132801，SCI-001 已核验原文存在性）：Poisson+读出噪声分解的 S/N 建模上下文——文章级定位，本合同 §5 诊断公式为 Project-defined，不引用其具体公式号。
2. MAD→σ 换算 `1.482602218505602 = 1/Φ⁻¹(3/4)`：标准正态 MAD 分位恒等式（`Φ⁻¹(3/4) = 0.6744897501960817`，double 逐位等于 `1/1.482602218505602`），教科书级，Project-defined 采纳。SCI-PHOT 侧的 4 位写法 `0.6745` 与全精度值相对差 **+1.5196e-05**（等价地 `1/0.6745` 相对差 **−1.5196e-05**），属该侧容许截断，**不得与本节冻结值互换**（V12-N-03，claim SC-002）。
3. Tukey biweight 内点权重（`r_inliers` 复用）：SCI-PHOT §14/PMS 文献链，本层仅消费 QA 集合不重复估计。
5. 掩膜半径默认值的导出依据（claim SC-009 / MASK-002）：以 §11 源污染 oracle 为判据，`k=0.1`、`r_min=max(1.5 px, 0.75·FWHM)`、硬上界 `rmax=max(1,r0)·max(1,scale)=60 px`、`N_sky ≥ 9216`、`n_qualified ≥ 8`。`k=0.1` 下 `r_local(F=10⁵ ADU, FWHM=3 px, β=2.5) = 17.6 px`（Gaussian 极限同阶）；实测 `r=10 px` 偏差 +0.13%（RMSE 0.17%），统一 60 px 在 1024²/320 星上 RMSE 0.76%（**4.6×**）、在 256²/50 星上整帧退化（rc=1）。**缺口的形式化闭合**：`10`/`6`/`60 px` 此前只由 `noise_model.cpp:626-627` 字面量 → `NOISE_ESTIMATION.md:134` → `config/defaults.json` 的 `source_ref` 三段构成**循环引用**（无第一性依据，MASK-001 §1.4），故此处按 §14.4 对 `min_samples` 的同款先例给出可复跑推导。来源 `reports/PROJECT-GOVERNANCE-01/research/MASK-001_掩膜语义重新推导.md` §3.3/§3.4/§5.1，复跑 `run/PROJECT-GOVERNANCE-01/MASK-001/expB_radius_bias.py`、`expD_domain.py`。
4. 默认值 `min_samples=64` 的导出依据（claim SC-002）：8×8 patch（P=64）下以本文件 §11 冻结的 5% oracle 为判据，`min_samples=5` 时单 patch 偏差 −19.2%、全局 `sigma_bg_global` 偏差 −25.1%、5% 门通过率 **0.6%**；`min_samples=64` 为 −1.25%/−1.7%、通过率 **92.8%**（纯高斯蒙特卡洛；常规无掩膜帧上 5 与 64 逐位同输出，差异只在 patch 残余样本 5~63 的掩膜 regime）。来源 `reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md` §2 EXP-1/2/3/9，复跑 `run/PROJECT-GOVERNANCE-01/R-5/exp1_mad_bias_mc.py`、`exp2_pipeline_mc.py`、`exp3_prod_threshold.py`。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5/§5a 公式与常数。

- **MAD→σ 常数 1.482602218505602 = 1/Φ⁻¹(3/4)**：标准正态分位恒等式；稳健性/有限样本校正见 Rousseeuw & Croux 1993, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。
- **稳健尺度与 σ-clipping**：Hoaglin, Mosteller & Tukey (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。
- **背景网格 + 稳健 σ 估计**：Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（SExtractor §3 背景网格与 σ 估计；DOI 10.1051/aas:1996164）；源码 SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）back.c/makeback。**差异**：SExtractor 用 mode/median 与迭代 σ，AstroCS 用 8×8 patch 的 MAD + 最小二乘平面场，二者**不等价**（网格尺寸、尺度估计器、场基不同），引用仅作方法学对照。
- **多尺度稳健噪声（MRS/N*）**：Starck, J.-L. & Murtagh, F. 1998, “Automatic Noise Estimation from the Multiresolution Support”, PASP 110, 193（DOI 10.1086/316124，starlet 小波；PixInsight ImageWeighting §2.4 的 MRS 出处，本轮逐字核验 2026-09-17）；Starck, J.-L. & Murtagh, F. 2006, Astronomical Image and Data Analysis, 2nd ed., Springer（ISBN 978-3-540-33023-3）Ch.2–3；Starck, Donoho & Candès 2003, A&A 398, 785（DOI 10.1051/0004-6361:20021569）。**核验状态**：文章级；AstroCS 现状**未采用**小波 MRS/N*，该条只作选型对照。
- **Poisson+read noise 诊断式**：Newberry 1991 PASP 103, 122；Janesick 2001 SPIE PM83 Ch.2；Howell 2006 Handbook of CCD Astronomy Ch.4。
- **饱和过滤**：LSST ip_isr（GPL-3.0）IsrTaskConfig.doSaturation 与 SAT 面；FITS SATURATE/DATAMAX 关键字（FITS Standard）。
- **掩膜半径的解析导出**：Gaussian/Moffat 轮廓尾翼积分属 Project-defined 推导（§5a）；Moffat 轮廓出处见 Moffat 1969, A&A 3, 455；PSF 尺度与 FWHM 换算见 docs/science/PSF.md §5。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

### 14a.1 SCI-001-S1 补充：PixInsight N* 常数与本项目口径（2026-09-17）

- PixInsight 官方 N* 稳健噪声（.pidoc 式[14][15]）：`N*_MAD=2.48308·MAD(R*)`、`N*_Sn=2.03636·S_n(R*)`（`S_n` 为 Rousseeuw & Croux 1993 尺度估计；常数用 10000 幅 4096² 高斯白噪声 bootstrap 标定）。**AstroCS 未采用这两个常数**：本项目用标准正态 MAD 一致化 `1.482602218505602`（§9），与 PI 的 2.48308 定义域不同，**不得互换**。
- PCL 2.10.4 `PSFSignalEstimator.h` 的 `NStar()` 默认取 `NStar_Sn`（2.03636），而 `Estimates::NStar` 文档注释写 2.05435——PI 自身存在版本/注释冲突（登记 UNRESOLVED U3）；本分片在 4×10⁶ 高斯样本上复核标准 Sn 的 σ 一致化为 1.1926，未能复现 2.03636（`run/RELEASE-01/science/exp_math.py` E1）。
- 逐像素 ivar 的开源对照：SWarp `COADD_WEIGHTED` 输出方差 `=1/Σ(1/var_k)`（GPL-3.0，`src/coadd.c:1279-1311`）；SExtractor `Var(F)=Σ(σ_bkg²+F_pix/gain)`（GPL-3.0，`src/analyse.c:200-203,304-310`）；SEP 孔径 `σ²_sum=Σvar_pix·w²+Σ/gain`（LGPL-3.0，`src/aperture.c:516-570`）；photutils `σ_tot²=σ_bkg²+I/g_eff`（BSD-3-Clause，`photutils/utils/errors.py:91-92`）。与本文件 §5 诊断式同构，可作对拍基线。
- 背景/权重重标定先例：SWarp `RESCALE_WEIGHTS` 用各背景网格实测 σ 与权重图比值的中位数重标定 `sigfac`（GPL-3.0，`src/back.c:361-389`），可作为本项目权重/方差标定门的对照。

## 15 Acceptance

- §11 Oracle 全过：Gaussian 5% 复现、Poisson 诊断 5% 交叉（仅诊断）、平面场 10% 恢复、四不变量门、Python 参考 rtol 1e-9、**源污染 oracle（含星帧，正例 + 三条负例；claim SC-009）**、**饱和域 oracle（正例 + 两条负例；claim SC-008）**；
- §8 全部退化路径显式（无合格 patch/NaN/floor/gain≤0）；饱和过滤状态在帧产品里**显式可读**（`NOISE_SATURATION_FILTER`，claim SC-008）；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-003 转换：Gaussian/Poisson/常量/blank sky/outlier/small-N 用例、estimator bias 与 ivar 边界（零/负/NaN→ivar=0）登记 SYN-003。
