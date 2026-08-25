# Photometry Science (SCI-PHOT)

> ID: SCI-PHOT-001  状态: FROZEN (T103 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-PHOT-001..  模块: photometric_calib (flux_calibrator)

## 1 目的与非目标

- **目的**：将仪器流量 `F_instr` 校准到以 Gaia 合成通量 `F_syn` 为参考的相对/绝对光度尺度，估计零点 `location`、尺度因子 `scale` 及残差 QA `sigma_residual / sigma_mag`。
- **非目标**：不处理带通外颜色项高阶效应（仅 QA 暴露残差分布）；不估计逐像素噪声方差（SCI-NOISE 边界）；不做大气消光时变建模。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `F_instr` | 仪器通量 (ADU·px 或 e⁻) | 输入 |
| `F_syn` | 合成通量（Gaia 星表模型） | 输入 |
| `r_i` | `log10(F_instr/F_syn)` dex | 定标核心 |
| `delta_i` | `−2.5·log10(F_instr)−G_Gaia` mag | 星等一致性 |
| `location` | IRLS/Tukey 稳健位置（dex） | `star_matcher.cpp:478-525` |
| `scale` | `10^{−location}` 校正因子 `I_cal=I·scale` | 输出 |
| `S` | `MAD(r)/0.6745` 初值尺度 (dex) | `star_matcher.cpp:21-27` |
| `c` | Tukey 形状参数 `4.685` | 同上 |
| `sigma_residual` | `MAD(r_inliers)/0.6745` dex | QA |
| `sigma_mag` | `2.5·sigma_residual` mag | QA |
| `mag_tolerance` | 星等一致性阈 `3.0 mag` | `star_matcher.cpp:241` |
| `psf_status,qf` | 饱和/质量标志 | `snr_estimator` |

## 3 物理量和单位

- `F_instr, F_syn`: ADU·px 或 e⁻（同尺度）；`r, location, S, sigma_residual`: dex (`log10` 比值)；`delta, sigma_mag`: mag；`scale, sigma_cal_rel`: 无量纲/相对误差（`sigma_cal_rel = ln10·sigma_residual`）；`qf` 无量纲标志。

## 4 输入有效域

- 每颗星 `F_instr>0, F_syn>0` 有限值；饱和星（`psf_status!=0` 或 `qf & (SATURATED|HAS_SATURATED)!=0`）不进入匹配与定标，计 `rejected_quality`（`star_matcher.cpp:35-40`）。
- 参考星数 `|r_consistent|>=3` 才进 IRLS，否则 `NO_DATA`；`|r_inliers|>=2` 才估计 `sigma_residual`，否则 `sigma_residual=0`（`552-559`）。
- `S>0` 时迭代 `max_iter=50, tol=1e-6`；`S==0` 跳过 IRLS 取 `median(r)`（`478-525`）。

## 5 连续定义

```text
r_i = log10(F_instr,i / F_syn,i)                         # dex

# 星等一致性预过滤 (进入 IRLS 前)
delta_i = −2.5·log10(F_instr,i) − G_Gaia,i
median_delta = median(delta)
预拒绝 i  若  |delta_i − median_delta| > mag_tolerance    # mag_tolerance=3.0

# IRLS + Tukey biweight (对 r_consistent)
S = MAD(r_consistent)/0.6745,  location_0 = median(r_consistent)
迭代直到 |loc_new−loc_old|<1e-6 或 50 步:
  u_i = (r_i − location)/(c·S),  c=4.685
  w_i = (1−u_i²)²   (|u|<1),  0 否则
  location = Σ w_i·r_i / Σ w_i
若 S==0 ⇒ location = median(r_consistent), robust_iterations=0

scale = 10^{−location}          # I_cal = I·scale
sigma_residual = MAD(r_inliers)/0.6745   # r_inliers={i|w_i>0}
sigma_mag = 2.5·sigma_residual
outlier_rate = 1 − |r_inliers|/|r_consistent|
```

与 `lib/photometric_calib/cpp/src/star_matcher.cpp:21-27,241-248,435-525,552-559` 及 `lib/photometric_calib/cpp/src/pc_api.cpp` 一致。

## 6 假设

- Gaia 合成星表在观测带通内提供可信参考；大气/仪器零点在观测尺度稳定；饱和判据可靠（`psf_status==0` 且无 `SATURATED` 标志）。

## 7 独立不变量

- **零点平移不变量**：`F_instr` 全体同乘因子 `k` 时 `location` 增 `log10 k`，`scale` 相应除 `k`，`sigma_residual` 不变。
- **尺度单调性**：`F_instr/F_syn` 比值越大 `location` 越大，非饱和样本集下单调。
- **鲁棒性**：注入 20% 离群 `r` 时 IRLS `location` 变化 `<0.1 dex`（Tukey 权重截断）。
- **S=0 退化**：全体 `r` 相等时 `S=0` ⇒ `location=median(r)`，不迭代。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无星/星数不足 | 返回 `NO_DATA` | `star_matcher.cpp:478` 前校验 |
| `S==0` (MAD=0) | 跳过 IRLS，`location=median(r)` | `star_matcher.cpp:552` |
| 饱和/质量异常 | 不参与定标，计 `rejected_quality` | `psf_status!=0 \|\| qf&SATURATED` |
| 预过滤全拒 | `r_consistent` 空 ⇒ `NO_DATA` | `mag_tolerance=3.0` 判别 |
| 非有限 `F` | 显式拒绝，不进入 `r` | 参数校验 |

## 9 精度策略

- FP64 对数空间；flux 比值不经 `ivar` 加权（ivar 不用于此层）；IRLS 收敛阈 `1e-6` dex，迭代上限 50。

## 10 不可接受变化

- 改变 `c=4.685`/`tol=1e-6`/`max_iter=50`/`mag_tolerance=3.0` 阈值而无 SCI 变更；
- 将 `q_psf` 或 `sigma_residual` 混为逐像素 ivar；
- 忽略饱和/质量标志使饱和星进入定标。

## 11 验证 Oracle

- **合成注入**：已知 `scale` 的 `F_instr=k·F_syn` 注入场，估计 `location≈log10 k`（`rtol 1e-4`）。
- **鲁棒门**：注入 20% 离群点，`location` 偏差 `<0.1 dex` 且离群权重为 0。
- **S=0 门**：常数 `r` 场直接取 median 通路，不迭代。
- **Python 参考**：NumPy `median/MAD/Tukey` 对同 `r` 复算 `location/scale`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-PHOT-001` IRLS-Tukey 零点估计
- `ALG-PHOT-002` 星等一致性匹配与 QA (`sigma_residual/sigma_mag/outlier_rate`)

## 13 追溯与测试

- 权威文件: `docs/science/PHOTOMETRY.md` (SCI-PHOT-001)
- 实现: `lib/photometric_calib/cpp/src/star_matcher.cpp` (`location/S/scale, mag_tolerance, IRLS`), `lib/photometric_calib/cpp/src/pc_api.cpp` (`pc_calibrate_simple`)
- 公开 API: `lib/photometric_calib/cpp/include/photometric_calib.h` (`pc_calibrate_simple, pc_calibrate_simple_with_gaia`)
- 测试: `TST-PHOT-001` 合成零点、`TST-PHOT-INV-001` 鲁棒性、`TST-PHOT-FAIL-001` 饱和拒（新增/映射见 `docs/TRACEABILITY.csv`）
