# PSF Science (SCI-PSF)

> ID: SCI-PSF-001  状态: FROZEN (T101 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-STAR-PSF-001..  模块: dynamic_psf

## 1 目的与非目标

- **目的**：描述点源响应（椭圆 Moffat4），估计 PSF 形状/位置及拟合质量代理 `q_psf`，用于 Astrometry/Photometry 的星点建模与剔星/QA。
- **非目标**：不处理超出椭圆 Moffat4 的高阶色差/空间变异（仅一阶椭率 `e,θ`）；不直接输出图像噪声 SNR（`q_psf`≠SNR，见 NOISE_MODEL）；不进入 Phase2 逐像素 science weight（SNR-008 已退休旧 `(A−B)/mad` 路径）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `I(r)` | 点源强度模型 `B + A/(1+Q)^4` | `dpsf_psf.cpp:13-18` |
| `Q` | 二次型 `p1·dx²+2p2·dxdy+p3·dy²` | `dpsf_psf.cpp:66-95` |
| `dx,dy` | 相对坐标 `x−(cx+x0), y−(cy+y0)` | 同上 |
| `B,A,x0,y0,sx,sy,θ` | Moffat4 7 参数 | `lm_solve` 7-vector |
| `σ,sx,sy` | 各向同性 σ / 各向异性轴尺度 (px) | FWHM 推导 |
| `e` | 离心率 `√(1−(s_min/s_max)²)` | 椭率 |
| `fwhm_x/y` | 轴向 FWHM `1.230310·s` | `MOFFAT4_FWHM_FACTOR` |
| `flux` | 解析通量 `2πA·sxsy/3` (β=4) | `dpsf_psf.cpp:368` |
| `residual_scale` | 10–90% trimmed mean \|residual\| | PSF 块第8列 |
| `robust_residual_sigma` | `residual_scale/0.7316728` | Gaussian 假设 |
| `q_psf` | `A/residual_scale` 拟合质量代理 | 剔星/QA |

## 3 物理量和单位

- `I,A,B,residual_scale`: ADU；`r,dx,dy,σ,sx,sy,fwhm`: px；`θ`: rad；`Q,e,q_psf`: 无量纲；`flux`: ADU·px²（含解析积分常数）。

## 4 输入有效域

- 图像 `uint16_t`/`float32`，维度 `w>0,h>0`，拟合窗口 `fitRadius` 使 `rect` 在图像内且面积 `rw*rh > 0`，否则返回 `DPSF_ERR_PARAM`（`dpsf_fit:433 empty rect`）。
- 初始幅度 `A0 = max_val − bkg0 > 0`，否则 `LOG_WARN Amplitude<=0` 并拒。
- `sx>0, sy>0`，否则 `Invalid fit params` 拒；`B` 受 `bkg0` 约束（`Background constraint violated`）。

## 5 连续定义

```text
I(r) = B + A / (1 + Q)^4
Q = p1·dx² + 2·p2·dx·dy + p3·dy²
p1 = cos²θ/(2sx²)+sin²θ/(2sy²)
p2 = sin2θ/(4sx²)−sin2θ/(4sy²)
p3 = sin²θ/(2sx²)+cos²θ/(2sy²)
dx = x−(cx+x0), dy = y−(cy+y0)

各向同性 sx=sy=σ ⇒ Q=0.5·r²/σ²
α=√2·σ, FWHM=2α√(2^{1/4}−1)=2√2·σ·√(2^{1/4}−1)≈1.230310·σ
flux = 2πA·sxsy/3   (整平面延伸假设)
```

与 `lib/dynamic_psf/src/dpsf_psf.cpp:13-18,66-95,351-368` 一致。

## 6 假设

- 视场内 PSF 缓变（块状拟合共享假设）；星点不饱和、采样充足（FWHM/px 合理范围）；残差在 `robust_residual_sigma` 换算时近似 Gaussian。

## 7 独立不变量

- **FWHM 缩放不变量**：各向同性 Moffat4 的 `FWHM/σ` 比值恒为 `1.230310`，与 `A,B` 无关。
- **积分一致性**：各向同性 `σ` 的解析 `flux` 在数值积分（足域）内与 `A,σ` 的 `2πAσ²/3` 比例一致（误差仅离散域/截断）。
- **旋转简并不变量**：`θ` 四候选 `{θ,π/2−θ,π/2+θ,π−θ}` 中以 trimmed-mad 最小者消歧后，`fwhm_x/y` 与方向无关（`dpsf_psf.cpp:351-363`）。
- **平移不变量**：整帧平移 `Δ` 后拟合中心 `cx+x0` 同步平移 `Δ`（子像素插值误差内）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 空 `rect`/越界 | 返回错误 `DPSF_ERR_PARAM` | `dpsf_fit:446 empty rect` |
| `A<=0` / `max<=bkg` | `WARN Amplitude<=0` 拒 | `dpsf_psf.cpp:310` |
| `sx<=0`/`sy<=0`/非有限 | `WARN Invalid fit params` 拒 | `dpsf_psf.cpp:333` |
| FWHM 超窗 | `WARN FWHM exceeds rect` | `dpsf_psf.cpp:343` |
| LM 不收敛 | 返回非零 `status`，成本 `cost` 上报 | `dpsf_psf.cpp:186` |
| 无星/密集混淆 | 上游采样为空 → 显式 NO_DATA | 调用方 |

## 9 精度策略

- FP64 拟合 LM 求解器 `lm_solve`（`dpsf_psf.cpp:98-182`），仅 7 参数 Moffat4 路径；`kTrimMeanToSigma=0.7316727929211932` 解析常数（`noise_model.cpp:35-37`）用于 `robust_residual_sigma`，仅 Gaussian 假设下有尺度意义。

## 10 不可接受变化

- 改变 Moffat β≠4 或 `FWHM_FACTOR` 而无 SCI 变更；
- 将 `q_psf` 当 SNR 进入 Phase2 权重；
- 引入未文档的高斯备选拟合路径作为主路径。

## 11 验证 Oracle

- **解析解**：各向同性 `FWHM/σ` 与 `flux` 公式的解析一致性（`max_abs==0`）。
- **Python 参考**：`scipy` / NumPy 对同参数 Moffat4 图像块做 `curve_fit` 复算，位置 `≤0.05px`、FWHM `≤1%`（合成无噪声谱）。
- **不变量门**：FWHM 缩放、旋转简并、平移三门。
- **失败注入**：空窗/非正幅度/非有限尺度返回显式错误码。

## 12 关联 ALG ID

- `ALG-STAR-PSF-001` Moffat4 拟合（LM 7 参数）
- `ALG-STAR-PSF-002` 几何常数（FWHM/通量）与 θ 消歧

## 13 追溯与测试

- 权威文件: `docs/science/PSF.md` (SCI-PSF-001)
- 实现: `lib/dynamic_psf/src/dpsf_psf.cpp` (`dpsf_fit/batch, lm_solve, MOFFAT4_FWHM_FACTOR, compute_trimmed_mad`), `lib/snr_estimator/cpp/src/noise_model.cpp:35-37`
- 公开 API: `lib/dynamic_psf/include/dynamic_psf.h` (`dpsf_fit, dpsf_fit_batch`)
- 测试: `TST-PSF-001` 解析一致性、`TST-PSF-INV-*` 三门、`TST-PSF-FAIL-*` 参数校验（新增/映射见 `docs/TRACEABILITY.csv`）
