# Astro Celestial Sphere Database（ACSD） Numeric Standard

> 上游：ASTROCS_DESIGN.md §3.3（科学量与星表，:185「每个科学量写清五件事：单位、坐标系、归一化、精度要求、有效有限域」）、§8.5（模块与 ABI）

## 每个科学 double/float 必须文档化

- 单位（ADU / e- / mag / deg / arcsec / 无单位比率）；
- 坐标系（pixel / WCS RA-Dec / HEALPix NESTED / tile+local xy）；
- normalization（除以曝光、中值、median 等）；
- precision requirement（FP32/FP64 边界；默认 science=FP64）；
- valid finite domain。

## 量纲与标度（最高设计 §3.3:185「归一化」一项的细化）

> 依据：`ASTROCS_DESIGN.md` §3.3:185「每个科学量写清五件事：单位、坐标系、归一化、精度要求、有效有限域」。
> 本条只把其中的**单位（量纲）**与**归一化（标度）**两件事写成可判定形式，不新增要求；下级文档在本条上只能细化。

每个科学量的文档化条目必须**同时**写明量纲与标度类别，缺一即条目不完整：

1. **量纲**：完整单位幂次串，**含立体角幂**（`ADU`、`ADU/sr`、`ADU^2/sr^2`、`sr^2/ADU^2`、`W·m^-2·nm^-1`）。
   单位串的语义权威 = **FITS Standard 4.0 §4.3 `BUNIT`**：「The value field shall contain a character string
   describing the physical units in which the quantities in the array, **after application of BSCALE and BZERO**,
   are expressed.」单位串写法按该标准指向的 IAU Style Manual（McNally 1988）。
   **禁止**省略立体角幂（面亮度方差是 `ADU^2/sr^2`，不是 `ADU^2`）。
2. **标度类别（scale class）**：该量数值所在的线性标度，取值只能来自下表（封闭词表）：

| token | 定义 | 量纲 | 现行承载面（示例） |
|---|---|---|---|
| `raw_adu` | 探测器计数域，物理值 = `BSCALE·样本 + BZERO` | ADU | 输入亮场/母版 |
| `calibrated_adu` | 校准后、测光归一化**前** | ADU | `cal` 面；帧级噪声节点 `p1_op_noise` 的消费面（`module_adapters.cpp:5648-5653`） |
| `photo_scaled_adu` | 已施加逐帧测光标度 `x′ = α·x`，`α = frame photscal`（**逐帧量，不是全仓常数**） | α × ADU | `photoapplied_<base>`；drizzle `data`/`variance` 块（`module_adapters.cpp:6457-6460`） |
| `surface_brightness` | 线性量除以像素立体角 `A_cell` [sr] | `<标度量纲>/sr` | HiPS signal/variance/ivar 子产品（`DATA_SEMANTICS` §12.2） |
| `synthetic_flux` | 模型通带积分辐照度 `F_syn` | `W·m^-2·nm^-1` | 测光定标通道 |

**线性标度律（强制）**：量按 `x′ = α·x` 变换时

```text
Var(x′) = α²·Var(x)          ivar(x′) = ivar(x) / α²
```

- **证据（一手）**：JCGM 100:2008（GUM）§5.1.2 式(10) `u_c²(y) = Σ_i (∂f/∂x_i)²·u²(x_i)` 在 `y = α·x` 上的
  线性特例（`∂f/∂x = α`）；相关输入形式见 §5.2.2 式(13)。原文经 BIPM 发布版（JCGM_100_2008_E.pdf）逐字核验。
- **合成实验（真值已知，含负例）**：`run/SCI-FIX-SEMANTICS-01/evidence/scale_dimension_results.json` §A/§C。
  `α = 1` 时方差与 ivar **逐位不变**（度量恰为 0）；`α = 2^40` 时逐位精确（rel diff = 0）。
