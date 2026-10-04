# 数值标准

上游：最高设计的精度归属与「每个科学量写清五件事」一章；数据对象的单位、坐标系与归一化语义
正本在 science 分册的数据语义卷。

每个科学浮点量的文档化要求、量纲与标度类别、线性标度律、面亮度标度律、无效值契约与
权重语义的工程正本。

## 每个科学 double/float 必须文档化

- 单位（ADU / e- / mag / deg / arcsec / 无单位比率）；
- 坐标系（pixel / WCS RA-Dec / HEALPix NESTED / tile+local xy）；
- normalization（除以曝光、中值、median 等）；
- precision requirement（FP32/FP64 边界；默认 science=FP64）；
- valid finite domain。

## 量纲与标度（最高设计 「归一化」一项的细化）

> 依据：`../../ACSD_DESIGN.md` 「每个科学量写清五件事：单位、坐标系、归一化、精度要求、有效有限域」。
> 本条只把其中的**单位（量纲）**与**归一化（标度）**两件事写成可判定形式，不新增要求；下级文档在本条上只能细化。

每个科学量的文档化条目必须**同时**写明量纲与标度类别，缺一即条目不完整[1]：

1. **量纲**：完整单位幂次串，**含立体角幂**（`ADU`、`ADU/sr`、`ADU^2/sr^2`、`sr^2/ADU^2`、`W·m^-2·nm^-1`）。
 单位串的语义权威 = **FITS Standard 4.0 `BUNIT`**[8]：「The value field shall contain a character string
 describing the physical units in which the quantities in the array, **after application of BSCALE and BZERO**,
 are expressed.」单位串写法按该标准指向的 IAU Style Manual（McNally 1988）。
 **立体角幂必须写出**（面亮度方差是 `ADU^2/sr^2`，不是 `ADU^2`）。
2. **标度类别（scale class）**：该量数值所在的线性标度，取值只能来自下表（封闭词表）：

| token | 定义 | 量纲 | 现行承载面（示例） |
|---|---|---|---|
| `raw_adu` | 探测器计数域，物理值 = `BSCALE·样本 + BZERO` | ADU | 输入亮场/母版 |
| `calibrated_adu` | 校准后、测光归一化**前** | ADU | `cal` 面；帧级噪声节点 `p1_op_noise` 的消费面（`lib/infrastructure/scheduler/src/module_adapters.cpp`） |
| `photo_scaled_adu` | 已施加逐帧测光标度 `x′ = α·x`，`α = frame photscal`（**逐帧量，不是全仓常数**） | α × ADU | `photoapplied_<base>`；drizzle `data`/`variance` 块（`lib/infrastructure/scheduler/src/module_adapters.cpp`） |
| `surface_brightness` | 线性量除以像素立体角 `A_cell` [sr] | `<标度量纲>/sr` | HiPS signal/variance/ivar 子产品（`DATA_SEMANTICS` ） |
| `synthetic_flux` | 模型通带积分辐照度 `F_syn` | `W·m^-2·nm^-1` | 测光定标通道 |

**线性标度律（强制）**：量按 `x′ = α·x` 变换时

```text
Var(x′) = α²·Var(x) ivar(x′) = ivar(x) / α²
```

- **证据（一手）**：JCGM 100:2008（GUM）式(10) `u_c²(y) = Σ_i (∂f/∂x_i)²·u²(x_i)` 在 `y = α·x` 上的
 线性特例（`∂f/∂x = α`）；相关输入形式见 式(13)。原文经 BIPM 发布版（JCGM_100_2008_E.pdf）逐字核验[6]。
- **合成实验（真值已知，含负例）**：`run/SCI-FIX-SEMANTICS-01/evidence/scale_dimension_results.json` 的线性标度段与负例段。
 `α = 1` 时方差与 ivar **逐位不变**（度量恰为 0）；`α = 2^40` 时逐位精确（rel diff = 0）。
