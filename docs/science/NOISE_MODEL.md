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
| `rmax` | 掩膜半径 `max(1,r0)·max(1,scale)`（fixed conservative） | 掩膜 |
| `a,b,c` | 最小二乘平面 `var(x,y)=a+b·x+c·y` | `snr_noise_model_v1` |
| `variance_floor` | 方差下界 `1e-12`（`max(var,floor)` clamp） | `default_config` |
| `g_model_floor` | 以 `model*` 为 key 的 floor 注册表 | `noise_model.cpp:32,151,474` |
| `gain, read_noise_e` | 诊断模型参数 e-/ADU, e- | `snr_noise_gain_variance` |
| `r_inliers` | Tukey 权重>0 的内点集（SCI-PHOT 复用符号，不混） | QA |
| `degenerate, has_spatial_field` | 退化/空间场标志 | `NoiseWeightModelV1` |

## 3 物理量和单位

- `x, σ_bg, √variance`: ADU（或 e⁻，同输入标度）；`variance`: ADU²；`ivar`: ADU⁻²；`a`: ADU², `b,c`: ADU²/pixel；`gain`: e-/ADU；`read_noise_e`: e-；`signal`: ADU；掩膜半径/坐标: pixel；`floor`: ADU²。

## 3a 坐标 frame

方差估计在**像素域**进行（8×8 patch 网格与平面场 `var(x,y)=a+b·x+c·y` 的 x,y 均为内部 0-based 像素坐标，GLOSSARY `pixel_coordinate`）；无 WCS/天球参与；帧身份沿用 `frame_id`（DATA_SEMANTICS §5），估计结果随帧 payload 唯一。

## 4 输入有效域

- 维度 `h>0,w>0`，`data` 非空且含有限值；`min_samples`（patch 样本数阈）默认 64；`rmax` 为固定值，不按星亮度/振幅缩放（API 仅 `star_x/y` 无 amplitude，见 §6）。
  <!-- (SCI-FIX-NOISE 订正 2026-09-16；claim SC-002；依据 reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md §2 EXP-1/2/3/9 与 run/PROJECT-GOVERNANCE-01/R-5/exp1..exp9，生产 API 零改动复跑) -->
- 平面场仅 `enable_spatial_field==1 && n_control_points>=4` **且控制点几何张成二维**时启用，否则退化为全局常量场（`has_spatial_field=0`）；几何判据 = 中心化控制点点云 Gram 矩阵特征值比 `λlo/λhi ≥ 1/16`（等价点云条件数 `κ=√(λhi/λlo) ≤ 4`，无量纲；绝对阈值 `|det|>1e-24` 已废除，claim SC-002 / DISP-NOISE-010）。
- `variance_floor>0` 时 `max(var,floor)` clamp 生效；`<=0` 时 `fill` 内部回退 `1e-12`（`fill` 阶段）。
- `gain<=0` 时 `snr_noise_gain_variance` 返回 0（诊断路径，不入生产）。

## 5 连续定义

```text
patch grid 8×8；星点掩膜 fixed conservative rmax = max(1,r0)·max(1,scale)（统一半径，不按亮度缩放）
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

与 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:32-151,235-281,362-507` 一致。

## 6 假设

- 空背景在 patch 尺度局部平稳；源星点可被 fixed conservative 掩膜与 5σ 裁剪分离；
- 掩膜半径与星亮度解耦（API 无 amplitude 输入，统一 `rmax`；若需 PSF-aware adaptive mask 须先扩展 API 并重冻结）；
- 增益/读出噪声诊断公式仅在 `signal≈μ` 的 Poisson+读出噪声假设下有意义，不替代经验 `variance`。

## 7 独立不变量

- **常量场不变量**：常数输入 `x=C` 时 `σ_bg=0` ⇒ `has_spatial_field=0, degenerate` 全局常量场，不产生伪梯度。
- **掩膜解耦不变量**：`rmax` 与输入振幅无关，亮星与暗星掩膜半径相同（fixed conservative 已冻结）。
- **空 support 不传播**：无合格 patch 时 `ivar=0, r=1` 拒绝加权，不产生伪有效权重。
- **Floor 夹逼不变量**：任意 `variance` 经 `max(...,1e-12)` 后 `ivar` 有限、`variance≥1e-12`。
- **量纲一致**：`variance` [ADU²] → `ivar` [ADU⁻²] 倒数关系精确，`gain` 模型量纲 `max(signal,0)/gain` [ADU²] 无量纲混。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无合格 patch | `degenerate=1`, 若 sky 样本`<min_samples/2`或`robust_sigma`非有限/≤0 ⇒ `ivar=0,r=1` 拒；否则 `degenerate=1` 全局常量场 `has_spatial_field=0, r=0` fallback | `noise_model.cpp:235-260` |
| 全帧 NaN/饱和 | `degenerate=1, ivar_bg_global=0, r=1` | 同上 |
| `variance_floor<=0` | `fill` 回退 `1e-12` clamp | `noise_model.cpp:443-445` |
| `gain<=0` | `snr_noise_gain_variance` 返回 0 | `noise_model.cpp:500` |
| `star_x/y` 非有限 | 掩膜跳过该星，不污染统计 | 参数校验 |
| `MAD=0` | `σ_bg=0` ⇒ 退化路径（见上） | `robust_sigma` |

## 9 精度策略

