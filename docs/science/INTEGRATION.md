# Integration / Coaddition Science (SCI-INT)

> ID: SCI-INT-001,002,004,008  状态: FROZEN (T108 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-INT-001..  模块: phase2 (integrate)

## 1 目的与非目标

- **目的**：将每像素的 `UPM-calibrated + accepted` 候选栈加权合并为统一信号 `signal` 与支撑度 `support`，输出 `P2PixelResult`。
- **非目标**：不决定候选资格/排异（SCI-REJ）；不估计噪声权重策略（SCI-NOISE）；不进行天区重投影（SCI-DRIZZLE）；不存完整协方差（见 `UNCERTAINTY_AND_COVARIANCE.md`）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `values[i]` | 校准后 accepted 样本值 | `P2PixelStack.values` |
| `weights[i]` | 数值权重 `w_i`（可空=等权 1.0） | `P2PixelStack.weights` |
| `support[i]` | 覆盖支撑 [0,1]（可空=1.0） | `P2PixelStack.support` |
| `accepted[i]` | 排异接受掩码（可空=全接受） | `P2PixelStack.accepted` |
| `count` | 输入候选数 | `count` |
| `wsum` | `Σ w_i·[accepted∧finite∧w_i>0]` | `integrate.cpp` |
| `vs` | `Σ w_i·x_i` | 同上 |
| `sup_max` | `max(accepted support)` canonical reducer | `sup_max` |
| `n_used` | 实际参与积分样本数 | `P2PixelResult.n_used` |
| `P2_INTEGRATE_*` | `OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT` | `integrate.h` |

## 3 物理量和单位

- `x, signal, values`: ADU（与校准后同一标度）；`w, ivar`: 信号⁻²（但本层不编码 ivar 语义，仅数值权重）；`support`: 无量纲 [0,1]；计数 `n_*`: 无量纲；`wsum`: 信号⁻²。

## 4 输入有效域

- `count` 可为 0（⇒ `NO_CANDIDATES`）；`values` 非空才计算，否则 `NO_CANDIDATES`。
- `weights` 可空（等权）；非空时每个 `w_i` 须 `finite ∧ ≥0`；`w==0` 合法但不贡献，`w<0` 或非有限 ⇒ `INVALID_INPUT`。
- `support` 可空（置 1.0）；非空时每个 `support_i` 须 `finite ∧ >0` 否则该样本 `INVALID_INPUT`。
- `accepted` 可空（全接受）；非空时仅 `accepted[i]==1` 的样本进入有效集。
- `values[i]` 须有限，否则 `INVALID_INPUT`。

## 5 连续定义

```text
输入:  values[0..count-1], weights[0..count-1] (可空), support[0..count-1] (可空), accepted[0..count-1] (可空)

资格 (eligibility):
  valid(i) ⇔ accepted(i) ∧ finite(values[i]) ∧ (support 空 ∨ (finite(support[i]) ∧ support[i]>0))
               ∧ (weights 空 ∨ finite(weights[i])) ∧ weights[i]≥0
  invalid_input ⇔ ∃i: accepted(i) ∧ (¬finite(values[i]) ∨ non-finite support≤0 ∨ non-finite weights ∨ weights<0)

聚合:
  n_accepted      = |{i | accepted(i)}|
  n_finite        = |{i | valid(i) ∧ weights[i]≥0}|
  n_positive_weight = |{i | valid(i) ∧ weights[i]>0}|  (weights 空时视 1.0)
  若 invalid_input           ⇒ status=INVALID_INPUT, n_used=已计数部分
  否则若 n_positive_weight==0:
       status = (n_accepted==0) ? ALL_REJECTED : ZERO_VALID_WEIGHT
  否则:
       wsum = Σ_{valid,W>0} w_i
       vs   = Σ_{valid,W>0} w_i·x_i
       signal  = vs / wsum
       support = (support 空) ? 1.0 : max_{valid,W>0} support[i]   # canonical reducer, 覆盖并集保守下界
       n_used  = n_positive_weight
       status  = OK
```

与 `lib/phase2/src/integrate.cpp:10-79` 及 `lib/phase2/include/astro/phase2/integrate.h:1-75` 一致。`support` 为 `max(accepted support)` 且仅消费 `pr.support`，调用方不二次 max/mean。

## 6 假设

- 权重与信号独立（由 SNR-010 噪声模型保证）；帧间像素已 `UPM.calibrate_block` 到公共零底；输入栈已排异分层（invalid/UNDERDETERMINED 已在 REJECTION 层处理）。

## 7 独立不变量

- **零权重惰性**：`w_i==0` 时 `signal` 与未提供该样本等价（`continue` 不贡献），与 `ZERO_VALID_WEIGHT` 区分（全 0 才触发）。
- **常量场不变量**：`values[i]=C` 常数且 `w_i>0` 时 `signal=C`（与权重分布无关）。
- **空支撑守恒**：无 `support` 输入时 `support=1.0`，不产生伪 0 支撑。
- **确定性**：加权求和顺序按输入索引 `i=0..count-1` 固定，`signal` 确定性（FP64 舍入仅顺序确定性）。
- **支撑单调性**：`sup_max = max(accepted support)`，增样本不减 `support`。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `count==0` / `values==null` | `NO_CANDIDATES` | `integrate.cpp:23` |
| 非有限 `values/support/weights` | `INVALID_INPUT` | `integrate.cpp:41-54` |
| 负权重 `w<0` | `INVALID_INPUT` | `w<0` 分支 |
| `w==0` 全部 | `ZERO_VALID_WEIGHT` (若有 accepted) | `n_positive==0` 分支 |
| 全拒 `n_accepted==0` | `ALL_REJECTED` | `n_accepted==0 ? ALL_REJECTED` |
| `support` 全空 | `support=1.0` | `support 空 ?1.0` |

## 9 精度策略

- FP64 `vs/wsum` 加权均值；求和顺序按索引固定以保证确定性；`support` 取 `max` 无浮点误差放大；不编码 ivar 语义，数值权重由调用方先经 `p2_validate_candidate_weights` 校验（负/NaN/Inf 拒）。

## 10 不可接受变化

- 将 `support` 改为 `mean`/`sum` 或二次聚合（canonical 为 `max`）；
- 将 `w==0` 改为 `INVALID_INPUT`（零权重为合法不贡献）；
- 将 `INVALID_INPUT` 合并为 `ZERO_VALID_WEIGHT`（显式互斥状态）；
- 在本层引入 `ivar/SNR` 策略（policy 在调用方，reducer 无知）；
- 改变求和顺序导致非确定性 `signal`。

## 11 验证 Oracle

- **常量场门**：常数 `values=C` 的 `signal==C`（`max_abs==0`）。
- **零权重门**：含 `w=0` 样本的 `signal` 与移除该样本等价（`TST-INT-ZERO`）。
- **状态穷尽门**：`NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT/OK` 五态互斥覆盖（`p2_validate_candidate_weights` 负/NaN/Inf 各一例）。
- **支撑门**：`support=max(accepted)` 与暴力 `max` 等价。
- **Python 参考**：NumPy 对同 `values/weights/support/accepted` 复算 `signal/support/status`（`rtol 1e-12`）。

## 12 关联 ALG ID

- `ALG-INT-001` `p2_integrate_pixel` 加权均值 + `max` reducer
- `ALG-INT-002` `p2_validate_candidate_weights` 资格校验（负/NaN/Inf 拒，0 合法）

## 13 追溯与测试

- 权威文件: `docs/science/INTEGRATION.md` (SCI-INT-001,002,004,008)
- 实现: `lib/phase2/src/integrate.cpp` (10-79), `lib/phase2/include/astro/phase2/integrate.h` (P2PixelStack/Result, P2_INTEGRATE_*)
- 公开 API: `p2_integrate_pixel, p2_validate_candidate_weights`
- 测试: `TST-INT-001` 常量场、`TST-INT-ZERO` 零权重、`TST-INT-FAIL-*` 四态、支撑 `max` 门（新增/映射见 `docs/TRACEABILITY.csv`）