- **标度未声明的权重后果（结构结论，可由公式自证）**：`w_k ∝ 1/σ_k²`，而 `σ_k² = α_k²·σ_adu,k²`
 ⇒ 未声明标度时第 `k` 帧权重相对声明后的权重整体多乘 `α_k²`，帧间权重比失真因子为 `(α_max/α_k)²`。
 `α_max/α_k` 在 `α` 取最小值的帧处取极大，故

 ```text
 max_k (α_max/α_k)² − 1 ≡ (α_max/α_min)² − 1
 ```

 即**最不利帧对的权重比相对偏差上界**，其中 `α_min`、`α_max` 是同一次运行内逐帧标度的极值。
 该量是**逐次运行的实测量**：`α` 的极值跨度与据此的权重效率损失增量
 `ΔE = E_未声明 − E_已声明`（判据 `E = Var_w/Var_opt − 1`，`docs/science/noise_snr/NOISE_SNR.md`）
 都由该次运行的逐帧 `α` 读数给出，**本标准不为它们冻结任何数值**；把某一次运行的 `α` 极值跨度
 写成全仓常数或阈值断言即违反本条。
- **换算的精确性（结构结论，逐位）**：逐帧换算是精确代数折合——换算后 `w_k = 1/(α_k²·v_k)`
 逐位等于声明标度下的权重，故 `Var_w` 逐位相同、`E` 逐位相同、`ΔE ≡ 0`。该结论只依赖换算的
 代数精确性，与任何读数无关。

**面亮度标度律（强制）**：`S = F/A_cell` ⇒

```text
Var(S) = Var(F) / A_cell² A_cell = 4π / (12·nside²) [sr]
```

- **证据**：同一 GUM 式(10)（`∂S/∂F = 1/A_cell`）[6]。像素立体角由像素角尺度派生，而 HiPS 的像素角尺度
 以**度**为单位[7]（IVOA REC-HIPS-1.0 的 `hips_pixel_scale` / `s_pixel_scale` 记「Unit : degrees」）。
- **因子随 `nside` 变化**：`1/A_cell² = (12·nside²/4π)² = 144·nside⁴/(16π²)`，
 以 `dex` 记为 `log₁₀(1/A_cell²) = 8·log₁₀(nside) − 2·log₁₀(4π/12)`，即每翻一倍 `nside` 因子涨 4 倍、涨 8 dex。
 `nside` 是运行参数，不是常数，故本标准只给该表达式；下表是**示例取值**，取 `nside = 2^18`：

 | `nside` | `A_cell` [sr] | `1/A_cell²` | dex |
 |---|---|---|---|
 | 2¹⁸ = 262144 | 1.5239e-11 | 4.306e21 | 21.63 |
 | 2¹⁶ = 65536 | 2.4382e-10 | 1.682e19 | 19.23 |

 两行之差恰为 2 个 `nside` 位 ⇒ 8 dex，与上式的斜率一致；引用任一行都必须同时给出 `nside`。
- **负例**：`A_cell = 1`（像素域即面亮度域）⇒ 因子恰为 1，度量归零。

**违规判据**：

