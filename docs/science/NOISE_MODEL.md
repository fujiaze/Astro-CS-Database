# Noise / Variance / Ivar / SNR Science (SCI-NOISE)

> ID: SCI-NOISE-001..015 (SNR-001..015)  状态: FROZEN (T104 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-NOISE-001..  模块: snr_estimator (NoiseWeightModelV1)

## 1 目的与非目标

- **目的**：估计校准后空背景随机分量的逐像素方差 `variance` 及其倒数 `ivar=1/variance`，作为 Phase2 逐像素科学权重入 `var(x,y)=a+b·x+c·y` 空间场 + 全局兜底，用于 UPM 控制光度拟合与加权积分。
- **非目标**：不估计测光零点残差散度（SCI-PHOT `sigma_residual` QA）；不输出 PSF 拟合质量 `q_psf`（SCI-PSF）；不生产完整协方差矩阵（Drizzle 后相邻像素相关见 `UNCERTAINTY_AND_COVARIANCE.md`）；生产权重不融合 gain/readnoise 诊断模型（见 §5/§10）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x` | 像素值（校准后 ADU/e⁻ 空背景） | 输入 |
| `variance` | 随机分量方差 `σ_bg²` 或平面预测 `a+b·x+c·y` | `NoiseWeightModelV1.var` |
| `ivar` | `1/variance` (pixel⁻²·ADU⁻²) | `variance_bg_global` 倒数 / `fill` |
| `σ_bg` | `1.4826022185·MAD(|x−median|)` | `noise_model.cpp:robust_sigma` |
| `MAD` | `median(|x−median(x)|)` | 同上 |
| `rmax` | 掩膜半径 `max(1,r0)·max(1,scale)`（fixed conservative） | 掩膜 |
| `a,b,c` | 最小二乘平面 `var(x,y)=a+b·x+c·y` | `snr_noise_model_v1` |
| `variance_floor` | 方差下界 `1e-12`（`max(var,floor)` clamp） | `default_config` |
| `g_model_floor` | 以 `model*` 为 key 的 floor 注册表 | `noise_model.cpp:32,126,433` |
| `gain, read_noise_e` | 诊断模型参数 e-/ADU, e- | `snr_noise_gain_variance` |
| `r_inliers` | Tukey 权重>0 的内点集（SCI-PHOT 复用符号，不混） | QA |
| `degenerate, has_spatial_field` | 退化/空间场标志 | `NoiseWeightModelV1` |

## 3 物理量和单位

- `x, σ_bg, √variance`: ADU（或 e⁻，同输入标度）；`variance`: ADU²；`ivar`: ADU⁻²；`a`: ADU², `b,c`: ADU²/pixel；`gain`: e-/ADU；`read_noise_e`: e-；`signal`: ADU；掩膜半径/坐标: pixel；`floor`: ADU²。

## 4 输入有效域

- 维度 `h>0,w>0`，`data` 非空且含有限值；`min_samples`（patch 样本数阈）默认 5；`rmax` 为固定值，不按星亮度/振幅缩放（API 仅 `star_x/y` 无 amplitude，见 §6）。
- 平面场仅 `enable_spatial_field==1 && n_control_points>=4` 启用，否则退化为全局常量场（`has_spatial_field=0`）。
- `variance_floor>0` 时 `max(var,floor)` clamp 生效；`<=0` 时 `fill` 内部回退 `1e-12`（`fill` 阶段）。
- `gain<=0` 时 `snr_noise_gain_variance` 返回 0（诊断路径，不入生产）。

## 5 连续定义

```text
patch grid 8×8；星点掩膜 fixed conservative rmax = max(1,r0)·max(1,scale)（统一半径，不按亮度缩放）
σ_bg = 1.482602218505602 · median(|x − median(x)|)   # MAD→σ，Gaussian 假设
稳健裁剪: cosmic/hot 5σ 阈，≤2 轮
控制点: 合格 patch（样本数 ≥ min_samples）的 patch variance
空间场: 最小二乘平面 var(x,y) = a + b·x + c·y；负预测 clamp 至 variance_floor (1e-12)
全局兜底: 合格 patch variance 的稳健中位数 vmed
variance_bg_global = max(vmed_or_sig², variance_floor)
g_model_floor: 以 model 指针为 key 注册 floor，snr_noise_model_v1_free 时按指针擦除，无全局共享
ivar = 1 / max(variance, floor)   # fill 阶段 max(a+b·x+c·y, floor)；control 点亦 max(patch_var, floor)
```

```text
Gain/Readnoise 诊断模型 (仅 diagnostic, NOT FOR PRODUCTION):
  var_ADU = max(signal,0)/gain + (read_noise_e / gain)²   # signal: ADU, gain: e-/ADU, rn: e-
  用途仅 SNR-005 诊断交叉验证 (noise_model_science_test.cpp:238-272)，生产 source==0 empirical 不融合
```

与 `lib/snr_estimator/cpp/src/noise_model.cpp:32-126,210-256,333-433,456-464` 一致。

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
| 无合格 patch | `degenerate=1`, 若 sky 样本`<min_samples/2`或`robust_sigma`非有限/≤0 ⇒ `ivar=0,r=1` 拒；否则 `degenerate=1` 全局常量场 `has_spatial_field=0, r=0` fallback | `noise_model.cpp:210-235` |
| 全帧 NaN/饱和 | `degenerate=1, ivar_bg_global=0, r=1` | 同上 |
| `variance_floor<=0` | `fill` 回退 `1e-12` clamp | `noise_model.cpp:403-404` |
| `gain<=0` | `snr_noise_gain_variance` 返回 0 | `noise_model.cpp:456` |
| `star_x/y` 非有限 | 掩膜跳过该星，不污染统计 | 参数校验 |
| `MAD=0` | `σ_bg=0` ⇒ 退化路径（见上） | `robust_sigma` |

## 9 精度策略

- FP64 全链路；MAD 常数 `1.482602218505602`（15 位截断，与 `1.4826022185` 差 `<1e-12`）；`q_psf` 的 `0.7316727929211932`（10–90% trimmed mean \|residual\| →σ）与本节 `1.4826` 不可互换（前者 trimmed mean，后者 MAD）。
- `variance_floor=1e-12` 保证 `ivar` 有限；平面预测负值 clamp 至 floor。
- 5σ 裁剪 ≤2 轮，避免过度剔除。

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
- 实现: `lib/snr_estimator/cpp/src/noise_model.cpp` (`snr_noise_model_v1, _f64, _fill, _free, snr_noise_gain_variance, g_model_floor`), `lib/snr_estimator/cpp/include/snr_estimator.h`
- 公开 API: `snr_noise_model_v1, snr_noise_model_v1_f64, snr_noise_model_v1_fill, snr_noise_model_v1_free, snr_noise_gain_variance, snr_noise_model_v1_default_config`
- 测试: `TST-NOISE-001..015` (`noise_model_science_test.cpp`), `TST-NOISE-INV-*` 四门、不变量，`TST-NOISE-FAIL-*` 空 patch/NaN 拒绝（新增/映射见 `docs/TRACEABILITY.csv`）
