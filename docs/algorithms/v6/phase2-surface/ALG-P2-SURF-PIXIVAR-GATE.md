# ALG-P2-SURF-PIXIVAR-GATE — pixel-ivar 近似门（ε 数值冻结）

- 文档 ID：`ALG-P2-SURF-PIXIVAR-GATE`（`ALG-P2-SURF-001` 子文档）
- 上位：`FZ-GATE-PIXIVAR-APPROX`（`SCI-ADJ-001` FREEZE_LIST §3.8）；`ADJ-P2-02`（`SC-ADJ-P202.1..3`）；`UNIFIED §5`；`DESIGN-P2 §6.1`；`PROJECT_SPEC §3`
- 本任务条款：`ALG-P2S-PXIV.1..7`
- **本任务冻结 ε 数值**（`ADJ-P2-02`："epsilon 的数值由 ALG-P2-SURF-001 冻结"）。

## 1 度量（`ALG-P2S-PXIV.1`）

以 `R~` 表示实现实际使用的组合算子（像素 ivar 近似），`C_in` 为输入协方差（含可表示共享项）。对每个输出元素 `p`：

```text
Var_GLS(p)    = [(A^T C_in^-1 A)^-1]_pp          （权威 Gauss–Markov 方差）
Var_approx(p) = [R~ C_in R~^T]_pp                （实际组合系数传播）
rho(p)        = Var_approx(p) / Var_GLS(p)       （>= 1，GLS 是 BLUE）
```

- `Var_GLS` 与 `Var_approx` **必须用同一 `C_in`**（含共享项/相关核）计算；不得用一个对角 `C~` 替代 `C_in` 再比较（否则比较无意义）。
- 只出对角 variance 时，`R~ C_in R~^T` 的非对角（相关核 `rho_ij`）仍须报告（`ADJ-AR-02`）。

## 2 结构性准入条件（必要条件；`ALG-P2S-PXIV.2`）

仅当 **四项全部** 满足，像素 ivar 近似才进入定量门：

1. **同点采样**：`A_k` 对该输出元素退化为选择/聚合（无跨元素重采样混合）；
2. **噪声独立**：`C_in` 帧块对角，且元素内噪声对角（无未表示相关）；
3. **a_k 一致**：帧已 UPM 归一至公共通量尺度（`a_k` 相同），或 `a_k` 显式并入权重 `w_k = a_k^2/var_k`；
4. **满秩**：`A^T C_in^-1 A` 满秩（rank_rtol=`FZ-AP2S-RANK-RTOL`）。

任一不满足 → 不得进入像素 ivar 近似路径；必须走完整 `A^T C_in^-1 A` 或报告 `R~ C_in R~^T` 与 `(A^T C_in^-1 A)^-1` 的比值（`ADJ-P2-02`）。

## 3 定量门：ε 与判据（`ALG-P2S-PXIV.3`）

| 冻结项 | 值 | 判据 |
|---|---|---|
| `FZ-AP2S-EPS-PIXIVAR`（ε） | **0.05** | 适用域内**面积加权 p95** 的 `rho` 满足 `rho_p95 <= 1 + eps = 1.05` |
| `FZ-AP2S-EPS-PIXIVAR-SUP`（逐元素硬上限） | **0.20**（= 4ε） | 任何有效元素 `rho(p) > 1.20` → 该元素 fail-closed（转空间模型或标 `unavailable`），不使整运行失败 |

**ε = 0.05 的判据来源**（登记，非任意）：GLS 是最小方差线性无偏估计，像素 ivar 是它的一个可行线性估计量，`rho >= 1` 恒成立。

- 方差膨胀 `rho=1.05` 对应标准差劣化 `sqrt(1.05)-1 = 2.47%`、深度（m5）损失 `0.027 mag`，落在光度/深度一致性目标的噪声预算内；
- `1.05` 与 `1.16`（`SCI-P2-001 Oracle C4.3` 实测"忽略 a_k 的 ivar 近似"劣化比）之间留出清晰判别余量——故意忽略 `a_k` 的近似（`rho≈1.16`）被 5% 门**干净拒绝**；
- 逐元素硬上限 `0.20`（≈9.5% 标准差劣化 / `0.10 mag`）用于 fail-closed，避免单个病态元素拖垮整张 map。

数值归属：本任务（W3）冻结 ε 度量与数值；正式冻结由 `CONTRACT-FREEZE-001`（W4）写入 `docs/science/v6/frozen/`，并建议负责人确认（`SO-07-SURF`）。**实现不得自行放宽。**

## 4 报告要求（`ALG-P2S-PXIV.4`）

使用近似 `R~` 时必须输出：

1. `R~ C_in R~^T`（实际系数传播方差）与 `(A^T C_in^-1 A)^-1`（权威）的比值 `rho` 图或摘要：`p05/p50/p95/max` + 采样覆盖 + 适用域（`ADJ-GEN-04` 同构）；
2. `covariance.method = combination_coefficients`、`combination_coefficients` 非空、`variance_from ∈ {combination_coefficients,...}`（`WEIGHT_PROVENANCE_GATE R6`）；
3. 误差门声明（`epsilon=0.05`、判据、所用 `C_in` 版本）。

**无误差门声明即 REJECT**（`ADJ-P2-02` gate；`FZ-GATE-PIXIVAR-APPROX`）。

## 5 负向判据（必须能红；`ALG-P2S-PXIV.5`）

| 注入 | 期望 |
|---|---|
| 忽略 `a_k` 的 ivar（`a_k` 不一致时）→ `rho > 1.05` | 门拒绝 / 必须升级完整 GLS |
| 相关帧/相关噪声按独立处理 → `rho > 1.05` | 门拒绝（`UNIFIED §10` 简单求和过度乐观检出） |
| 报告 `1/W_ideal` 而非实际 `R~ C_in R~^T` | 违反 `FZ-FORMULA-COV-PROP` → REJECT |
| 无误差门声明 | REJECT |
| 把 `support/coverage/median_source_snr/fwhm/residual` 当权重来源 | REJECT（`FZ-GATE-SUPPORT-COVERAGE`/`FZ-GATE-MEDIAN-SNR`） |

## 6 与点源模式的边界（`ALG-P2S-PXIV.6`）

像素 ivar 近似**不是**点源 PSF-aware 最优（`UNIFIED §11`）：在 PSF 不同的帧上，普通像素 ivar coadd 不保证最大点源 SNR，不得宣称等价（`DESIGN-P2 §6.2`）。点源目标走 `point_information`（`ALG-P2-POINT-001` 域），与本门无关。

## 7 与 HiPS 父级对角近似的区分（`ALG-P2S-PXIV.7`）

本门针对 Phase2 `surface_gls` 的**估计器近似**（`Var_approx/Var_GLS`）；Phase1 HiPS 父级 tile 方差对角近似是**另一个门**（`FZ-GATE-PARENT-VAR`，`deficit=(exact-diag)/exact`，阈值由 `ALG-P1-001` 冻结）。二者度量不同，不得混用。
