# Round 3 — Science 验证（V17）

日期：2026-08-14

## 1. integration correctness（C01-C05）

```text
V17NonFiniteWeightInvalid   : NaN/+Inf/-Inf/负权重 → INVALID_INPUT，绝不 OK+NaN
V17NonFiniteSupportInvalid  : NaN/Inf support → INVALID_INPUT；正有限正常
V17StatusesExplicit         : NO_CANDIDATES / ALL_REJECTED / ZERO_VALID_WEIGHT 可达
V17InvalidMethodStatus      : AUTO 进 kernel → P2_STATUS_INVALID_METHOD（hard fail 集合）
validate_candidate_weights  : 非 finite/非正 → 1
```

gate 74/74 PASS（含上述）。

## 2. 受控 clean rejection truth（zero-outlier 合成 20 帧）

```text
true sample FPR            = 1.88%（30000 samples；Siril 1.4.3 frozen harness
                             同源 case 8000 decisions 100% 一致）
pixel any-rejection FPR    = 26.3%（1500 背景像素）
star aperture flux bias    = -0.07%（8 星 aperture）
PSF FWHM bias              = +0.012%（矩估计）
faint structure bias       = -0.29%
background noise efficiency= 1.045（auto vs none）
thin satellite recall      = 1.0（1145 px）
compact cosmic recall      = 1.0（96 px）
hot streak recall          = 1.0（2884 px）
```

结论：LinearFit 的 1.88% FPR 是 frozen Siril reference 行为（同源 case
逐样本一致），不是 AstroCS 过拒；星点/PSF/结构保持良好。

## 3. 真实 16 帧（NGC1727 H-alpha，V17 重跑）

```text
trail recall               = 1.0（1907/1907）
observed sample rejection  = 0.54%（100/18448）
observed pixel any-reject  = 6.58%（79/1200）
clean vs truth bg bias     = 0.000 / p95 6.04e-05
clean vs truth bg std ratio= 0.9991
trail vs truth bg bias     = 0.000
underdetermined px         = 10766（depth≤2 真实边缘）
```

## 4. large_scale（astrocs.large_scale_rejection.v1）

```text
单元：trail grow ±2（Chebyshev）、端点扩张、cosmic(2×2)<min8 不生长、
      sparse 星点噪声不生长、low/high 独立、disabled no-op、非法参数拒绝
E2E ：satellite 变体 pre=88055 post=91134 grown=3079（线带扩张）
      cosmic 变体 grown=0（紧凑结构不扩张）
```

## 5. oracle / 对照

- Siril 1.4.3 frozen harness：同源 8000 decisions 100% 一致（V17 受控 truth）；
- 既有 oracle（Astropy sigma / NIST ESD / RCR 2.4.7）保持 PASS（gate 内含）；
- PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED 不变。

## 科学结论

```text
REJECTION_QUALITY = PASS（受控 truth + 真实 observed + 注入 recall）
INTEGRATION_CONTRACT = PASS（非 finite 安全 + status 显式 + support 单路径）
LARGE_SCALE = PASS（实现 + 单元 + E2E）
```

```text
ROUND3=PASS
```
