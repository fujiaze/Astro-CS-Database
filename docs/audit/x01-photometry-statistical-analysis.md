# X01 相对测光的统计验收命题：从 5% 单样本阈值改为归一化残差

> 状态：**科学验收命题修订记录**，不是生产算法认证。旧的 5% 失败原样保留；新命题有独立协方差推导与至少一次实跑数值。
> 受检运行：`fa4649d57`，`ACSD_DRIZZLE_BLOCK=16`（normalize 8/8、mosaic 1、export 1）；blind solve RMS 0.2109 px。
> 独立审查：`docs/audit/audit-normalize.md`（P0-1 与 X01 专项），以及 `validation/x01/uncertainty_x01.py`（不调用生产 helper）。

## 1. 原命题及其问题

`compare_x01.py` 原判据是

```
M_i = (F_i / F_peak) / (T_i / T_peak)
历史判据：max_i |M_i - 1| <= 0.05
```

其中 `F_i` 是 `sources.measure` 的冻结位置联合拟合通量，`F_peak` 是亮源（带 656.3 nm 线）通量，`T` 是冻结真值。

问题不是“5% 太严”，而是**该判据把单次实现的随机波动当成系统误差**：`M_i` 本身有方差。`F` 是线性联合拟合 `d = Σ_s F_s H_s + β + ε` 的解，其协方差为

```
C = (Dᵀ W D)⁻¹,   W = diag(1/Var_pix),
Var_pix = (sky + dark + Σ_s F_s H_s)/gain² + (read/gain)²
```

于是

```
σ²(M_i) = C_ii/F_i² + C_pp/F_p² − 2 C_ip/(F_i F_p).
```

对 X01 的 B 组最暗源（`x01-06`, source 1003，weight 0.70），代入冻结真值的 sky/dark/gain/read、PSF σ=1.35、测量位置与 7 源联合设计：

```
σ(M_i) ≈ 0.0585  (5.85%)   [OLS sandwich]
观测 M_i − 1 = 10.33%  →  z ≈ 1.77–1.90（取决于位置/PSF 的参数化）
```

即：**5% 阈值低于该估计量自身的 1σ**。在 8 帧 × 6 个非峰源 = 48 个相关残差上取最大值，即使估计量完全无偏，最大 |z| 的典型值也约 2.5–2.7，对应约 15% 的最大相对误差。因此“单次实现最大相对误差 ≤5%”在 B 组的 SNR 下不可达；它不是对算法的合理否证。

## 2. 独立协方差与残差（实际数值）

`validation/x01/uncertainty_x01.py` 的输入全部来自冻结真值（PSF 像元积分、源位置、sky/dark/gain/read）与 trace 中实际使用的测量位置；它不复用 `sources.measure`、`geometry` 或 `numerics`。对每帧构造同一估计量的 `D`、`C`，并计算：

| 量 | 数值 |
|---|---|
| 非峰源/帧残差数 | 48（7 源 × 8 帧，去掉每帧峰源） |
| 最大 \|z\| | **1.90** |
| 平均 z | +0.156 |
| RMS z | 0.907 |
| 4σ 内 | 48/48 |
| 最大相对误差（同旧指标） | 0.1033（仍 FAIL 5%） |
| B 组最暗源预测 σ(M) | 5.85% |

绝对通量（不做峰值归一）的独立检查同样给 56/56 在 3σ 内，最大 |z| 1.94；但 X01 的实际判据是比值，故以 `σ(M)` 为准。

## 3. 排除系统偏差的独立核对

`audit-normalize.md` 在会话内独立复算（非生产 helper）：

- 独立 OLS 复现 `measure` 逐源差 ≤1.5%；
- 四帧联合自由位置 + 自由 PSF 宽度：B 组 σ=1.33948±0.00022、A 组 σ=1.35489±0.00012，统一偏差 −0.78%/+0.36%，不是 10%；
- B 组 WCS 位置残差 0.162–0.257 px；x01-06 的 1003 偏移仅 (−0.018,−0.012) px，δ=0.25 px 的模板错位只给 −0.85%；
- 背景 β 与 `(sky+dark)·t/gain` 差 <0.05%；
- 响应身份 B/A=0.596 vs 0.600；
- 四帧联合 χ²≈6.4/7 dof。

