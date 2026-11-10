# 08｜有向舍入与数值线性代数：MPFR、IEEE 754、QR/SVD

**状态：`PRIMARY_READ_AND_MAPPED`（MPFR 4.2.2 §4.4、LAPACK 例程文档）；`PRIMARY_LOCATED_NOT_YET_READ`（IEEE 754-2019 正文、Golub & Van Loan、Higham、Wedin 1972）**
**核对日期：2026-11-10　核对人：lit-researcher**

## 1. 规范书目与永久标识

| 项 | 内容 | 状态 |
|---|---|---|
| MPFR 手册 | GNU MPFR **4.2.2**，《MPFR: The Multiple Precision Floating-Point Reliable Library》，<https://www.mpfr.org/mpfr-current/mpfr.html>（本次读取时 `mpfr-current` = 4.2.2，见手册页首） | **已读 §4.4** |
| IEEE 754 | IEEE Std 754-2019, *IEEE Standard for Floating-Point Arithmetic*, DOI <https://doi.org/10.1109/IEEESTD.2019.8766229> | **仅定位**（付费墙；**未读任何条款**） |
| LAPACK 例程文档 | netlib 现行源码：`dgelsd.f`、`dgels.f`、`dgesvd.f`、`dgeqrf.f`，<https://www.netlib.org/lapack/double/dgelsd.f> 等 | **已读头注释** |
| Golub & Van Loan | *Matrix Computations*, 4th ed., JHU Press（DOI <https://doi.org/10.1017/CBO9780511804441> 为 CUP 版） | **仅定位**（无合法免费全文） |
| Higham | *Accuracy and Stability of Numerical Algorithms*, 2nd ed., SIAM 2002 | **仅定位**（无合法免费全文） |
| Wedin | P.-Å. Wedin (1972), *Perturbation bounds in connection with singular value decomposition*, BIT 12, 99–111, DOI <https://doi.org/10.1007/BF01932678> | **仅定位**（ACSD 的 `sigma_error_upper` 需要的扰动界来源，未读） |

## 2. 已核对的章节/式

### MPFR 4.2.2 §4.4 “Rounding”

| 要点 | 原文 |
|---|---|
| 六种舍入模式 | `MPFR_RNDN`（round to nearest, **roundTiesToEven**）、`MPFR_RNDD`（roundTowardNegative）、`MPFR_RNDU`（roundTowardPositive）、`MPFR_RNDZ`（roundTowardZero）、`MPFR_RNDA`（away from zero）、`MPFR_RNDF`（faithful，**实验性**） |
| 有向舍入定义 | 对 `MPFR_RNDD`：`y ≤ x` 且最接近 `x`；`RNDU`：`y ≥ x`；`RNDZ`：`|y| ≤ |x|`；`RNDA`：`|y| ≥ |x|` |
| 正确舍入保证 | 「the value stored … is **correctly rounded** … the error on the result is less than or equal to **1/2 ulp** … in the rounding to nearest mode, and **less than 1 ulp** … in the directed rounding modes」 |
| 三值返回 | 函数返回 int（ternary value）：0 = 精确；正 = 结果大于精确值；负 = 小于。`RNDF` 的三值/inexact 标志**未定义**且结果**不可复现** |
| 零的符号 | 「in particular for a result equal to zero, **the sign is preserved** by the rounding operation」 |
| 与 IEEE 的关系 | 手册明确把 MPFR 模式逐一对到 IEEE 754 的 `roundTiesToEven`/`roundTowardNegative`/`roundTowardPositive`/`roundTowardZero`（**这是 MPFR 手册的陈述，不是 IEEE 条款原文**） |

### LAPACK 例程文档（头注释）

| 例程 | 已核对的算法陈述 |
|---|---|
| `dgelsd` | 求 `min ‖b − Ax‖₂` 的**最小范数解**，用 A 的 SVD；三步：(1) Householder 双对角化 → BLS；(2) divide-and-conquer 解 BLS；(3) 回代 Householder；**有效秩**由「小于 `RCOND × σ_max` 的奇异值视为零」决定 |
| `dgels` | QR/LQ 分解解满秩最小二乘（`m≥n` 用 QR，`m<n` 用 LQ） |
| `dgeqrf` | Householder QR（`A = QR`） |
| `dgesvd` | 双对角化 + 双对角 SVD（源码 `EXTERNAL` 列表含 `DGEBRD`、`DBDSQR`；即隐式 QR 迭代，**不是** divide-and-conquer）；头注释给出 `A = U·SIGMA·Vᵀ` 的约定 |

