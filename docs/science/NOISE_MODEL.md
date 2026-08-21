# Noise Model Science（NoiseWeightModelV1）

## 目的

估计**校准后空背景随机分量**的逐像素方差（→ ivar），用于 Phase2 科学权重。

## 科学定义

目标量：`variance of calibrated blank-sky random component`。
≠ PSF 星亮度、≠ photometric calibration scatter、≠ q_psf（SNR-003/008/010）。

## 算法

```text
patch grid 8×8；星点掩膜（fixed conservative，统一半径
rmax = max(1, r0) × max(1, scale)，不按星亮度缩放；已冻结）
σ_bg = 1.4826022185 × median(|x − median(x)|)     # MAD→σ
cosmic/hot 5σ 稳健裁剪 ≤2 轮
控制点 = 合格 patch（≥ min_samples）
空间场 = 最小二乘平面 var(x,y) = a + b·x + c·y（负预测 clamp 1e-12）
全局兜底 = 合格 patch variance 稳健中位数
ivar = 1/variance
```

- 平面场仅在 `enable_spatial_field=1 && n_control_points>=4` 时启用；否则退化为全局常量场。
- Floor 传播：`variance_floor` 默认 1e-12，经 `g_model_floor` 以 model 指针为 key 注册，随 `fill` 阶段对平面预测 `max(a+b·x+c·y, floor)` clamp；control 点自身亦 `max(patch_var, floor)`。`g_model_floor` 在 `snr_noise_model_v1_free` 时按指针擦除，无全局共享。

## Gain/Readnoise 诊断模型（仅诊断 / diagnostic，NOT FOR PRODUCTION）

- 函数 `snr_noise_gain_variance(signal, gain_e_per_adu, read_noise_e)` 实现 `max(signal,0)/gain + (rn/g)²`（ADU 方差）；`gain<=0` 返回 0。
- 用途：**仅作 SNR-005 诊断交叉验证**（Poisson+readnoise 合成帧与经验 blank-sky 的一致性校验，`noise_model_science_test.cpp:238-272`），不参与生产权重。
- 生产基线：`NoiseWeightModelV1.source==0 empirical`（blank-sky 稳健估计）；即使 header 提供 gain/readnoise 亦**不融合**至 `variance/ivar` 生产场；`gain/readnoise` 字段仅保留作诊断/追溯，防误用（NO-01 P0）。
- 公式物理域：`signal` 为 ADU 均值近似 `μ`，`gain` 单位 e-/ADU，`read_noise_e` 单位 e-；`SNR-005` 诊断中 `var_th = μ/gain + rn²/gain²` 与经验 `variance_bg_global` 在 5% 内一致即通过。

## 变量/单位

- x：像素 ADU/e⁻；variance/ivar：信号² / 信号⁻²。
- gain_e_per_adu: e-/ADU；read_noise_e: e-；signal: ADU。

## 假设

- 空背景局部平稳；源星点可掩膜；掩膜半径与星亮度解耦（fixed
  conservative 语义；API 仅接收 `star_x/star_y` 无 amplitude，统一 `rmax`；**NOT GUARANTEED**：若未来需要 PSF-aware adaptive mask，须先扩展 API 输入振幅并重新冻结/测试，当前固定半径语义不保证向 adaptive 演进，禁止文档与实现分叉）。

## 生产配置（NOISE-WIRE-001，已冻结）

生产调用必须先 `snr_noise_model_v1_default_config()` 再覆盖显式
元数据（gain/readnoise 等）；`cfg==nullptr`、`default_config()` 与
生产默认（default + gain/readnoise=0）三者逐字段 exact（含 spatial
field 默认开启、variance_floor=1e-12）。

## 有效域

- 有合格 patch（≥min_samples）；无大面积云/卫星轨迹时优先。
- `variance_floor>0` 时生效；`<=0` 时内部回退 1e-12（fill 阶段）。

## 不保证

- 不保证 Drizzle 后相邻像素独立（协方差见 UNCERTAINTY_AND_COVARIANCE）。
- 不保证 gain 诊断模型参与生产权重（见上节 diagnostic 声明）。

## 失效条件

- 无合格 patch → 全局兜底路径：若全帧 sky 样本仍 `<min_samples/2` 或 `robust_sigma` 非有限/≤0，则 `degenerate=1, ivar=0, r=1`（调用方应拒绝加权）；否则返回 `degenerate=1` 的全局常量场（`has_spatial_field=0`）作为可用的 fallback，`r=0`。
- 全帧 NaN/饱和（无有效像素）→ `degenerate=1, ivar_bg_global=0, r=1`。
- NaN 权重 → INVALID_INPUT。

## 系统/随机误差

- 系统：平面场残差低阶项（空间场吸收一部分）；随机：patch 采样误差。

## 数值精度

FP64；MAD 常数 1.4826022185（Gaussian，文档缩写；实现 `1.482602218505602` 双精度截断，差值 <1e-12，截断策略：保留 15 位小数后截断，非四舍五入到 10 位）；`robust_sigma = 1.482602218505602·median(|x−median|)` 与 PSF 侧 `trimmed-mean-abs (0.7316727929211932)` 系数并列时易混——前者为 MAD→σ，后者为 10-90% trimmed mean |residual| → σ（`E=0.731673σ`），不可互换。
裁剪 5σ；`variance_floor` clamp 保证 `ivar` 有限。

- PsfFitQuality 补充（NO-10）：`residual_scale` 为 10-90% trimmed mean |residual|，`robust_residual_sigma = residual_scale / 0.7316727929211932`（`kTrimMeanToSigma=0.7316727929211932`，对应 `E[trimmed mean |r|]=0.731673σ`），`q_psf = amplitude_above_bg / residual_scale` 仅为拟合质量代理，不进入 Phase2 逐像素 weight。

## 参考文献

SNR_REDESIGN_CONTRACT（工程控制）；Fruchter & Hook (2002) 权重语义。

## ID

SCI-NOISE-001..015（SNR-001..015）；ALG-NOISE-ESTIMATION-*（S2）。
