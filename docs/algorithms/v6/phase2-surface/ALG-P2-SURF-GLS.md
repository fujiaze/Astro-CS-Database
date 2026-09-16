> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# ALG-P2-SURF-GLS — surface_gls 扩展源 GLS 算法规格

- 文档 ID：`ALG-P2-SURF-GLS`（`ALG-P2-SURF-001` 子文档）
- 上位：`DESIGN-P2-001 §6.1/§7/§9/§10`；`UNIFIED §5/§7/§8/§10`；`PROJECT_SPEC §3/§5/§7/§8`；宪章 §4.1/§6.3
- 裁决锚：`ADJ-P2-02`（`SC-ADJ-P202.1..3`）；`FZ-FORMULA-GLS`；`ADJ-AR-02`（`SC-ADJ-AR02.1..2`）
- 文献锚：Horne 1986 DOI:10.1086/131801；Naylor 1998 MNRAS 296,339；Fruchter & Hook 2002 PASP 114,144
- 本任务条款：`ALG-P2S-GLS.1..8`

## 1 目标与模式归属（`ALG-P2S-GLS.1`）

`surface_gls` 是扩展源/面亮度的生产科学模式（`FZ-MODE-PRODUCTION`）。目标：对同一输出 sky element 的未知面亮度 `x` 给出**最小方差无偏线性估计**（`Cov(n)=C` 正确时 Gauss–Markov BLUE，`UNIFIED §5`；`DESIGN-P2 §6.1`）。

- 生产模式仅显式三模式 `point_information/surface_gls/psfsw_robust`（`ADJ-S1`）；`equal/pixel_ivar` 仅作**基线比较**（`FZ-MODE-BASELINE`），不得声明科学最优。
- `psf_snr_power` 本包 **DEFERRED/NOT_IMPLEMENTED**，不得进入生产路由（`FZ-MODE-DEFERRED`；`C-004.1`；`ADJ-C004-01`）。
- 默认模式由配置显式选择，不得由"检测到多少颗星"等偶然因素自动切换（`SCI-PSFW-001 §4`）。

## 2 权威公式（原样继承 `FZ-FORMULA-GLS`；`ALG-P2S-GLS.2`）

```text
d = A x + n,   Cov(n) = C_in
x_hat       = (A^T C_in^-1 A)^-1 A^T C_in^-1 d
Cov(x_hat)  = (A^T C_in^-1 A)^-1
R           = (A^T C_in^-1 A)^-1 A^T C_in^-1      (实际组合算子)
```

- 恒等式门：`R C_in R^T == (A^T C_in^-1 A)^-1`，容差 `FZ-AP2S-IDENT-RTOL = 1e-9`（`SCI-P2-001-COVARIANCE-EPSF §2`）。
- 标量输出形式（单输出元素）：`Var(x_hat) = c^T C_in c`，`c` 为实际组合系数（`FZ-FORMULA-COV-PROP`）。
- **禁止**从权重标量反推 variance（`FZ-FORMULA-COV-PROP` gate；`RULINGS #5`）。

### 2.1 设计矩阵 A 的可实施定义（`ALG-P2S-GLS.3`）

对每个输出 sky element `p`，把贡献该元素的输入像素 `i∈N(p)` 按帧 `k` 堆叠：

```text
A[(k,i), p] = a_k * (P_k (x) K_k)(u_i^k - u_p)      # 单位: 1/px^2 (面亮度) 或 1 (per-pixel flux)
```

- `a_k`：帧 k 的 UPM 归一后光度尺度（无量纲；含 `g_k` 与归一，见 `ALG-P2-SURF-UPM.md`）；
- `P_k`：帧 k 的有效 PSF（`Σ P_k = 1`，相位已对齐输出元素）；
- `K_k`：重采样/插值核（版本化 kernel registry；`DESIGN-P2 §6.1`）；
- `(x)`：卷积；`u`：像素/天球坐标。

`A` 的乘积 `A^T C_in^-1 A` 是唯一权威正规矩阵；**禁止**用"先 coadd 后除权重"的等价式替代 `A^T C_in^-1 A`（`DESIGN-P2 §6.1`）。

### 2.2 C_in 的可表示形式（`ALG-P2S-GLS.4`）

`C_in` 必须纳入（`DESIGN-P2 §6.1`；`UNIFIED §6`；`ADJ-OBS-01`）：

1. 独立随机项 → 对角 variance/ivar；
2. 共享系统项（共同 master、共同天光/背景、重采样相关）→ 低秩因子 `L L^T`、相关核 `rho_ij`、或共同 master ID+强度参数之一（`ADJ-F-OBS-04`，`SC-ADJ-F04.1..3`）；
3. UPM 参数不确定度 → `J C_theta J^T`（见 `ALG-P2-SURF-UPM.md §5`）；
4. 模型偏差（光度零点、PSF 模型误差、WCS）→ validity/quality + 系统误差预算，**禁止**伪装随机 ivar。

若共享项无法表示 → 显式 `unavailable` 或输出系统误差预算，**禁止**按独立随机项静默处理（`ADJ-OBS-01` gate）。

### 2.3 条件式退化（`ALG-P2S-GLS.5`）

仅当 **同时** 满足以下条件，`surface_gls` 才可退化为像素 ivar 加权平均：