因此 10.3% 是单次噪声实现，不是 `measure` 的系统偏差。注意：这不等于 `measure` 已验证正确——它的通量区间 P0（列缩放缺失）仍由 `audit-normalize.md` P0-1 记录并修复中；本页只裁决 X01 的**验收命题**。

## 4. 新命题

对 X01 的冻结输入与估计量：

1. **Gating**：48 个非峰源/帧的比值残差全部满足 `|z_i| ≤ 4`，其中 `z_i = (M_i−1)/σ(M_i)`，`σ(M_i)` 由上述联合协方差（含源–峰协方差）给出。48 个独立检验在 4σ 的双侧 FWER≈0.003；残差相关只会使有效检验数更小，故 4σ 是保守的。同时报告 `mean z` 与 `rms z` 作为无偏性/协方差校准诊断（不单独 gating）。
2. **历史判据**：`max_i |M_i−1| ≤ 5%` 保留在 `comparison.json`，状态 `superseded`，观测值 0.1033 不隐藏。它不再作为 gating，因为已证明低于估计量 1σ。
3. **独立真值**：`σ(M)` 的计算不得调用生产 `measure`/`geometry`/`numerics`；当前由 `uncertainty_x01.py` 从真值模型独立构造。
4. **适用域**：该结论条件于冻结的 PSF 族（像元积分 Gaussian）、已知的 sky/dark/gain/read、以及测量实际使用的位置。真实仪器 PSF 失配、未知 gain、DCR 不在本命题内。

## 5. 复现与当前状态

```bash
python3 validation/x01/truth_x01.py
ACSD_DRIZZLE_BLOCK=1 python3 validation/x01/e2e_x01.py   # 当前默认，block>1 已被拒绝
python3 validation/x01/uncertainty_x01.py validation/x01/observed/normalize_trace.jsonl
python3 validation/x01/compare_x01.py validation/x01/observed/normalize_trace.jsonl
```

**当前未完成**：`comparison.json` 记录的数值来自 `fa4649d57` 且使用 `block=16`。几何审计 P0-6 证明 block>1 改变了 drizzle 的估计量，工作流已改为拒绝 block≠1；因此该记录标为 `pending_at_current_head`，必须在最终 HEAD 用默认 block=1 重跑后才能作为当前 PASS。旧 10.3% 失败在任何情况下都保留在 Git 历史与本文件中。

## 6. 遗留的 X01 夹具问题

`audit-normalize.md` 发现 X01 的 `calibration.json` 写 `dark=4.8 ADU`，而 truth 注入 `12 e⁻/s × 100 s / gain = 480 ADU`；差值被联合拟合的 β 吸收，所以该案**从未真正检验 dark 单位/曝光比**。这是夹具缺陷，不是生产实现通过。修复需要同时改 truth 与 calibration 并重跑，且会改变旧证据；在修复前不得宣称“dark 路径已由 X01 覆盖”。

## 7. 资源预算的夹具更正（不是科学容差）

X01 的 `normalize.config.json` 原先写 `work=10,000,000`。该值在过去从未被工作流执行（workflow 硬编码 4e9），所以旧“三命令通过”并不表示它足够。C1 审查把用户 `Limits` 接入 Runtime 后，第一帧就消耗了几乎全部 10M，第二帧的 1e6 plan 无法准入，报 `TaskSpec.work_upper exceeds the declared max_work budget`。

实际测量（本分支，默认 block=1，同一二进制）：

- 单帧 A 组 normalize 在 10M 下成功；
- 两帧 A 组在 20M 下成功、耗时约 8 分 9 秒；
- 因此当前逐像素几何约 **10M work/帧**，8 帧需要约 80M。

据此把 `normalize.config.json` 的 `work` 改为 **100,000,000**（10× 单帧的保守上界）。这是**资源输入**的更正，不是科学容差/真值的放宽：它不改变估计量、PSF、背景、WCS 或任何数值判据；它只让当前算法在声明的预算内完成。逐像素几何 ~10M work/帧本身是 C2/W6 的性能问题（runtime 审查测得每次 invoke 约 6.12 ms 固定 setup），应由 cohort 化降低，而不是靠预算掩盖。`mosaic.config.json`/`export.config.json` 暂保持 10M；若实跑超预算，必须同样测量并记录，不能静默提高。
