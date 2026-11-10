# 09｜BLUE/GLS 与非负最小二乘/QP

**状态：`PRIMARY_READ_PARTIAL`**——Boyd & Vandenberghe 已读（QP 定义、KKT）；NIST 加权最小二乘页已读；**Aitken (1935) 与 Lawson & Hanson 未读**
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 内容 | 状态 |
|---|---|---|
| Aitken 1935 | A. C. Aitken, *On least squares and linear combination of observations*, Proc. Roy. Soc. Edinburgh **55**, 42–48, DOI <https://doi.org/10.1017/S0370164600017806>（解析 302） | **仅定位**（未读正文） |
| NIST 标准文档 | NIST/SEMATECH *e-Handbook of Statistical Methods*, §4.1.4.3 “Weighted Least Squares Regression”，<https://www.itl.nist.gov/div898/handbook/pmd/section1/pmd143.htm> | **已读**（加权最小二乘的统计动机与权重含义） |
| Boyd & Vandenberghe | *Convex Optimization*, CUP 2004，免费 PDF <https://web.stanford.edu/~boyd/cvxbook/bv_cvxbook.pdf>；DOI <https://doi.org/10.1017/CBO9780511804441> | **已读 §4.4 式 (4.34)、§5.5.3 式 (5.49)** |
| Lawson & Hanson | *Solving Least Squares Problems*, SIAM Classics in Applied Mathematics **15**（1995 重印；原 1974 Prentice-Hall） | **仅定位**（无合法免费全文；NNLS 算法的具体章号未核对） |
| Golub & Van Loan | *Matrix Computations*（QR/SVD 的 LLS 章节） | **仅定位**（见卡 08） |

## 2. 已核对的章节/式

| 位置 | 原文内容要点 |
|---|---|
| NIST §4.1.4.3 | 加权最小二乘的动机：当各观测精度不同时，等权会“给低精度点过多影响、给高精度点过少影响”；权重反映各观测所含信息的精度 |
| Boyd §4.4 式 (4.34)（p.152） | QP 标准形：`min (1/2)xᵀPx + qᵀx + r  s.t. Gx ≼ h, Ax = b`，`P ∈ Sⁿ₊`；「minimize a convex quadratic function over a polyhedron」 |
| Boyd §5.5.3 式 (5.49)（p.243） | KKT 条件：`f_i(x*) ≤ 0`、`h_i(x*) = 0`、`λ*_i ≥ 0`、`λ*_i f_i(x*) = 0`、`∇f₀ + Σλ*_i∇f_i + Σν*_i∇h_i = 0`；并给出**凸情形下 KKT 充分**的陈述 |
| Boyd §5.5.3（p.244） | 若 `f_i` 凸、`h_i` 仿射，满足 KKT 即原/对偶最优且零对偶间隙 |

> **未读但必需的原文**：BLUE 的最优性（Gauss–Markov）与 GLS 的原始陈述在 Aitken 1935；NNLS 的算法与有限终止性在 Lawson & Hanson。本卡**不引用**它们的式号，只把它们列为待读来源。

## 3. 原文的假设与适用域

1. **Boyd 的 QP/KKT 是纯优化结论**：它不管 `P`、`G`、`A` 从哪来；把它用到统计上，需要另行证明“目标函数 = 方差/负对数似然”。
2. **KKT 充分性要求凸性**：`Γ` PSD ⇒ 目标是凸二次；若 `Γ` 不定，KKT 只是必要条件。
3. **NIST 的 WLS 是**对角权（独立观测）情形；**全协方差 GLS** 需要额外一步白化（`Γ^{-1/2}`），NIST 页不覆盖。
4. **Aitken/Gauss–Markov 的前提**（标准陈述，待原文核对）：模型线性、`E[ε]=0`、`Cov(ε)=Γ` **已知**、设计矩阵满列秩。任一条不成立，BLUE 就不是“最佳”。
5. **NNLS**：`min ‖Ax−b‖₂ s.t. x ≥ 0`；解存在且唯一当 `A` 满列秩；秩亏时解集可能非单点，需额外规则（ACSD 取最小 2 范数）。

## 4. ACSD 采用/变体所需的独立推导要点

### 4.1 BLUE / GLS

1. **白化**：`Γ` 对称正定，令 `Γ = LLᵀ`（Cholesky）。则 `min (y−Aq)ᵀΓ⁻¹(y−Aq)` 等价于对 `L⁻¹(y−Aq)` 做普通最小二乘。正规方程 `AᵀΓ⁻¹A q = AᵀΓ⁻¹y`，解 `q̂ = (AᵀΓ⁻¹A)⁻¹AᵀΓ⁻¹y`，`Cov(q̂) = (AᵀΓ⁻¹A)⁻¹`。**要求 `A` 满列秩**（否则逆不存在）。
2. **归一化约束**：ACSD 的权重解带 `1ᵀq = 1`（权重和为 1）。这是**等式约束**的 GLS：用 Lagrange 乘子得 `[Γ 1; 1ᵀ 0][q; μ] = [0; 1]`，即 `q = Γ⁻¹1/(1ᵀΓ⁻¹1)`，方差 `1/(1ᵀΓ⁻¹1)`。ACSD 文档写 `Γq = 1, t = q/(1ᵀq)` 是同一解的写法，**需在正本里逐步写出**。
3. **`Γ` 半正定（PSD）**：若 `Γ` 奇异，`Γ⁻¹` 不存在。正确做法是限制在 `range(Γ)` 上求解并保留零方差方向的精确约束（ACSD NUMERICS 已如此规定）；**不能**加 ridge/floor（那会改变目标，见 NUMERICS.md 的禁令）。
4. **gauge**：若 `A` 的零空间非平凡（如 sky 的全局常数），`AᵀΓ⁻¹A` 奇异；必须显式加 gauge 约束（固定参考帧或 `1ᵀq=1`），并证明约束后的解唯一。
5. **“最佳”的边界**：BLUE 只在 `Γ` **已知且正确**时最小方差；`Γ` 估错时它仍无偏（线性 + `E[ε]=0`），但不再最小方差。ACSD 不能声称“用 BLUE ⇒ 最优”，只能说“在声明的 `Γ` 下最优”。

