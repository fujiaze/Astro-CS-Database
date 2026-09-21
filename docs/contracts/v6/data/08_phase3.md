> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# 08 — Phase3 投影/采样 schema（Omega_i / 采样核 registry / Q-W 重算 / fail-closed）

> 上游：ASTROCS_DESIGN.md §3.1（数据对象）、§8.4（模块与 ABI）

上位锚：`FZ-P3-MODES`、`FZ-P3-FAILCLOSED`、`FZ-P3-QW-RECOMPUTE`、`FZ-P3-KERNEL-REGISTRY`、`FZ-P3-BUNIT-QUADRATIC`、`FZ-FORMULA-COV-PROP`；
DESIGN-P3 §1/§2/§3/§4/§5；PHASE3_REVIEW C-P3-PROP-1..16；ADJ-P3-01/ADJ-S4；F3-01..06；Calabretta & Greisen 2002。
机器：`contracts/proposals/v6/data/astrocs.v6.phase3.v1.schema.json`（`phase3.v1`）；正例 `examples/phase3.example.json`。

## 1. 三输出模式与 measurement 语义

| 模式 | signal 语义 | measurement_capable | 备注 |
|---|---|---|---|
| `surface_brightness` | 行归一面亮度 | true（声明测量则必需 variance） | 常数场不变量要求行归一 |
| `point_source_flux` | Q/W/flux/detection | true | 必需 PSF / effective PSF / Omega / a / C_y |
| `visualization` | 显示拉伸 | **必须 false** | 禁写 VARIANCE/IVAR/POINT_INFORMATION 作测量层 |

## 2. 逐像素立体角 Omega_i（F3-03 归属本任务设计面）

```text
Omega'_i = |det(d(sky)/d(pixel))|_i        （TAN/SIN/CAR/AIT 各由 FITS WCS Paper II 定义）
CAR 面积元: Omega = s_rad * (sin dec_hi - sin dec_lo)  （**仅 |CRVAL2|<=1e-9（未倾斜）成立**；
           CRVAL2≠0 时行是倾斜等纬线，该解析式不成立——v3 起 CAR/AIT 的 CRVAL2 进映射，
           Paper II §2.2 三 Euler 角；±60deg、CRVAL2=0 时 max/min = 2.0000）
AIT 等积:   max/min = 1.00000006
```

`phase3.v1.omega` 要求 `units="sr"`、`representation`、`required_for`、`projection_id`。
**surface_brightness 做 flux 换算而无逐像素 Omega → REJECT**；`point_source_flux` 必需 Omega。
禁「常数 Omega」近似冒充逐像素面积元（Oracle P3 常数 Ω 通量偏差 97.5%）。

## 3. 采样核 registry（`FZ-P3-KERNEL-REGISTRY`）

采样核是**产品语义**，由版本化 registry 注册，不能由现码倒推冻结：

| family | 状态要求 | 语义 |
|---|---|---|
| `nearest` | 仅 mask/诊断/显式用户选择 | 离散 |
| `bilinear_4quad` | 须独立 Oracle + 通量/面亮度语义 + 误差/边界定义后注册 | 面亮度（行归一）或通量（列归一） |
| `higher_order` | 各自注册并带独立 Oracle、误差门、适用域 | 连续场 |

`status != registered` 用于生产、或 `registered` 而无 `oracle_ref`/`boundary_definition` → REJECT。
variance 传播：`nearest: var_out=u_in`；`bilinear: var_out=Sum_k c_k^2 u_k`（`Σc_k=1` 但 `Σc_k²≠1`，禁误用 Σc_k 归一 variance）。

## 4. 输出帧 Q/W 重算（`FZ-P3-QW-RECOMPUTE`）

```text
a = 光度响应尺度;  pi = S p  (输出有效 PSF)
Q = a * pi^T C_y^-1 f;  W = a^2 * pi^T C_y^-1 pi;  F_hat = Q/W;  Var(F_hat) = 1/W
```

- **必须**在输出帧以输出有效 PSF 与完整（或带相关核的）`C_y` 重算；**禁止**重采样输入 Q/W；
- Phase3 **消费**上游 `W_info`，不重算、不替换；`psfsw_robust_weight` 等无量纲相对权重**不得**写入 ivar/variance；
- `R` 与匹配滤波不可交换：`W_out != Sum_i (核) W_in,i`；
- 缺 PSF / PSF 未归一 / 缺 point_information 且不可重建 / 仅对角 covariance 无相关核 / 缺 a / 未输出 effective PSF → REJECT。

## 5. variance / correlation（沿用 `FZ-FORMULA-COV-PROP`）

`C_y = R C_x R^T`；`Var(y_i) = Σ_j R_ij² [C_x]_jj + 2 Σ_{j<k} R_ij R_ik [C_x]_jk`。
现行 `Σ c_k² u_k` 只在输入 `C_x` 对角时严格；Drizzle 输入 `mean|ρ|≈0.19` 下对角省略低估 **23.3%**，
对角化后声明的 `1/W_diag` 比真实方差低 **28.7%**（须被检出，Oracle P3-O5）。
Phase3 输出 variance BUNIT = (主 HDU signal BUNIT)²，ivar = 1/variance。

## 6. fail-closed 矩阵（C-P3-PROP-16，12 门 mutation 全红）

| 模式 | 拒绝条件 |
|---|---|
| `surface_brightness` | 无 Omega 却 flux 换算；声明测量却 uncertainty unavailable；BUNIT 非二次律；只出对角无相关核 |
| `point_source_flux` | 缺 PSF / PSF 未归一 / 缺 point_information 且不可重建 / 对角无相关核 / 缺 a / 未出 effective PSF |
| `visualization` | `measurement_capable=true` 或写 VARIANCE/IVAR/POINT_INFORMATION 作测量层 |
| 全局 | 把上游相对复合权重写成 ivar/variance |

## 7. 迁移建议

- 现行会话对 `projection != "TAN"` 返回 UNSUPPORTED，且无 `Omega`/effective PSF/Q-W：属 NOT_IMPLEMENTED，实现归 W5/W7。
- 生产 exchange schema 的 fits geometry 仅 TAN：四投影与三模式扩展须 W6 与 runtime validator 同一提交（F-UNC-003）。