- 跨标度类别直接比较、合并、加权或做阈值判定；**跨帧 `α_k` 不同时必须逐帧换算到同一标度再聚合**；
- 把与数据无关的绝对常数（如按 ADU² 冻结的地板/下限）直接作用于其它标度的数组。
 地板必须与所作用数组**同标度**，且**在该产品 dtype 中可表示**。
 - **float32 下的可表示性（结构结论，可由常量自证）**：`variance_floor` 是按 `raw_adu` 冻结的绝对常数
 （`variance_floor = 1e-12 ADU²`，单一事实源 `eng/packaging/config/defaults.json` 的 `noise.variance_floor`，
 来源 = `docs/science/noise_snr/NOISE_SNR.md`）。换算到帧标度 `α` 面得

 ```text
 floor(α) = variance_floor · α²
 下溢为 0 当且仅当 floor(α) < 2⁻¹⁵⁰（就近舍入的半 ULP 界；float32 最小次正规数
  2⁻¹⁴⁹ = 1.401298464324817e-45，其一半 2⁻¹⁵⁰ = 7.006492321624085e-46）[9]
 次正规带 = [2⁻¹⁵⁰, 2⁻¹²⁶) ，2⁻¹²⁶ = 1.1754943508222875e-38（float32 最小正规数）
 零点阈值   α₀ = sqrt(2⁻¹⁵⁰ / variance_floor) = sqrt(7.006492321624085e-34) = 2.6469779601696886e-17
 次正规起点 α₁ = sqrt(2⁻¹⁴⁹ / variance_floor) = sqrt(1.401298464324817e-33) = 3.7433921305746435e-17
 正规起点   α₂ = sqrt(2⁻¹²⁶ / variance_floor) = 1.0842021724855044e-13
 ivar 有限阈值 α₃ = sqrt(1 / (variance_floor · f32max)) = 5.4210110239862425e-14
 ```

 判红线是 `α₀` 而**不是** `α₁`：float32 就近舍入（round-to-nearest-even）下
 `2⁻¹⁵⁰ ≤ floor(α) < 2⁻¹⁴⁹` 仍舍入为最小次正规数 `2⁻¹⁴⁹`（非零），只有 `floor(α) < 2⁻¹⁵⁰`
 才舍入为 `0`。故 `α < α₀` 时该地板在 float32 平面内**恒为 `0`**（`clamp` 分支只能产出 `0`
 =「显式不可用」态）；`α₀ ≤ α < α₂` 时地板虽为非零次正规数、有效位已失真，且只要 `α < α₃`
 其倒数 `1/floor(α)` 放回 float32 **必然溢出为 `+Inf`** —— 故 `α < α₃` 时该地板在 float32
 平面内始终给不出「有限方差 + 有限 ivar」这一对可用的产品值；只有 `α ≥ α₂` 才同时满足两侧。
 该判据是**逐帧条件式**的：一次运行是否命中，取决于该次运行的逐帧 `α` 与 `α₀`、`α₃` 的大小关系，
 必须由该次运行的逐帧读数判定，不得由本标准预判。
 - **产品面**（M42 生产噪声平面 `run/M42-VARIANCE-RCA-01/plane_fixed.f64`，sha256 `965e6fe5502202941c9715e00d188f0017ed8a91f6d30b3d1aabc38a4b46010c`；
 同次运行的帧标度读数 `run/M42-VARIANCE-RCA-01/meta.json` 的 `photscal = 2.3846837130250378e-17`）：
 该平面是 `photo_scaled_adu` 面，其标度取该次运行的 `α`，不是 `α = 1`。判据：若取 `α = 1`，
 平面中位 `2.1457208312226298e-29` 对应 `σ = 4.63e-15 ADU`，比任何探测器的读出噪声低 15 个数量级，
 不可能成立；按 `α = 2.3846837130250378e-17` 换算则对应 `σ = 194.2 ADU`，与该次运行自身记录的
 `background_adu = 194.0560302734375` 同量级。在该 `α` 下
 `floor(α) = 1e-12 × 5.686716411166881e-34 = 5.686716411166881e-46 < 2⁻¹⁵⁰`
 ⇒ float32 下**恰为 `0`**（`α = 2.3847e-17 < α₀ = 2.6470e-17`）；该平面上 `≤ 0` 的像素占 30.09%，
 与同次运行根因报告记录的「平面负预测占比 30.09%」逐位吻合。
 - **负例**：`α = 1`（ADU 面）⇒ 地板恰为 `1e-12`，落在正规带内，float32 可表示、`1/floor` 有限。
 与上条互为对照：地板是否可表示完全由 `α` 决定，`α = 1` 是唯一使该地板在 float32 内保持正规数的情形。