- **真实数据推导**：同一次生产运行内逐帧 `α_k` 实测跨 **5.28×**（`run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`，
  33 帧，`α ∈ [1.1387e-17, 6.0083e-17]`）⇒ `α²` 跨 **27.84×**。由公式「`w_k ∝ 1/σ_k²`，而 `σ_k² = α_k²·σ_adu,k²`」
  推出：标度未声明时归一化权重最大相对偏差应为 `max_k (α_max/α_k)² − 1`；实测 **1.0336**，
  权重效率损失增量 `ΔE = E_未声明 − E_已声明 = 0.2184`（判据 `E = Var_w/Var_opt − 1`，`docs/science/CONTROL_WEIGHT_SNR.md` §8b）；
  逐帧换算后该效应恰为 **0**（`ΔE = 0.0`，逐位）。

**面亮度标度律（强制）**：`S = F/A_cell` ⇒

```text
Var(S) = Var(F) / A_cell²          A_cell = 4π / (12·nside²)   [sr]
```

- **证据**：同一 GUM §5.1.2 式(10)（`∂S/∂F = 1/A_cell`）。像素立体角由像素角尺度派生，而 HiPS 的像素角尺度
  以**度**为单位（IVOA REC-HIPS-1.0 §4.4.1：`hips_pixel_scale` / `s_pixel_scale`「Unit : degrees」）。
- **数值**：`nside = 2^18` ⇒ `A_cell = 1.5239e-11 sr` ⇒ 像素域→面亮度域的方差因子 `1/A_cell² = 4.306e21`（21.63 dex）。
- **负例**：`A_cell = 1`（像素域即面亮度域）⇒ 因子恰为 1，度量归零。

**禁止**：

- 跨标度类别直接比较、合并、加权或做阈值判定；**跨帧 `α_k` 不同时必须逐帧换算到同一标度再聚合**；
- 把与数据无关的绝对常数（如按 ADU² 冻结的地板/下限）直接作用于其它标度的数组。
  地板必须与所作用数组**同标度**，且**在该产品 dtype 中可表示**。
  - **真实数据**（M42 生产噪声平面 `run/M42-VARIANCE-RCA-01/plane_fixed.f64`，sha256 `965e6fe5502202941c9715e00d188f0017ed8a91f6d30b3d1aabc38a4b46010c`）：
    冻结 `variance_floor = 1e-12 ADU²` 按 33 个实测 `α² ∈ [1.2966e-46, 3.6099e-45]` 换算后，
    在 float32 下 **32/33 精确下溢为 0**，其余为次正规数（max `4.204e-45` < float32 最小正规数 `1.1755e-38`）
    ⇒ 该地板在 float32 产品上**不可能产出正值**，`clamp` 分支只能产出 `0`（= §4a 的「显式不可用」态）。
  - **负例**：`α = 1`（ADU 面）⇒ 地板恰为 `1e-12`，float32 可表示、`1/floor` 有限。

**适用域**：本条适用于全部科学 `double`/`float`（含中间块、产品面、诊断面）的量纲/标度声明；
不适用于纯计数/索引/位掩码类整数量（`nContrib`/`nused`/`nrej`/坐标索引），后者按各自合同的 dtype 条款。

## MUST