1. 同一输出量、线性算子退化为**同点采样**（无跨元素重采样混合）；
2. 帧间噪声独立、元素内噪声对角；
3. `a_k` 一致（帧已 UPM 归一至公共通量尺度，或 `a_k` 显式并入权重）。

否则必须用完整 `A^T C_in^-1 A`。**该退化不是点源 PSF-aware 最优的替代**（`UNIFIED §11`）。
白噪声近似 `W_info = a^2/(sigma_pix^2 A_NEA)` 为**条件式**，仅在 `C` 对角且 `sigma_pix` 声明时可用（`FZ-COND-WHITENOISE`）。

## 3 输入（`ALG-P2S-GLS.6`）

| 输入 | 单位/域 | 来源 | 缺省行为 |
|---|---|---|---|
| `d_k` 校准后 signal | `ADU/px^2`（面亮度）或 `ADU`（per-pixel flux，须声明） | Phase1 HiPS + UPM apply | 缺单位声明 → REJECT |
| `a_k` / `g_k` | 1 | UPM/光度响应 | 缺 → REJECT（点源/扩展源均需） |
| `P_k` | 归一 `ΣP=1` | Phase1 PSF 模型（空间量按位置求值） | PSF 未归一 → REJECT |
| `K_k` | 线性算子 | kernel registry | 未注册核 → REJECT |
| `C_in` | `signal_unit^2` | Phase1 variance + 共享项 + UPM | 只对角且无相关核 → REJECT |
| `validity`/`rejection` | 门 | 本目录 `ALG-P2-SURF-REJECTION.md` | 空拒绝不得静默当有效 |
| `provenance` | — | `FZ-PROV-MINIMAL-SET` | 缺键 → REJECT |

## 4 输出（`ALG-P2S-GLS.7`；`DESIGN-P2 §9`）

`surface_brightness` 产品族必须包含：`signal`、`variance`、**相关描述**（correlation kernel 或可重建算子摘要）、`effective_psf`、`support`、`coverage`、`validity`/`rejection`、UPM 参数与残差、`provenance`。
不得只写一张 signal + 一个语义不明的 weight（`DESIGN-P2 §9` 末段）。`support/coverage` 只作门，禁作 inverse-variance/SNR/科学权重（宪章 §6.3；`FZ-GATE-SUPPORT-COVERAGE`）。

单位：`signal_sb = ADU/px^2`、`sb_variance_out = ADU^2/px^4`、`sb_ivar_out = px^4/ADU^2`；二次律 `variance = signal^2`、`ivar = 1/variance`（`ADJ-GEN-01`）。

## 5 fail-closed 条件（`ALG-P2S-GLS.8`）

| 条件 | 行为 |
|---|---|
| `A^T C_in^-1 A` 秩亏（rank < 未知数，rank_rtol=`FZ-AP2S-RANK-RTOL`） | 该元素/块 `unavailable`，**禁止**伪逆静默 |
| 条件数 `kappa > FZ-AP2S-KAPPA-MAX` | 分组件或 `unavailable` |
| 共享系统项不可表示 | `unavailable` 或系统误差预算（`ADJ-OBS-01`） |
| 只出对角 variance 无相关核/算子摘要 | REJECT（`ADJ-AR-02`） |
| 像素 ivar 近似无误差门声明 | REJECT（`ADJ-P2-02` gate；见 `ALG-P2-SURF-PIXIVAR-GATE.md`） |
| UPM 参数协方差未传播 | REJECT（`ALG-P2S-UPM.6`） |
| BUNIT 量纲不可判 | REJECT/`unavailable`（`FZ-BUNIT-SEMANTICS`） |
| `unavailable` 无原因 | REJECT（`FZ-PROV-MINIMAL-SET`） |

## 6 空间模型与标量降级（`ALG-P2S-GLS.8` 续；`FZ-DEGRADE-SCALAR`）

`W_info/背景/variance/photometric response/PSF` 默认空间量。压成帧级标量必须**同时**过 (a) 空间残差/趋势门与 (b) 功率损失门，并输出 `p05/p50/p95` + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域；任一不满足 → 存 map/model/control points（`UNIFIED §8`；`ADJ-GEN-04`）。真实数据 M42/银心接缝检查归 `REAL-SCIENCE-001`（Wave 10），不在本任务。

## 7 禁止项（缺省即红）

- `support/coverage/median_source_snr/fwhm/residual` 作权重来源（`FZ-GATE-SUPPORT-COVERAGE`/`FZ-GATE-MEDIAN-SNR`）；
- 像素 ivar average 对任意 PSF 点源目标宣称最优（`UNIFIED §11`）；
- 从权重标量反推 variance（`FZ-FORMULA-COV-PROP`）；
- 跨帧相关却用 `Σ_k` 简单求和（相关帧必须联合 `C_in`；`UNIFIED §4/§10`）；
- 把 `psf_snr_power` 放进生产模式或路由（`C-004.1`）。

## 8 验证门（`DESIGN-P2 §10`）

1. 独立高精度矩阵/NumPy Oracle 验证 GLS 与 covariance（`G C G^T == (A^T C^-1 A)^-1`）；
2. 扩展源常量场、梯度、总通量与方差无偏；
3. 相关噪声失配能红（简单求和过度乐观被检出）；
4. 忽略 `a_k`/相关项的 ivar 近似方差严格劣化（比值 > 1）必须可复算；
5. 每个近似有 mutation 能使门变红。
实测 rc 见 `ALG-P2-SURF-VERIFICATION.md`。