- FP64 全链路；MAD 常数**唯一权威写法** `1.482602218505602`（= 1/Φ⁻¹(3/4) 的 double 字面量；11 位简写 `1.4826022185` 只能出现在"约等于"语境，与之绝对差 **5.602e-12**、相对差 **3.779e-12**）；`q_psf` 的 `0.7316727929211932`（10–90% trimmed mean \|residual\| →σ）与上述 MAD 常数不可互换（前者 trimmed mean，后者 MAD）。
- `variance_floor=1e-12` 保证 `ivar` 有限；平面预测负值 clamp 至 floor。
- 5σ 裁剪 ≤2 轮，避免过度剔除。

## 9a 专属问题回答（SCI-003 指定问题逐项）

- **signal/noise/blank sky**：`x`=校准后空背景像素值（ADU 同标度）；noise=空背景随机分量；blank sky 样本域=星点 `fixed conservative rmax` 掩膜 + 5σ≤2 轮裁剪后的合格 patch（§5）。
- **sigma_cal_rel / 零点标准误（消费侧口径，SCI-PHOT 引用）**：`sigma_cal_rel = ln10·sigma_residual` 是**逐星定标散度**（dex → 相对），**不是**零点（median 位置）的不确定度；零点统计标准误为 `sigma_location_se_dex ≈ 1.253·sigma_residual/√N_eff`（`1.253=√(π/2)`，median 的位置标准误），`sigma_location_se_mag = 2.5·sigma_location_se_dex`（实现 `snr_phot_cal_quality`；代码已按此实现，本行仅补文档口径，claim SC-002）。
- **SNR**：本合同不产出 SNR 图。消费侧可以构成**逐像素探测显著性** `signal/√variance`，但该量**不含源泊松项**（`variance` 仅空背景），不是源的通量信噪比；源通量 SNR 必须另行定义（逐源 `σ_F`，见 CONTROL_WEIGHT_SNR.md §1）。本层唯一产出为 `variance/ivar`（GLOSSARY `variance/ivar`）。<!-- (P5-SNR 订正 2026-09-14，负责人授权；依据 PHOTOMETRY_LITERATURE_REVIEW D.2 S5) -->
- **variance/ivar**：`variance`=ADU²（平面场或全局兜底），`ivar=1/max(variance,floor)` 精确倒数（§7 量纲不变量）；零/负/NaN 条件：负平面预测 clamp 至 floor、`floor<=0` 回退 `1e-12`、非有限输入在参数域拒绝（§8）。
- **Poisson+read noise**：`var_ADU=max(signal,0)/gain+(read_noise_e/gain)²` **仅诊断路径**（SNR-005 交叉验证），生产唯一基线为 empirical MAD（`source==0`，NO-01 P0，§10）。
- **权重归一与适用域**：`ivar` 作为 Phase2 逐像素科学权重直接入加权（归一在消费侧 `Σw/Σ`），适用域=空背景随机分量；不含源泊松项/系统项/协方差（Drizzle 后相关见 UNCERTAINTY_AND_COVARIANCE.md）；`ivar=0` 显式表示不可用，禁止伪装（§7 空 support 不传播）。

## 10 不可接受变化

- 将 `snr_noise_gain_variance` 结果融合至生产 `variance/ivar`（`source==0 empirical` 为唯一生产基线，即使 header 有 gain 亦不融合，`NO-01 P0`）；
- 将掩膜改为按振幅/星亮度自适应而不扩展 API 并重冻结；
- 改变 `variance_floor` 默认值 `1e-12` 或 `g_model_floor` 的指针 key 隔离语义；
- 将 `q_psf`/`photometric scatter` 混为逐像素 `variance`；
- 改变 `8×8` patch 网格或 `5σ ≤2 轮` 裁剪策略而无 SCI 变更。

## 11 验证 Oracle

- **Gaussian 合成**：`N(0,σ²)` 空背景合成帧（`σ=5 ADU`），经验 `σ_bg` 在 `5%` 内复现（`SNR-004`）。
- **Poisson 诊断交叉**：`μ/gain + rn²/gain²` 的 `var_th` 与经验 `variance_bg_global` 在 5% 内一致（`SNR-005, 238-272`），仅诊断通过，不入生产。
- **平面场恢复**：注入线性梯度 `var(x,y)=a+b·x+c·y` 场，拟合 `a,b,c` 在 10% 内复现（`SNR-006`）。
- **不变性门**：常量场、空 patch 拒绝、`floor` 夹逼、量纲 `ivar=1/var` 四门（`TST-NOISE-INV-*`）。
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
4. 默认值 `min_samples=64` 的导出依据（claim SC-002）：8×8 patch（P=64）下以本文件 §11 冻结的 5% oracle 为判据，`min_samples=5` 时单 patch 偏差 −19.2%、全局 `sigma_bg_global` 偏差 −25.1%、5% 门通过率 **0.6%**；`min_samples=64` 为 −1.25%/−1.7%、通过率 **92.8%**（纯高斯蒙特卡洛；常规无掩膜帧上 5 与 64 逐位同输出，差异只在 patch 残余样本 5~63 的掩膜 regime）。来源 `reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md` §2 EXP-1/2/3/9，复跑 `run/PROJECT-GOVERNANCE-01/R-5/exp1_mad_bias_mc.py`、`exp2_pipeline_mc.py`、`exp3_prod_threshold.py`。

## 15 Acceptance

- §11 Oracle 全过：Gaussian 5% 复现、Poisson 诊断 5% 交叉（仅诊断）、平面场 10% 恢复、四不变量门、Python 参考 rtol 1e-9；
- §8 全部退化路径显式（无合格 patch/NaN/floor/gain≤0）；
- `tools/science_contract_lint.py` PASS（15 节+claim ID+锚点）；
- 解析不变量→SYN-003 转换：Gaussian/Poisson/常量/blank sky/outlier/small-N 用例、estimator bias 与 ivar 边界（零/负/NaN→ivar=0）登记 SYN-003。
