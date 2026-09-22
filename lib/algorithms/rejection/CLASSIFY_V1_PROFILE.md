# astrocs.rejection.classify.v1 — V6 分类排异 profile（IMPL-P2-REJ-001）

> 状态：v1 **版本化算法 profile**（非 SCI/ALG 冻结阈值）。冻结合同条目：
> `ALG-P2S-REJ.1..7`、`FZ-REJ-INHERITED-THRESH`（继承阈值）、
> `FZ-AP2S-REJ-CALIB-BINMIN/ABS/BSS-MIN`（校准门，PENDING_OWNER_SIGNOFF
> SO-07，fail-closed 按文档值实现）。实现锚：`lib/algorithms/coverage/src/rejection.cpp`
> （`p2_reject_classify` / `p2_reject_calibration`）；声明锚：
> `lib/algorithms/coverage/include/astro/phase2/rejection.h`。

## 1 判据：预测残差方差（ALG-P2S-REJ.3）

```text
sigma_eff^2 = sigma_phase1^2 + J C_theta J^T      [ADU^2/sr^2]
z           = residual / sigma_eff                [1]
```

- `residual = d - model` [ADU/sr]；`sigma_phase1` [ADU/sr]；
  `J C_theta J^T` 为 UPM 参数不确定度标量对角 [ADU^2/sr^2]；
  `C_theta=(J^T W J)^-1`（ALG-P2-SURF-UPM §4）。
- **fail-closed**：`noise_flags` 未同时含 `P2_NOISE_PHASE1_DECLARED` 与
  `P2_NOISE_UPM_DECLARED` → `INVALID_INPUT`（禁止用裸残差作阈值）；
  非 finite 或 `sigma_eff^2<=0` → `INVALID_INPUT`。

## 2 reason（4 继承，ALG-REJ-001）

| 条件 | reason |
|---|---|
| `z <= -4.0` | `rejected_low` |
| `z >= +3.0` | `rejected_high` |
| 否则 | `accepted` |
| `n <= underdetermined_n(=2)` | `underdetermined`（全接受，recall=0 显式） |

阈值 `sigma.lower_sigma=4.0 / sigma.upper_sigma=3.0 / max_iterations=8`、
`linear_fit 5.0/3.5/8`、`percentile 0.2/0.1`、`ESD alpha 0.05/max 10`、
`large_scale 8/2/2`、`minmax 1/1/4`、`rcr technique=0` 全部**原样继承**
（`p2_reject_plan_thresholds_inherited` 逐项校验；任一改动 →
`INVALID_CONFIGURATION`，FZ-REJ-INHERITED-THRESH）。

## 3 reason_class（6 污染类，ADJ-GEN-02 正交字段）

| class id | 证据门（`P2RejectClassifyInput` 字段） |
|---|---|
| `cosmic_ray` | `compact_single_frame && !large_scale_growth` |
| `satellite_trail` | `large_scale_growth` |
| `bad_column` | `column_consistent` |
| `moving_source` | `cross_frame_motion >= motion_min_px(0.5)` |
| `cloud_gradient` | `low_frequency` |
| `defocus_trail` | `psf_shape_anomaly >= psf_anomaly_min(0.2)` |

- 仅对 `z` 越阈样本或移动源证据样本做类归属；并列取**最小类 id**
  （固定序确定性，M7 mutation 可检）。
- `moving_source` 是科学信号：默认 `preserved=1`、`deleted=0`、`reason=accepted`
  （可保留到独立层，`moving_source_layer` provenance）；`keep_moving_source=0`
  时按阈值照删。
- `reason`（方向）与 `reason_class`（机制）落两个字段，不得用同一整数承载两义。

## 4 probability（仅门/推断，禁进权重面）

高斯混合后验（log 空间，数值稳定）：

```text
H0: r ~ N(0, sigma_eff^2);   H1: r ~ N(0, kappa^2 sigma_eff^2)
p = prior*pdf1 / (prior*pdf1 + (1-prior)*pdf0)
  = 1 / (1 + exp( (log(1-prior) - z^2/2)
                - (log(prior) + (-z^2/(2 kappa^2) - log kappa)) ))
```

v1 常量：`contamination_prior=0.05`、`outlier_inflation kappa=4.0`
（版本化 profile 常量，非冻结排异阈值；改动 → `INVALID_CONFIGURATION`）。
`probability` 与 `class_probability[c]=p*w_c/sum(w)` 只作门/推断输出，
**禁止**进入 `weight.sources`/`weight_value`/`variance_from`
（`p2_rejection_weight_surface_guard` 强制：命中
`rejection/probability/support/coverage/median*snr/fwhm/residual/psfsw/
psf_snr_power/auto/support_x_snr2/0` → REJECT）。

## 5 小样本失败（ALG-P2S-REJ.3）

`n <= 2`（`plan.underdetermined_n` 默认 2）→ 全接受 `underdetermined`、
`recall=0.0` **显式**、`probability=0`、`class=none`；不做伪剔除。
栈级 `status` 沿用继承 8 态：`OK / MIN_SAMPLES / ALL_REJECTED /
INVALID_INPUT / UNDERDETERMINED / INVALID_CONFIGURATION / INVALID_METHOD /
INTERNAL_ERROR`。

## 6 校准门（ALG-P2S-REJ.4；PENDING_OWNER_SIGNOFF SO-07）

在预注册污染注入集上（训练/验收样本须不同）：

```text
可靠性: 按 p 分位分箱（每箱 >= BINMIN = 50）；max|obs - mean(p)| <= ABS = 0.10
技巧分: BSS = 1 - BS/BS_ref > BSS_MIN = 0.10
```

- 分箱样本 `<50` → 该箱不参与判定，但 `bins_skipped_small` 覆盖必须登记。
- 任一门失败（或 `BS_ref==0` 退化）→ `probability_is_scientific_gate=0`，
  必须回退显式阈值判定并登记校准失败原因；禁止声称概率校准。
- 数值 `50/0.10/0.10` 为 PENDING_OWNER_SIGNOFF SO-07：fail-closed 按文档值
  实现，未签字生效前不得放宽（M2/M3 mutation 可检）。

## 7 provenance（FZ-PROV-MINIMAL-SET；ALG-P2S-REJ.6）

每样本：`reason` / `reason_class` / `probability` / `class_probability` /
`sigma_eff`（含 `sigma_phase1` 与 UPM 项组成）/ `z`；
栈级：`status` / counts / `accepted_count` / `recall`。
profile 版本字符串：`astrocs.rejection.classify.v1`。

## 8 证据（本任务）

- 独立 Oracle（纯标准库，不调用 astrocs）：`eng/tests/unit/v6_p2_rej/oracle_rej.py`
  生成 `oracle_expected.inc`（3 固定用例 + 6 校准用例期望值）。
- 单元测试：`eng/tests/unit/v6_p2_rej/p2_rej_v6_test.cpp`（units/oracle/reference/
  negative/calibration/determinism 六组，ctest 6/6）。
- 负向 mutation：12 条违反冻结的实现改动全部被测试检出（`run/v6/IMPL-P2-REJ-001/mutations.json`）。