### 已核对的 ACSD 现状（不是论文，是源码，用于对照）

| 位置 | 事实 |
|---|---|
| [src/numerics/interval.cpp](../../../src/numerics/interval.cpp) | 用 C99 `fenv` 的 `fesetround(FE_DOWNWARD/FE_UPWARD)` 实现有向舍入；`detect_directed_rounding()` 探测平台是否真的遵守；不遵守时回退到“最近舍入 + 向外扩 1 ulp”，并报具名状态 `outward_ulp_expansion`；`#error` 禁止 `-ffast-math` |
| [src/numerics/linalg.cpp](../../../src/numerics/linalg.cpp) | **one-sided Jacobi SVD**（`jacobi_svd`，最多 60 sweeps），并给出 `delta_u`、`delta_v`、`residual`、`sigma_error_bound`、`rank` 等证书量 |
| 同上 `sigma_error_upper` | 注释写「The standard perturbation bound for a computed SVD: `|σ_i(A) − σ_i(USVᵀ)| ≤ e_R`，正交缺陷再加 `e_U‖S‖₂√(1+δ_V)` 与 `e_V‖S‖₂`」——**未给文献出处** |

## 3. 原文的假设与适用域

1. **MPFR 的正确舍入保证只对 MPFR 自己的运算成立**：它保证“给定精确输入、无限精度计算后正确舍入”，**不**保证你用 `double` 写的表达式也是正确舍入的。
2. **IEEE 754 定义的是基本运算与格式**；`sin`/`cos`/`exp`/`log` 等超越函数**不要求正确舍入**（各实现可有 ≤1–2 ulp 误差，允许“faithful”）。因此“IEEE 754 合规”**不能**推出“`std::sin` 的区间端点包含真值”。
3. **LAPACK 文档给的是算法与接口约定**，不是误差证明；`RCOND` 的秩判定是**约定**，不是“物理零”。
4. **Jacobi SVD 与 LAPACK 的 bidiagonal QR/D&C 是不同算法**：Jacobi 在高相对精度上有优势（经典结论），但 LAPACK 文档**不覆盖**它；ACSD 用 Jacobi 必须自己引来源并自证收敛/精度。
5. **扰动界**：`|σ_i(A) − σ_i(A+E)| ≤ ‖E‖₂`（Weyl）对奇异值成立；但 `sigma_error_upper` 里的正交缺陷项需要 Wedin/Davis–Kahan 类结果。**本轮未读 Wedin 1972**，故不能声称该式“有出处”。

## 4. ACSD 采用/变体所需的独立推导要点

1. **区间端点的来源**：ACSD 的包含性依赖“每个浮点运算向外舍入”。在 `fenv` 路径下，`+ − × ÷ √` 由 IEEE 754 的 `roundTowardNegative/Positive` 属性保证（MPFR 手册 §4.4 明确了这些属性的名字）；**超越函数不在保证内**，必须走 MPFR/有理逼近或显式误差界。ACSD 的 `outward_ulp_expansion` 回退只对**基本运算**有“最近舍入结果 ±1 ulp 包含真值”的论证，对 `sin/cos` 不成立——这是一个必须在文档里写死的边界。
2. **MPFR 的角色**：MPFR 是**参考/高精度路径**（用于细化、验证、`precision_exhausted` 前的升级），不是生产主路径。ACSD 源码里 MPFR 只出现在 [wcs_process.cpp](../../../src/coordinates/wcs_process.cpp) 的注释中（“dyadic interval primitives (MPFR/GMP managed buffers)”），**没有实际链接 MPFR**。不能写“ACSD 用 MPFR 保证舍入”。
3. **QR/SVD 的最小二乘**：`min‖Ax−b‖₂` 的解由 QR（满秩）或 SVD（秩亏，取最小范数）给出。ACSD 的 `jacobi_svd` + `classify_rank` 要在**给定 `rank_relative` 阈值**下复现 `dgelsd` 的“有效秩”语义；阈值是**约定**，不是数据性质。
4. **证书量的推导**：`residual` 是 `‖A − USVᵀ‖_F` 的区间上界；`sigma_error_bound` 要由 `residual + 正交缺陷项` 组成。后者需要：`‖A − USVᵀ‖ ≤ e_R` ⇒ 由 Weyl 得 `|σ_i(A) − σ_i(USVᵀ)| ≤ e_R`；再加 `USVᵀ` 与真 SVD 的差（正交缺陷）。**这条链必须在 NUMERICS 正本里逐步写出并引 Wedin**，当前只有一行注释。
5. **条件数**：`dgelsd` 的 `RCOND` 语义与 ACSD 的 `rank_relative` 不同（前者相对 `σ_max`，后者需明确是否同义）。文档必须写清，否则“秩认证”不可审计。