### 4.2 NNLS / QP

1. **写成 QP**：`min (1/2)tᵀΓt  s.t. 1ᵀt = 1, t ≥ 0`，即 Boyd 式 (4.34) 的特例（`P=Γ`、`q=0`、`G=−I`、`h=0`、`A=1ᵀ`、`b=1`）。
2. **KKT**（Boyd 式 5.49）：存在 `λ ≥ 0`、`ν` 使 `Γt − λ + ν1 = 0`、`λ_i t_i = 0`、`1ᵀt = 1`、`t ≥ 0`。ACSD 的停止判据正是 stationarity + complementarity + 可行性 + 归一（NUMERICS.md 已写）。
3. **active-set 有限终止**：需要非退化假设；退化时按 Bland 规则（最小下标）防循环。ACSD 用 frame ID 打破平手，等价于给变量一个全序，**需在正本里证明**它满足 Bland 的无循环条件。
4. **多解**：`Γ` PSD 奇异时最优解集可能是凸集；ACSD 取最小 2 范数解。这要求 `Γ` 的零空间与约束的交可计算，且“最小范数”是**约定**，不是统计结论。
5. **非负解不是 BLUE**：加上 `t ≥ 0` 后，解可能有偏（约束估计），ACSD 文档已写“不把非负解称无约束 BLUE”——正确。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `coadd.weights` | [src/coadd/weights.cpp](../../../src/coadd/weights.cpp)：`solve_blue`、`solve_nonnegative_qp` | §4.1/§4.2 的 BLUE 与 QP | 不能声称 BLUE 在 `Γ` 未知时仍最优 |
| `coadd.weights` | 同上 `solve_blue` 的 `rank_relative` 参数 | PSD/秩处理 | 不能把 `rank_relative` 当物理秩 |
| `sky.fit` | [src/sky/sky.cpp](../../../src/sky/sky.cpp)、[sky_fit_process.cpp](../../../src/sky/sky_fit_process.cpp) | 全局 GLS + gauge | 不能声称 gauge 由数据决定 |
| `numerics` | [src/numerics/linalg.cpp](../../../src/numerics/linalg.cpp)（`jacobi_svd`、`classify_rank`） | BLUE 的求解/秩 | 见卡 08 |
| `uncertainty.covariance` | [src/uncertainty/covariance.cpp](../../../src/uncertainty/covariance.cpp) | 协方差传播 | 不能把 `Γ` 的估计误差忽略 |

## 6. 不能声称的结论（红线）

- ❌「Aitken 1935 证明了我们的 active-set 收敛」：Aitken 是 GLS/BLUE 的统计结论，与 active-set 无关；且本卡**未读** Aitken。
- ❌「Boyd 的 KKT 定理保证了我们的解最优」：KKT 只在你把问题写成凸 QP 且约束正确时成立；`Γ` 不定或约束写错时不成立。
- ❌「BLUE 在任何 `Γ` 下都最优」：`Γ` 必须已知且正确。
- ❌「NNLS 解唯一」：秩亏时不唯一；ACSD 的最小范数是约定。
- ❌「非负 QP 解是 BLUE」：加约束后不再是无约束 BLUE。

## 7. 与现有 ACSD 文档的差异

1. [NUMERICS.md](../../engineering/rebuild/standards/NUMERICS.md)「BLUE、PSD 和非负 QP」节写 active-set 从 equal 可行解开始、沿线段走到最先碰 0、Bland 规则、最小 2 范数多解规则——**与 Boyd 的 QP/KKT 框架一致**，且比原文更具体（原文没有 active-set 的工程细节）。问题：该节**没有引用任何来源**；建议补 Boyd §4.4/§5.5.3（KKT）与 Aitken（BLUE）两条，并注明 Aitken 待读。
2. [STATISTICS_SKY_COADD.md](../../science/foundation/STATISTICS_SKY_COADD.md) 用 `Γq=1, t=q/(1ᵀq)` 的写法；本卡 §4.1 给出等价推导（等式约束 GLS），建议正本采用“Lagrange → `q=Γ⁻¹1/(1ᵀΓ⁻¹1)`”的写法，便于审计。
3. `coadd.weights.md` 的 W-NEG 例子（`Γ=[[1,2],[2,5]]`，BLUE=`[3/2,−1/2]`）与 W-PSD（`diag(0,1)`）是**独立有理数真值**，与 BLUE 公式一致；本卡不重复验证，但指出它们可作为 Aitken 公式的回归测试。

## 8. 待办

- [ ] 读 Aitken 1935（或等价的 Gauss–Markov 定理原始陈述），补上 BLUE 的正式引用。
- [ ] 读 Lawson & Hanson 的 NNLS 章，核对 active-set 与有限终止的原始陈述；补上章号。
- [ ] 独立复核者用 W-PSD/W-NEG 例子，脱离源码重算 BLUE 与 QP，验证 `Γ` 奇异时的 range/gauge 处理。