- **NaN/Inf 契约（样本级掩膜口径，依据 `ASTROCS_DESIGN.md` §5.5）**：
  输入校验返回显式 `INVALID_*` 状态。重采样 / 集成的 NaN 处置**唯一口径** =
  **rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`**（**唯一正本 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`
  §2a 的 `invalid_handling` 块**；科学正本见 `docs/science/DRIZZLE.md`）：
  - **样本级掩膜 + 重归一**：不合格样本（`¬isfinite(x_j)`，**只看值是否有限**）
    从该输出像素的**分子、分母、方差三项中一并剔除**并**重新归一**；
    **禁止**让单个不合格样本使整个输出像素变为 NaN；**禁止**保留被剔除样本的权重在分母里。
  - **分母符号随分支而定（强制）**：`D_p` 与 `W_p` 是**两个量纲不同的量**，不得同名复用、不得互换。
    - **Phase 1 drizzle drop**：分母 = **覆盖球面面积** `D_p = Σ_j a_jp`，量纲 **`sr`**；
      方差传播 `variance_p = Σ_j v_j·w_jp² / D_p²`（`DATA_SEMANTICS` §4a / §31.1a）。
    - **Phase 2 帧间集成 / Phase 3 投影重采样**：分母 = **权重和** `W_p = Σ_{合格} w`，量纲 = `w` 的
      量纲（Phase 3 双线性几何权重无量纲；Phase 2 逆方差权重取 `1/(signal 量纲)²`）；
      方差传播 `Var_p = Σ_{方差可用} V_j·w_j² / W_p²`。
    - **适用域边界**：`W_p` 在 Phase 1 分支**不成立**（那里分母是 `D_p[sr]`，不是权重和）；
      `D_p` 在 Phase 2/3 分支**不成立**（那里分母是权重和，不是球面面积）。代入错误相差
      `A_cell²` —— `nside = 2^18` 时 `1/A_cell² = 4.306e21`，即 **21.63 dex**（数值与推导见本文件
      「面亮度标度律」）。正本 = `DATA_SEMANTICS` §4a 与
      `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a。
  - **方差可用性是独立通道，不参与合格性判定**：`V_j` **有限且 ≤ 0** = 「有覆盖但
    方差不可用」⇒ 信号与几何权重照常计入 `F_p` 与该分支的分母（Phase 1 为 `D_p`、
    Phase 2/3 为 `W_p`）（保信号、保覆盖），不计入
    `Var_p`，产品面写 `variance=0 ∧ ivar=0`（显式不可用，**禁 NaN**）；`V_j` 非有限 =
    方差面损坏（§4a）⇒ 按不合格样本剔除并计入 `n_rejected_nonfinite_variance`。
  - **覆盖级 NaN**：仅当**零合格样本**（该分支分母 `= 0`：Phase 1 为 `D_p = 0`、
    Phase 2/3 为 `W_p = 0`）时输出 `signal = NaN` **且** `support ≤ 0`
    （两者互推）；**NaN 是无效的唯一表示**；**禁止**用 `0`、`±Inf` 或任意哨兵值冒充无效。
  - **强制计数（禁止静默）**：每个输出像素**必须**暴露被剔除样本计数
    **`n_rejected_nonfinite`**（值非有限 / 方差非有限 / 权重非正，分类计数）；
    计数为 0 与「字段缺失」**必须可区分**。
  - **禁止**把 NaN **传播**为合法产品。
    与此相反的 `DISP-DRZ-004`「NaN 经 `F_p` 传播、不掩膜」为 **TRACKED/OPEN** 偏差，
    见 `docs/standards/STANDARDS_REGISTRY.md` D.drizzle 偏差表与 §3 索引。
  - **本文件不复制第二套**：三处（本文件、DATA-002 §2a、STANDARDS_REGISTRY D.drizzle）
    必须逐字同口径；如有分歧以 DATA-002 §2a 的 `invalid_handling` 块为准。
- division by zero：显式守卫或状态。
- overflow：checked 尺寸运算；科学累积用 FP64/stable sums。
- epsilon 必须说明物理/数值来源，禁止裸 `1e-6` 无来源。
- FP32/FP64 boundary：fp32 路径与 fp64 等价性测试。

## 权重/逆方差

- **权重不是被存的东西**：HiPS 里只**存**「**帧级 SNR**」与「**稀疏控制点上的绝对 SNR**」
  （最高设计 §2.1）；**权重 = 阶段二在集成时，按某个天球像素对应的那组输入帧现场算出的
  派生量**，**由 SNR 计算**（`w = 1/σ² = SNR²/F_ref²`；最高设计 §4.3/§4.4）。
- **阶段一、阶段三不产生、也不消费任何权重**（最高设计 §2.1）。
- `ivar` 与 `uncertainty` 是**数据对象**（各有正本定义，见 `docs/contracts/DATA_SEMANTICS.md`），
  **不是**两个可回退的权重来源档位；不存在「权重模式」（`ASTROCS_DESIGN.md` §3.1）。
- 权重必须**正有限**；全 0 / NaN / Inf 权重 → `ZERO_VALID_WEIGHT` / `INVALID_INPUT`。
- **禁止**用 `support` / `coverage` / `validity` / `mask` 冒充权重（四概念分离，最高设计 §4.4）。

## 关联

- docs/science/UNCERTAINTY_AND_COVARIANCE.md；docs/science/DRIZZLE.md；
- docs/standards/CODE_STANDARD.md；docs/standards/STANDARDS_REGISTRY.md。