**适用域**：本条适用于全部科学 `double`/`float`（含中间块、产品面、诊断面）的量纲/标度声明；
不适用于纯计数/索引/位掩码类整数量（`nContrib`/`nused`/`nrej`/坐标索引），后者按各自合同的 dtype 条款。

## MUST

- **NaN/Inf 契约（样本级掩膜口径，依据 `../../ACSD_DESIGN.md` ）**：
 输入校验返回显式 `INVALID_*` 状态。重采样 / 集成的 NaN 处置**唯一口径** =
 **rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`**（**唯一正本 = `../data/PHASE_PRODUCT_EXCHANGE.md`
 a 的 `invalid_handling` 块**；科学正本见 `docs/science/drizzle/DRIZZLE.md`）：
 - **样本级掩膜 + 重归一**：不合格样本（`¬isfinite(x_j)`，**只看值是否有限**）
 从该输出像素的**分子、分母、方差三项中一并剔除**并**重新归一**；
 不合格样本逐样本剔除：单个不合格样本的效应限于自身，输出像素保持可计算；被剔除样本的权重同时从分母移除。
 - **分母符号随分支而定（强制）**：`D_p` 与 `W_p` 是**两个量纲不同的量**，各自具名、不可互换。
 - **Phase 1 drizzle drop**：分母 = **覆盖球面面积** `D_p = Σ_j a_jp`，量纲 **`sr`**；
 方差传播 `variance_p = Σ_j v_j·w_jp² / D_p²`[5]。
 - **Phase 2 帧间集成 / Phase 3 投影重采样**：分母 = **权重和** `W_p = Σ_{合格} w`，量纲 = `w` 的
 量纲（Phase 3 双线性几何权重无量纲；Phase 2 逆方差权重取 `1/(signal 量纲)²`）；
 方差传播 `Var_p = Σ_{方差可用} V_j·w_j² / W_p²`。
 - **适用域边界**：`W_p` 在 Phase 1 分支**不成立**（那里分母是 `D_p[sr]`，不是权重和）；
 `D_p` 在 Phase 2/3 分支**不成立**（那里分母是权重和，不是球面面积）。代入错误相差
 `A_cell²` —— 相差 `1/A_cell²`，按上式的 `dex` 斜率是 `8·log₁₀(nside)` 数量级
 （`nside = 2^18` 时 21.63 dex、`nside = 2^16` 时 19.23 dex；推导与示例见本文件
 「面亮度标度律」）。正本 = `DATA_SEMANTICS` 与
 `../data/PHASE_PRODUCT_EXCHANGE.md`。
 - **方差可用性是独立通道，不参与合格性判定**：`V_j` **有限且 ≤ 0** = 「有覆盖但
 方差不可用」⇒ 信号与几何权重照常计入 `F_p` 与该分支的分母（Phase 1 为 `D_p`、
 Phase 2/3 为 `W_p`）（保信号、保覆盖），不计入
 `Var_p`，产品面写 `variance=0 ∧ ivar=0`（显式不可用，**禁 NaN**）；`V_j` 非有限 =
 方差面损坏 ⇒ 按不合格样本剔除并计入 `n_rejected_nonfinite_variance`。
 - **覆盖级 NaN**：仅当**零合格样本**（该分支分母 `= 0`：Phase 1 为 `D_p = 0`、
 Phase 2/3 为 `W_p = 0`）时输出 `signal = NaN` **且** `support ≤ 0`
 （两者互推）；**无效的唯一表示 = NaN**（`0`、`±Inf` 或任意哨兵值都不构成无效表示）。
 - **强制计数（显式可见）**：每个输出像素**必须**暴露被剔除样本计数
 **`n_rejected_nonfinite`**（值非有限 / 方差非有限 / 权重非正，分类计数）；
 计数为 0 与「字段缺失」**必须可区分**。
 - NaN 只作无效标记出现，合法产品只含有效值。
 与此相反的 `DISP-DRZ-004`「NaN 经 `F_p` 传播、不掩膜」状态为 **CLOSED**（唯一口径 =
 rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`：样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数）[4]，
 见 `../../science/algorithms/DRIZZLE_GEOMETRY.md` 的 drizzle 偏差表（`DISP-DRZ-004` 行）。
 - **本文件不复制第二套**：本文件、`../data/PHASE_PRODUCT_EXCHANGE.md` 的 `invalid_handling` 块
 与 `../governance/TRACEABILITY.md` 的 `drizzle` 登记项必须逐字同口径；如有分歧以
 `../data/PHASE_PRODUCT_EXCHANGE.md` 的 `invalid_handling` 块为准[2]。
 - **通用非有限值的比较语义**（非有限值与缺失的位置与语义如何比较）唯一正本 =
 `../testing/TEST.md` 的 NaN 与 Inf 语义一节[3]；本条只规定产品面上非有限值的**产生与分布**口径，
 两者不是同一件事，不得互相替代。