## 5. 对应 Process 与当前源码路径

| Process | 源码 | 对应 | 不能声称 |
|---|---|---|---|
| `numerics`（区间） | [src/numerics/interval.cpp](../../../src/numerics/interval.cpp)、[interval.hpp](../../../src/numerics/interval.hpp) | 有向舍入原语；`RoundingGuard` 线程安全 | 不能声称“IEEE 754 保证了我们的超越函数包含” |
| `numerics`（线代） | [src/numerics/linalg.cpp](../../../src/numerics/linalg.cpp)：`jacobi_svd`、`classify_rank`、`sigma_error_upper` | QR/SVD 的 ACSD 实现 | 不能声称“等价于 LAPACK”：算法不同（Jacobi vs 双对角 QR/D&C） |
| `numerics`（求积） | [src/numerics/quadrature.cpp](../../../src/numerics/quadrature.cpp) | 分段求积 | 原文无求积误差界；ACSD 自研 |
| `uncertainty.covariance` | [src/uncertainty/covariance.cpp](../../../src/uncertainty/covariance.cpp) | 用 SVD 证书做协方差 | 不能把 `rank_relative` 当物理秩 |

## 6. 不能声称的结论（红线）

- ❌「ACSD 使用 MPFR 做有向舍入」：生产路径是 `fenv` + ulp 回退；MPFR 只在注释里出现。
- ❌「IEEE 754 合规 ⇒ 区间包含」：超越函数不要求正确舍入。
- ❌「LAPACK 文档证明了 `dgelsd` 的误差界」：它只描述算法与 `RCOND` 约定。
- ❌「`sigma_error_upper` 是标准结论」：它需要 Wedin 类扰动界，当前**无出处**，属 `DERIVATION_TO_REVIEW`。
- ❌「Jacobi SVD 与 LAPACK 的 SVD 数值等价」：不同算法，误差特性不同。

## 7. 与现有 ACSD 文档的差异

1. [NUMERICS.md](../../engineering/rebuild/standards/NUMERICS.md) 与 [05-NUMERICS-AND-CERTIFICATION.md](../../workplans/v2-science-engineering/science/05-NUMERICS-AND-CERTIFICATION.md) 对“有保证的向外舍入”有明确要求，方向正确；但**未写明“超越函数不在 IEEE 保证内”**这一边界。建议补一句，否则 `sin/cos` 的区间端点会被误认为有保证。
2. `sigma_error_upper` 的注释自称 “standard perturbation bound” 却无引用——这是**最容易被独立审核击穿**的一处；本卡把 Wedin 1972 列为必读。
3. `interval.cpp` 的回退状态名 `outward_ulp_expansion` 是好的具名状态；但“最近舍入 ±1 ulp 包含真值”只对基本运算成立，文档需限定。
4. [docs/references/README.md](../README.md) 的“数值有向算术”行只列 MPFR 手册；建议补 IEEE 754-2019、LAPACK、Wedin 三条，并标读取状态。

## 8. 待办

- [ ] 读 Wedin 1972（或 Golub & Van Loan §8.6 / Higham §20–21 的相应定理），把 `sigma_error_upper` 的推导补成“定理 + 条件 + 本文实现”的形式。
- [ ] 核对 IEEE 754-2019 中 `roundTiesToEven`/`roundTowardNegative` 的条款号（当前只能引 MPFR 手册的映射）。
- [ ] 独立复核者构造一个 `rank_relative` 边界反例（如 `diag(1, 2^-40)`），检验秩分类与 `dgelsd` 的 `RCOND` 语义差异。