- division by zero：显式守卫或状态。
- overflow：checked 尺寸运算；科学累积用 FP64/stable sums。
- epsilon 必须说明物理/数值来源；裸 `1e-6` 视为无来源。
- FP32/FP64 boundary：fp32 路径与 fp64 等价性测试。

## 权重/逆方差

- **权重不是被存的东西**：HiPS 里只**存**「**帧级 SNR**」与「**稀疏控制点上的绝对 SNR**」[1]；
 **权重 = 阶段二在集成时，按某个天球像素对应的那组输入帧现场算出的
 派生量**，**由 SNR 计算**（`w = 1/σ² = SNR²/F_ref²`）[1]。
- **阶段一、阶段三不产生、也不消费任何权重**[1]。
- `ivar` 与 `uncertainty` 是**数据对象**（各有正本定义，见 `docs/science/unified/DATA_SEMANTICS`），
 **不是**两个可回退的权重来源档位；不存在「权重模式」[1]。
- 权重必须**正有限**；全 0 / NaN / Inf 权重 → `ZERO_VALID_WEIGHT` / `INVALID_INPUT`。
- 权重取值为正有限值；`support` / `coverage` / `validity` / `mask` 与权重各自独立（四概念分离）[1]。

## 关联

- docs/science/noise_snr/NOISE_SNR.md；docs/science/drizzle/DRIZZLE.md；
- CODE.md。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计，第 2.2 节（跨帧绝对信噪比）与第 3.1 节（数据对象），上位来源。
[2] 内部文档 `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md`，阶段间产品交换合同，`invalid_handling` 块为
 重采样与集成的 NaN 处置唯一正本。
[3] 内部文档 `docs/engineering/testing/TEST.md`，测试标准与测试矩阵，通用容差规则与 NaN 与 Inf 语义为唯一正本。
[4] 内部文档 `docs/science/algorithms/DRIZZLE_GEOMETRY.md`，守恒映射算子的几何口径，drizzle 偏差表登记
 `DISP-DRZ-004`。
[5] 内部文档 `docs/science/unified/DATA_SEMANTICS.md`，统一数据语义卷，方差传播与分母语义的正本。
[6] JCGM 100:2008 (GUM), Evaluation of measurement data — Guide to the expression of uncertainty in
 measurement, BIPM, 2008. 式(10) 与 式(13)，发布版 JCGM_100_2008_E.pdf。
[7] IVOA Recommendation REC-HIPS-1.0, HiPS — Hierarchical Progressive Protocol for Sky Hierarchical
 Interleaving of Sources. `hips_pixel_scale` / `s_pixel_scale` 的单位定义为度。
[8] FITS Working Group. FITS Standard 4.0, `BUNIT` 关键字定义。单位串写法依该标准指向的
 IAU Style Manual, McNally 1988, Space Science Series, Reidel.
[9] IEEE. IEEE Std 754-2019, IEEE Standard for Floating-Point Arithmetic. IEEE, 2019.
 二进制浮点的最小正规数、最小次正规数与 unit roundoff 取值依此标准。
