# Photometric Fit Algorithms (ALG-PHOT)

本页补充记录光度拟合算法步骤、参数和源码锚。光谱、通带与通量定义见[光度科学分册](../../photometry/PHOTOMETRY.md)及[Normalize科学正本](../../normalize/NORMALIZE_SCIENCE.md)；阶段一字段和生产—消费关系见[Normalize数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。源码锚或测试设计本身不表示验证已经运行。


> 上游：ACSD_DESIGN.md [D-1]「P1 通量积分拟合：测光校准到测光星等坐标系」一节（创新点一）、[D-1]「输出合同」一节（输出合同）

## 1 上游 SCI 与输入输出

- 上游: `SCI-PHOT-001` (r=log10(F_instr/F_syn) IRLS Tukey c=4.685, mag_tolerance=3.0)
- 输入: 仪器流量 `F_instr` + 合成流量 `F_syn` (Gaia XP)
- 输出: `PhotometricCalibrationQuality` (sigma_mag, sigma_cal_rel) + scale（无 zero_point 字段：无定义式、结构体无字段）

## 2 离散公式

```text
F1: r_i = log10(F_instr,i / F_syn,i)
F2: S = MAD(r)/0.6744897501960817, init location=median(r)
F3: IRLS Tukey: u=(r−location)/(c·S), c=4.685, w=(1−u²)² if |u|<1 else 0, location=Σw·r/Σw, iter≤50 tol=1e-6
F4: sigma_residual = median_i(|r_i − location|)/0.6744897501960817, r_inliers={i | |u_i|<1}
F5: sigma_mag = 2.5·sigma_residual, sigma_cal_rel = ln10·sigma_residual
F6: scale = 10^{−location}
F7: 星等一致性预过滤 |Δ−median(Δ)|>3.0 mag reject where Δ=−2.5·log10(F_instr)−G_Gaia
```

来源: `lib/algorithms/photometry/cpp/src/star_matcher.cpp`；`lib/algorithms/photometry/cpp/src/image_corrector.cpp`

**逐式的量纲、中心约定、适用域与证据**

- **F1 量纲**：`F_instr` 单位 **ADU**；`F_syn` 单位 **W·m⁻²·nm**（定义式与逐项量纲见 `docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节）；`r_i` 是**有量纲比值**的对数，单位 `dex(ADU/[F_syn 单位])`；`location` 同单位；`scale` 单位 `[F_syn 单位]/ADU`。**有效域**：`F_instr>0`、`F_syn>0`、`r` 有限（`star_matcher.cpp`）；不成立者显式拒绝，不进入 `r`。
- **F2 中心约定**：`MAD` 的中心是**中位数**（`star_matcher.cpp`：先 `location=median(r_consistent)`，再取 `MAD=median(|r−location|)`）。常数 `0.6744897501960817 = Φ⁻¹(3/4)` 的推导前提即「中心 = 中位数」。
  **有限样本偏差（适用域）**：`MAD/0.6744897501960817` 对 σ 的期望在 n<20 时系统性偏低；修正因子 `b_n`（Croux & Rousseeuw 1992, *Computational Statistics* **1**, 411, p.414 表：n=3→1.495、4→1.363、5→1.206、9→1.107，n>9→`n/(n−0.8)`）。合成实验独立复算该表的期望并与文献预言一致（读数正本 = `实验/photometric-magnitude/code/redo/route2/REPORT_route2.md` [S-1]实验读数）。本层冻结门 `|r_consistent|≥3` 恰落在偏差最大处：**n=3 时 S 与 sigma_residual 的期望偏低约 1.49 倍**、n=9 偏低 1.107 倍、n≥50 偏差 <2%；**该偏差在实现中不修正**，故低星数帧的 `sigma_mag` 是下偏估计。证据与读数正本 = `实验/photometric-magnitude/code/redo/route2/REPORT_route2.md` [S-1]实验读数。
- **F3 尺度路线**：`S` 由 `MAD` **一次估计后在迭代中固定**（`star_matcher.cpp`），属「先验尺度估计的 M 估计」路线（Huber & Ronchetti 2009, *Robust Statistics* 2nd ed. §6.5；与 §6.4「位置-尺度同时迭代」并列）。
  **收益**：`location` 继承 `MAD` 的 50% 崩溃点（MASS 7.3-66 `R/rlm.R:171-183` 的 MM 法说明：「The final estimator is an M-estimator with Tukey's biweight and **fixed scale** that will inherit this breakdown point provided c > k0」）。
  **代价**：`c=4.685` 的 95% 渐近效率只对**已知尺度**严格成立（statsmodels `robust/_tables.py`：`tukeybiweight_eff[0.95]=(4.685065, 0.119414)`；闭式复算 `ARE(4.685065)=0.950000`）。本层合成实测 93.7%（n=1000）–96.0%（n=50），与名义值同量级。证据与读数正本 = `实验/photometric-magnitude/code/redo/route1/REPORT_route1.md` [S-1]实验读数。
  **成对变更约束**：若采用**每轮重估尺度**方案（MASS `rlm()` `R/rlm.R:171-183`、statsmodels `RLM(update_scale=True)` 的默认路线），`c=4.685` 下的崩溃点降到 **11.9%**；要保 50% 崩溃点必须同时把 `c` 降到 1.548（效率仅 28.7%）。⇒ `c` 与「尺度是否重估」**必须成对变更**，单独改任一项即破坏另一侧保证。
- **F4 中心约定与语义**：实现的 `MAD` 中心是 **IRLS 收敛后的 `location`**，不是 `median(r_inliers)`（`star_matcher.cpp`）。二者相差 `δ=|location−median(r_inliers)|` 时，本式相对标准 MAD 的一阶偏差为 `δ/σ`：合成实验核实该一阶偏差（纯高斯与含污染两档；读数正本 = `实验/photometric-magnitude/README.md` [S-1]「参数与常数」一节 纯解析合成的实现正确性与约定收敛表）。**适用域**：`δ ≪ σ`（帧内残差近似单峰对称）时与标准 MAD 等价；`δ ≳ 0.1σ` 时必须声明所用中心。证据与读数正本 = `实验/photometric-magnitude/README.md` [S-1]「参数与常数」一节。
  **语义**：`sigma_residual` 是**逐星定标散度**（系综量，dex），不是单源通量误差；与 SExtractor 的 `FLUXERR/MAGERR` **同名不同义**，两者各自独立、不可互换。SExtractor 定义见其官方文档 `Param.html` 的「Flux and magnitude uncertainties」**式 (2)**（`MAGERR = 2.5/ln10·FLUXERR/FLUX`）与 `Photom.html` 的 `FLUXERR` 式（`FLUXERR = sqrt(Σ(σ_i²+p_i/g_i))`），原文注明该误差「provides a lower limit of the true uncertainty, as it only takes into account photon and detector noise」——即**逐源**量，与本层系综散度不同。
- **F5 语义与覆盖边界**：`sigma_mag`、`sigma_cal_rel` 是帧级定标散度的线性换算，**不含**参考端（XP 谱解码、Gaia 星表、通带曲线）误差与逐源测量误差；作为单源精度指标使用会低估（误差预算逐项见 `docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节）。
- **F6 量纲与施加面**：`scale` 单位 `[F_syn 单位]/ADU`，其绝对量级由该帧仪器标度（增益/口径/曝光等不可测量）决定，**无物理意义**（`docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节）；施加式 `I_cal=I·scale` 逐元素（`image_corrector.cpp`，OpenMP `schedule(static)`）。
- **F7 适用域**：窗口作用于 `Δ−median(Δ)`，故对 `F_instr` 的**整体乘性标度严格不变**（合成实验：`k=10⁻⁴…10⁶` 下拒绝集逐位相同）；对**与星相关的颜色项不不变**。窗口 = **7.29σ（≈10.81×MAD）** ⇒ 需颜色项散度 ≈3.0 mag 才首次触发（斜率 4.0 mag/颜色单位），故该窗在本仓真实数据上不承担「防通带失配」职能（通带取错的跨星散度读数正本 = `实验/photometric-magnitude/README.md` [S-1]「参数与常数」一节 非退化负例表 N3 行与 `实验/photometric-magnitude/docs/fsyn_convention.md` [S-3]实验读数）；对 6 mag 级粗大离群，其相对 IRLS 的边际贡献不可测（污染 0–49% 时 location RMSE 差 <2%）。`3.0` 为 Project-defined 冻结值。证据与读数正本 = `实验/photometric-magnitude/code/redo/route1/REPORT_route1.md` [S-1]实验读数。

## 3 伪代码

```text
function photometric_fit(F_instr, F_syn, G_Gaia):
  if n < min_ref → NO_DATA
  Δ_i = −2.5·log10(F_instr)−G_Gaia; median_Δ = median(Δ)
  r_consistent = {i | |Δ_i−median_Δ|≤3.0} → r_i=log10(F_instr/F_syn)
  if S=MAD(r)/0.6744897501960817 ==0 → location=median(r) skip IRLS
  else:
    location=median(r); repeat 50×:
      w_i=(1−((r_i−location)/(c·S))²)² if |u|<1 else 0; c=4.685
      new_loc=Σw·r/Σw; if |new−old|<1e-6 break
  sigma_res=MAD({r|w>0})/0.6744897501960817; scale=10^{−location}
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| `F≤0` / log10 非有限 | skip REJECT |
| `MAD==0`（S=0） | 跳过 IRLS 取 `location=median(r_consistent)`；`robust_iterations=0` |
| `S<0` / n<min | NO_DATA |
| `\|r_consistent\|<3` | NO_DATA 拟合失败：`scale=1.0`、`fit_used=0`、`sigma_residual=0`，不写标度 |
| `\|r_inliers\|<2` | `sigma_residual=0`（不可估计） |
| `S=0` 且 `\|r_consistent\|≥3` | 可达：n=3 时两颗星 `r` 逐位相等即 `MAD=0`（合成实验实测）；此时 `location=median(r)`、`scale` 照常发布、`sigma_residual=0` |

**`sigma_residual=0` 的双义与下游消歧（正向约束）**：`0` 同时表示「不可估计（`|r_inliers|<2`）」与「实测零散度（S=0 分支）」。下游 `snr_phot_cal_quality`（`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`）把 `sigma≤0` 映射为 `fit_status=2`（未估计）并**不发布** `sigma_mag/sigma_cal_rel`，故两种含义在 SNR 面被消歧为「无不确定度可用」；**任何其他消费方**必须按同一规则处理，`0` 的解释 = 「无不确定度可用」；「零不确定度」是另一含义。
**S=0 分支的可达性判据**：n=3 两值相等 ⇒ S=0；n=3 三值互异 ⇒ S>0（负例对照）；证据与读数正本 = `实验/photometric-magnitude/README.md` [S-1]「参数与常数」一节「`S=0` 退化」行与 `实验/photometric-magnitude/code/redo/route3/REPORT_route3.md`实验读数（H5c 构型）。

**负例与归零分支 N05（T05–T07 负向轮，PHOTOMETRIC_FIT 未收敛状态位）**：负例输入构造 =
IRLS 达最大迭代仍未收敛（或 `|r_consistent| < 3` 拟合失败）。预期行为 = `scale = 1.0`、
`fit_used = 0`、`sigma_residual = 0`，不写标度并置未收敛状态位 / `fit_status = 2`
（未估计），不发布 `sigma_mag/sigma_cal_rel`。落盘标记 = `p1_phot.json.frames[]`
`fit_used/sigma_residual` + 状态位；把未收敛写成数值标度继续用 ⇒ 判红；

## 5 确定性与归约

- 排序 median/MAD 确定性；IRLS 按 r 索引固定顺序加权和，无跨样本归约。

### 5.1 顺序、确定性与并列匹配

样本枚举顺序会影响浮点归约与匹配实现。只有输入、浮点环境、数据类型、运算顺序和归约顺序均固定时，才以逐位相同描述重复执行；改变样本序时须按声明的数值容差比较，不能由代数等价推出逐位不变。

| 到达序来源 | 当前实现事实 | 可作出的数值陈述 |
|---|---|---|
| **PSF 星表序**（`psf_cx/cy/flux/status` 的数组序） | `matchWithKdTree` 按 PSF 下标正向查询；`cleanAndScale` 按匹配结果顺序累积 | 同一顺序和固定数值环境下可复现；重排会改变累加顺序的，结果差异按[数值标准](../../../engineering/standards/NUMERIC.md)中的误差预算评估，本文不声称普遍小于 1 ulp 或位级不变 |
| **Gaia 参考星表序**（`gaia_ra/dec/mag/fsyn` 的数组序） | 参与 KD-tree 构建及候选枚举 | 仅在匹配关系唯一且数值路径及归约序保持相同的条件下讨论等价；当前实现没有对精确等距匹配定义稳定 tie-break，故不保证任意重排下全输出位级不变 |

匹配谓词在无并列时定义最近邻关系；后续归约仍可能受枚举和累加顺序影响，因此顺序变化须结合实际实现与数值标准解释。

**并列（exact tie）是合法输入情形，但当前实现的选择未定义**：当两颗 Gaia 星到同一 PSF 星的距离**精确相等**时，
最近邻选择退化为「KD-tree 遍历序先到者胜」（`findNearestRec` 的 `dist2 < best_dist2` 严格比较），
而遍历序由建树时的 `std::sort` 结果决定 ⇒ **该情形下匹配伙伴随输入序变化**。
该输入不应作为调用方前置条件排除。匹配需有确定的 tie-break；当前实现未定义，作为实现差异登记于 §13.3。测试应覆盖此类并列输入。

**可执行判据**（确定性组）—— 线程数扫描 1/2/4/8（I5 的显式补全）+ 两类到达序置换
+ 两个序相关负例（N1 并列 tie-break / N2 在线归约），正负例共用同一 `verdict` 函数。
**注意与 §13.4 I5 的区别**：I5 只扫线程数、每个线程跑**完全相同的样本序**，因此**抓不到**顺序依赖；
两者互补，不可互相替代。

### 5.2 模型通带身份与逐帧装配

拟合的参考通量 `F_syn` 由 `T(λ)·Q(λ)·λ` 在参考星谱上合成（`docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节）。`T(λ)` 的身份是**装配输入**，不是拟合内部的自由量，故本算法层的契约是：

| 项 | 约束 | 判据 |
|---|---|---|
| 曲线身份 | 将实际用于该帧的滤镜与 QE 曲线身份绑定到响应记录；曲线身份不能由首帧标签推断 | 身份不可解析时如实记录响应不可用/未知；不得以“不同于首帧”作为观测准入拒绝 |
| 逐帧响应 | 科学合同按输入 occurrence 绑定 frame override，未覆盖项依明确配置优先级继承 block 设置 | 具有不同 (R_k) 的帧分别拟合/应用各自响应；不得把 block 首帧响应强加给它们 |
| 当前实现载体 | `module_adapters.cpp` 当前按输入路径、basename、frame_key 顺序解析首个匹配 override | 这是源码实现事实，不是完整 occurrence 身份合同；重复路径 occurrence 的严格区分仍按 Normalize 数据合同处理 |
| 当前配置限制 | `module_adapters.cpp` 在逐帧绑定前仍要求 block-level filter 与 qe_name 非空 | 完整的逐帧 override 不能替代缺失的 block 字段；这是实现差异，不是科学准入规则 |
| 身份自述 | 拟合/应用结果保留所用曲线身份与 `application_id`，并与真实 photo-applied plane 配对 | 仅有曲线标签或拟合成功不证明图像面已施加标度 |

不同响应下的测量结果须按各帧 (R_k) 解释；是否共享计算可按响应身份决定，但组内相同曲线不是 Normalize 的观测准入条件。光度科学定义与用户对合并帧集的选择见[光度科学分册](../../photometry/PHOTOMETRY.md)和[Normalize科学正本](../../normalize/NORMALIZE_SCIENCE.md)。

**可执行判据**（装配入口层与解析层 `[I0..I9]`）：两者都含恒真自检（正例与错误输入的判定必须互不相同）。

## 6 复杂度

- O(n_ref log n_ref) 排序 + O(n_ref·iter) IRLS

## 7 CPU-only 后端策略（V5）

- 仅 CPU：IRLS 迭代为全样本顺序归约(样本序固定)，天然确定性；规模小(星数级)无并行收益，worker pool 可行但非必需；**线程数取自 benchmark profile**。

## 5c SIMD 安全与取消点

- `r_i=log10(F_i/F_syn,i)` 逐星独立(SIMD 安全: 数组连续无别名)；IRLS 加权均值 `Σw·r/Σw` 为 FP64 固定序归约。逐位复现要求浮点环境、数据类型、运算与归约顺序固定；改变顺序的差异按[数值标准](../../../engineering/standards/NUMERIC.md)和对应误差预算判断，不保证仅小于 1 ulp。
- 取消点: IRLS 迭代间检查; 取消时返回未收敛状态不写 location/scale。

## 8 参考实现/Oracle

- 合成注入偏移恢复 PHOT-001..007 (scale 已知→location=log10 k)

## 9 容差来源

- location tol `1e-6`（IRLS 收敛判据 `_IRLS_CONVERGE`；定义与使用均在 `star_matcher.cpp`），预冻结。
  **适用域**：该容差是**迭代停机**阈值（相邻两次 `location` 之差），不是产品精度指标；产品精度由 `scale` 的相对误差承载。判据取绝对值而非相对值，故对 `location` 的量级不敏感（`location` 单位 dex、量级 ~10–17，绝对 1e-6 相当于相对 ~1e-7）。
  **未收敛时的行为**：达 `_IRLS_MAX_ITER=50` 仍未收敛时按最后一次迭代值发布 `location`/`scale`（`star_matcher.cpp` 无未收敛标记），`robust_iterations` 记录实际迭代数（=50 即触顶）；消费方须以 `robust_iterations` 判定是否触顶。

## 10 关联 ARC/API/TST

- API: `pc_api.h: pc_calibrate_simple, pc_calibrate_simple_with_gaia`
- TST: `TST-PHOT-*` 合成注入/鲁棒

## 13 实现锚定与测试设计补充

本节补充当前可定位的源码锚、已登记实现差异和候选测试设计。它们不是新的科学定义，也不因编号或设计标签表示测试已执行。锚点给到
> 文件级（`lib/algorithms/photometry/cpp/` 下的符号名），后续重构以
> grep 重锚为准。这里的实现锚与历史测量需分别按其调用路径和输入载体解释；不以旧任务编号或状态词代替当前生产证据。

### 13.1 逐符号实现锚定

**ALG-PHOT-001 IRLS-Tukey 稳健零点估计**（lib/algorithms/photometry/cpp/src/star_matcher.cpp）:

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| 常量 | _MAD_SCALE=0.6744897501960817 / _TUKEY_C=4.685 / _IRLS_MAX_ITER=50 / _IRLS_CONVERGE=1e-6 | `star_matcher.cpp` |
| 质量/饱和位过滤 | `PC_QF_SATURATED` 不入定标 | `star_matcher.cpp` |
| 有效残差 r_i=log10(F_instr/F_syn) | cleanAndScale 内 r 计算 | `star_matcher.cpp` |
| 星等预过滤 | delta−median_delta 阈值 mag_tolerance | `star_matcher.cpp` |
| 参考星数门 | `|r_consistent|<3` → NO_DATA，scale=1.0/fit_used=0 | `star_matcher.cpp` |
| IRLS 初值 | location=median(r), S=MAD/0.6744897501960817; S=0→median 兜底 | `star_matcher.cpp` |
| IRLS 迭代 | Tukey w=(1−u²)², ≤50 iter, 收敛 1e-6 | `star_matcher.cpp` |
| scale | 10^(−location) | `star_matcher.cpp` |
| inliers/sigma_residual | `|u|<1`; median(|r−location|)/0.6744897501960817 | `star_matcher.cpp` |
| diag 填充 | r_median/p90/max, rejected_quality | `star_matcher.cpp` |
| 一站式入口 | matchAndClean(2.0,3.0) | `star_matcher.cpp` |

**ALG-PHOT-002 星等一致性匹配（双向最近邻唯一配对）**（star_matcher.cpp）:

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| 常量/ctor | StarMatcher 构造 | `star_matcher.cpp` |
| Gaia 投影入帧 | matchWithKdTree 内投影与 in-frame 统计 | `star_matcher.cpp` |
| Gaia KD-tree | KdTree2D::build | `star_matcher.cpp` |
| PSF 有效过滤 | status==0 | `star_matcher.cpp` |
| PSF KD-tree | KdTree2D::build | `star_matcher.cpp` |
| 正向最近邻 | PSF→Gaia | `star_matcher.cpp` |
| 反向最近邻 | Gaia→PSF | `star_matcher.cpp` |
| 唯一配对 | 互为最近邻过滤 + match_radius=2.0 | `star_matcher.cpp` |
| 分阶段 diag | spatial_candidates/unique_matches/rejected_* | `star_matcher.cpp` |

**F_syn 合成测光**（spectrum_integrator.cpp; 生产路径=XPSD 官方解码）:

定义式（权威：`docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节，claim `PHOT-FSYN-CANON-001`）：

```text
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ        # W·m⁻²·nm；F_λ 单位 W·m⁻²·nm⁻¹；λ、dλ 单位 nm
```

**不含** `10^(−0.4·G)`；`G`（Gaia G 星等）不进入 `F_syn`。与官方定义（Gaia DR3 文档 §5.4.1 式 5.41：`⟨f_λ⟩ = ∫f_λ S λ dλ / ∫S λ dλ`）只差与星无关的归一化分母 `∫TQλdλ`，该分母被 `location`/天区合成颜色项 `synthetic_colour_median_mag` 吸收。

| 步骤 | 符号/位置 | 锚 |
|---|---|---|
| Akima 子样条 | akima_interpolate（fill=0） | `spectrum_integrator.cpp` |
| Simpson 1/3 复合（尾 3/8） | simpson_integrate | `spectrum_integrator.cpp` |
| **生产符号（XPSD 绝对口径）** | `compute_f_syn_cached_xpsd`（被积函数 `(byte·flux_mul+flux_min)·λTQ`） | `spectrum_integrator.cpp`（被积函数体同文件） |
| 滤光片/QE 缓存 | prepare_filter_cache（T、Q 用 Akima 重采样到**完整** 343 点谱网格，区间外 0；权重 = Simpson 系数×T×Q×λ） | `spectrum_integrator.cpp`（重采样与权重两段同文件） |
| XPSD 解码 | F(λ)=byte·flux_mul+flux_min（W·m⁻²·nm⁻¹，逐星量化参数） | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.h`；记录解码 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` |
| 谱网格来源 | 由 XPSD 文件头 `parameters="spectrumStart=336,spectrumStep=2,spectrumCount=343,spectrumBits=8"` 逐字给出（真实分片实测） | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` |
| **非生产通道（数值对拍用）** | `compute_f_syn` / `compute_f_syn_cached`：`∫uint8·T·Q·λdλ × 10^(−0.4·magG)`；仅作数值对拍，**生产定标面不含该通道** | `spectrum_integrator.cpp`（两处非生产符号） |

**生产路径的实证判据**（真实 M42 产物；读数与证据正本 = `实验/photometric-magnitude/docs/fsyn_convention.md` [S-3]「协方差传播」一节「真实数据（生产链逐位复现）」）：
`synthetic_colour_median_mag = median_i(magG_i + 2.5·log10 F_syn,i)` 必须由上式**逐位复现**（落盘值与复算值相对差 ≤1e-12）。
该量是**天区样本的中位颜色项**：输入只来自星表与本帧滤镜/QE 曲线，不含该帧任何观测数据，用途是通带/SED 一致性诊断。它**不得用作帧零点**——帧的测光标度是 `k_photo`，帧的星等零点由 `k_photo` 换算即得；把该颜色项当帧零点会把逐天区颜色差带进深度与帧间权重。（旧名 `zero_point_mag`，仅作追溯。）

**生产编排**（`lib/algorithms/photometry/cpp/src/pc_api.cpp`）: 生产主路径 = `run_with_gaia_impl<T>`；参数校验；退化（无 PSF / 无光谱星 / 滤光片-QE 失败 → scale=1.0、rc=0）；自适应锥搜 `mag_max_arr{12..16}`×5；F_syn OpenMP `schedule(dynamic,64)` 逐星（整数 `reduction(+:n_valid_fsyn)`）；匹配+清洗；逐星 PcMatchRecord；f64 内联像素校正；`make_dr3sp_id`；v2 封装三支（`pc_calibrate_simple_with_gaia_v2` / `_f64_v2` / `_f64_v2_qf`）。旧 ABI 通道：`pc_calibrate_simple`、`_with_gaia`、`_f64`、`_with_gaia_f64`（均在同一文件）。

**图像校正**: ImageCorrector::correctImage I_cal=I·scale（`image_corrector.cpp`，OpenMP static）。

**aperture 测光非生产符号**（lib/algorithms/photometry/wrapper_phase1/photometer.cpp）: 天空环收集 d∈[sky_inner,sky_outer]; 背景中值; 孔径积分 d²≤r² Σ(pixel−background); σ_sky=1.482602218505602·MAD; flux_error=sqrt(max(sum,0)+n_in·σ_sky²); snr（各段均在同一文件）。

### 13.2 实现事实修订（ALG 文档事实层；不改根科学公式）

- 匹配为**双向最近邻互为最近邻唯一配对**（KD-tree），match_radius=2.0px（§13.1 锚）。
- 清洗为**星等预过滤 + IRLS/Tukey 稳健位置估计**；
  scale=10^(−location)（IRLS 直出），median(F_syn/F_instr) 仅为
  computeScale 残留符号（DISP-PHOT-003）。
- F_syn 网格：生产 `prepare_filter_cache` 路径把 T/Q 重采样到**谱网格本身**（343 点 / 2 nm / 336–1020 nm，末点 336+2×342=1020），`compute_f_syn_cached_xpsd` 在该网格上做复合 Simpson；非生产的 `compute_f_syn` 用重叠区 **1.0 nm** 网格（`spectrum_integrator.cpp`）。
  **网格的实测来源**：真实 XPSD 分片文件头逐字为 `parameters="spectrumStart=336,spectrumStep=2,spectrumCount=343,spectrumBits=8"`（`gaia/GaiaDR3SP/gdr3sp-1.0.0-01.xpsd`，含 11 272 905 颗源、量程 `magnitudeRange="-2.00,13.62"`）⇒ 网格与位深是**容器自报**而非硬编码；生产按文件头取参（`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`）。
  **两条路径的网格不可直接对拍**：生产用 2 nm 谱网格，非生产用 1.0 nm 重叠区网格；两者的离散误差量级不同（2 nm 网格离散误差 0.66–1.34%）。
- **参考通量口径**：`F_syn = ∫F_λ·T·Q·λ dλ`（W·m⁻²·nm，**不含** `10^(−0.4·G)`；XPSD 解码 `F_λ=byte·flux_mul+flux_min` 已是绝对谱辐照度，实测 `median(m_syn−magG)=−0.0037 mag`）。权威 `docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节。
- 生产 XPSD 光谱为 uint8 编码 F(λ)=byte·flux_mul+flux_min（解码见 `spectrum_integrator.cpp`），非 float 原始光谱。
  **量化误差（真实数据）**：逐样本相对半步步长 `0.5·flux_mul/F` 与进入 `F_syn` 的**加权和**相对误差是两个不同量级——积分后比逐样本小约 30 倍（权重代理 `w=λ`，T=Q=1）。⇒ 引用该误差时**必须声明是「逐样本」还是「积分后」**；读数正本 = `实验/photometric-magnitude/docs/fsyn_convention.md` [S-3]「协方差传播」一节 误差分解。证据与读数正本 = `实验/photometric-magnitude/docs/derivation_robust_weights.md` 网格离散误差归因。
- 自适应星等锥搜 mag_max_arr={12,13,14,15,16}（`lib/algorithms/photometry/cpp/src/pc_api.cpp`）实际
  覆盖 mag_max 入参（DISP-PHOT-005）。
- 构建现状=`lib/algorithms/photometry/cpp/Makefile` + `build.ps1`（photometric_calib.dll，链接
  gaia_client.dll）；该构建路径的集成事实不定义当前 CLI 的逐帧响应合同。
- OpenMP：F_syn 逐星 `schedule(dynamic,64)`、像素校正 `schedule(static)` 逐元素；在同一二进制、浮点环境、输入和每星操作序固定且无跨星浮点归约时，线程数变化不改变各星的算术路径。
  **该结论的依据（逐条，缺一即不成立）**：① F_syn 按星写入**各自下标**的数组，星间无浮点归约；② 并行区内唯一的归约是**整数**计数器 `reduction(+:n_valid_fsyn)`（整数加法可结合，次序无关）；③ `location`/`scale`/`sigma_residual` 的 IRLS 归约是 `cleanAndScale` 内的**串行**循环（`star_matcher.cpp`，无 OpenMP 指令），样本序由 PSF 表序唯一决定（§5.1）；④ 像素校正是逐元素乘法（`image_corrector.cpp`）。
  ⇒ 逐位复现还要求输入序及浮点环境固定；若改变星间/样本间浮点归约顺序，按数值标准重新评估，不预设 bitwise 保证。
- 匹配半径 `match_radius=2.0 px` 是**像素域常数**（`lib/algorithms/photometry/cpp/src/pc_api.cpp` 五处调用点传入；默认值见 `lib/algorithms/photometry/cpp/src/star_matcher.h`）。
  **适用域**：匹配成功要求（a）该帧 WCS 的**绝对指向残差 ≪ match_radius**（像素域），且（b）`match_radius` ≳ PSF 质心不确定度。仓内真实帧的像元尺度跨 **0.9586″/px – 6.3076″/px（6.58 倍）**，同一常数对应的天球半径为 **1.92″ – 12.6″**；testdata 真实帧的 WCS 二轮精化后残余 **0.0068″**（`docs/science/photometry/PHOTOMETRY.md` [S-1]「公式与推导」一节 第 6 条）⇒ 在该帧上条件 (a) 以 ~280 倍余量成立。**跨仪器使用时必须按像元尺度复核该常数**（2.0 px 的通用性只在上述条件下成立）。

### 13.3 实现差异记录（DISP-PHOT-001..010）

- **DISP-PHOT-001**: `star_matcher.cpp` 头注释与 PhotometricDiag
  注释（`photometric_calib.h` "当前无双向过滤/rejected_ambiguous
  保持 0"）失实——实现为双向互最近邻唯一配对，rejected_ambiguous 实际
  统计（`star_matcher.cpp`）。
- **DISP-PHOT-002**: `lib/algorithms/photometry/docs/algorithm.md` 的部分
  表述（暴力最近邻 3px/scale=median/MAD 清洗 σ=3/0.1nm 网格/2D 曲面拟合）
  与现行实现不符；现行事实以本文档 [S-5]「输入输出端口」一节 为准。
- **DISP-PHOT-003**: ImageCorrector::computeScale（`image_corrector.cpp`）
  死代码，生产无调用方。
- **DISP-PHOT-004**: 无取消检查点；无 plan/execute/cancel/inspect 生命周期
  （PHASE1_API_V1 [S-1]「物理模型」一节 为计划语义）。
- **DISP-PHOT-005**: 入参静默失效——mag_max 被自适应 mag_max_arr 覆盖
  （`lib/algorithms/photometry/cpp/src/pc_api.cpp`；同文件形参注释 /*mag_max*/）；pc_calibrate_simple
  的 QE 三参数 (void) 丢弃（同一文件，头注释亦声明保留）。
- **DISP-PHOT-006**: PhotometricDiag.rejected_quality 为 invalid+mag+irls
  混合计数不可归因（`star_matcher.cpp`）；v2 per-star reject_reason
  才可区分。
- **DISP-PHOT-007**: legacy orchestrator 的双通道调用与当前 scheduler descriptor 记录是两种载体；前者不代表当前 CLI 实际入口。
- **DISP-PHOT-008**: Photometer 孔径/天空环全图 O(h·w) 扫描 + 背景中值
  整段排序（`lib/algorithms/photometry/wrapper_phase1/photometer.cpp`），未优化且未接管线。
- **DISP-PHOT-009**: 帧级 QA 换算（sigma_mag/sigma_cal_rel）落位
  snr_estimator（snr_phot_cal_quality，`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`），本模块仅
  登记边界。
- **DISP-PHOT-010**: Gaia 候选到同一 PSF 位置出现精确等距时，当前 KD-tree 最近邻比较没有稳定 tie-break；合法输入下选择可能依候选枚举次序变化。

### 13.4 测试设计参考（未据此声明运行）

- fixture F1: 合成注入已知乘性偏移 k（location=log10 k 恢复，rtol 1e-4，
  SCI-PHOT-001 [D-1]）。**该 fixture 的行使域（必须如实声明）**：F1 的注入**无扰动**（`lib/algorithms/photometry/tests/p1phot/p1phot_fixtures.hpp` 以 `perturb_amp=0.0` 构造 ⇒ 全部 `r_i ≡ log10 k`），故 `S=0`，走的正是 [S-1]「参数与常数」一节 的 **S=0 直取 median 分支**；可执行测试同时断言 `robust_iterations==0` 与 `sigma==0`（`lib/algorithms/photometry/tests/p1phot/p1phot_tests_units.cpp`）。⇒ **rtol 1e-4 门只覆盖 median 通路，不覆盖 IRLS 迭代通路**；IRLS 通路的可执行门是 F2（`perturb_amp=0.02` ⇒ `S>0`）的 `|Δlocation|<0.1 dex`（宽松门）。
  F2: 20% 星等离群注入（Δlocation<0.1 dex）；F3:
  双向唯一配对构造帧（含歧义对，断言 unique_matches 与 rejected_ambiguous）；
  F4: XPSD uint8 光谱解码+1.0nm 积分 NumPy 参考复算（rtol 1e-9）；F5:
  退化输入矩阵（无 Gaia/无 PSF/无光谱星/滤光片失败 → scale=1.0、rc=0、
  records reject_reason 显式）；F6: Photometer aperture 已知通量 + 越界/
  空环/空孔径显式失败（对齐孔径组 4 组）。
- 不变量 I1: out_pixels=round-trip(I·scale) bitwise（f64 通道）；I2:
  sigma_residual=`median(|r_inliers−location|)/0.6744897501960817` 与逐星 records 残差一致。**该门的性质**：oracle 侧（`lib/algorithms/photometry/tests/p1phot/p1phot_oracle.hpp`）按**同一中心约定**（中心 = `location`）复算，故 I2 是**实现一致性**门（可抓编码错误），**不是定义正确性**门——若中心约定本身有误，两侧同错、门仍绿。定义口径由 §2 F4 的显式声明固定；中心约定的敏感度见 §2 F4（δ/σ 一阶偏差）。I3:
  Σdiag.rejected_*+fit_used=unique_matches；I4: records[star_id] 与输入
  psf_star_ids 一一对应；I5: 线程数扫描（1/4/N）比较科学输出；逐位相等只在运算与归约序不变时作为判据，否则按 NUMERIC 的预声明容差评价；本条是测试设计，不是运行结论。
  I6: r 方向恒为 log10(F_instr/F_syn)。
- 负面矩阵: 空指针/h·w=0/n_gaia=0/PSF 全 status≠0/锥搜失败(rc=−3)/
  handle=null(rc=−2)；断言错误码与退化登记齐全。
- 本页未声明测试设计已运行；执行结果需另有输入与运行记录。

**可复现轮 T05–T10 诚实化（合成三实验 3 项）**：

- T-PHOT-01（step1 纯解析代数合成 Oracle + 负例 + 收敛性）：复现三件套齐备。命令 `bash 实验/photometric-magnitude/code/run_all.sh`（单步 `python3 -u 实验/photometric-magnitude/code/step1_analytic.py`）；seed：固定种子 20260921（`run_all.sh` 头部声明；合成随机由 `scia_sim.py` 的 `rng(seed_tag:tag)` 派生，`seed_tag = "sim"`）；产物 hash 指针：判据汇总 `实验/photometric-magnitude/docs/GATES.md`，复跑日志落 `run/SCI-401/logs/step1.log`，输出与既有判据表逐项可对照。
- T-PHOT-02（step2 HST M16 真实信号模板前向仿真帧 A/B/C）：复现三件套齐备。命令同上（单步 `python3 -u 实验/photometric-magnitude/code/step2_hst_sim.py`）；seed 同上（20260921，`rng` 派生）；产物 hash 指针：同上（`run/SCI-401/logs/step2.log` + `docs/GATES.md` 判据表）。前向仿真模板来源见该单元 `REPORT_experiment.md`。
- T-PHOT-03（step3 XP 合成通量 vs HST PHOTFLAM 绝对定标对拍）：复现三件套齐备。命令同上（单步 `python3 -u 实验/photometric-magnitude/code/step3_forward_vs_photflam.py`）；seed 同上（20260921）；产物 hash 指针：同上（`run/SCI-401/logs/step3.log` + `docs/GATES.md` 判据表）。注意 step0 外部参考数据是唯一需网络步骤（缓存 + SHA256 固定），离线复跑用缓存。

### 13.5 非生产通道与未接入符号

- ABI 兼容通道保留（均在 `lib/algorithms/photometry/cpp/src/pc_api.cpp`）:
  pc_calibrate_simple、pc_calibrate_simple_f64、pc_calibrate_simple_with_gaia、
  pc_calibrate_simple_with_gaia_f64（with-gaia 系为 v2 封装前的实现路径，同文件）；
  生产主路径=v2/_f64_v2。
- ImageCorrector::computeScale（`image_corrector.cpp`）=待迁移符号
  （median 回退，无调用方，DISP-PHOT-003）。
- acsd::phase1::Photometer（lib/algorithms/photometry/wrapper_phase1/photometer.{h,cpp}）
  =aperture 测光待迁移符号（静态库 acsd_phase1_phot，`CMakeLists.txt`；
  单测覆盖已撤，
  未接 orchestrator 管线），aperture 合同并入 lib/algorithms/photometry/
  README.md「计划迁移旧符号」一节。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/photometry/PHOTOMETRY.md（测光拟合科学正本 SCI-PHOT-001）。
> - [S-3] docs/fsyn_convention.md（相关科学正本）。
> - [S-4] docs/engineering/testing/TEST.md（相关科学正本）。
> - [S-5] docs/algorithm.md（相关科学正本）。

## 参考文献与参考代码库（含许可证）

- Tukey biweight c=4.685：Beaton & Tukey 1974, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）；Mosteller & Tukey 1977。
- MAD→σ 0.6744897501960817：标准正态分位恒等式；Rousseeuw & Croux 1993, JASA 88, 1273。
- 最优提取/统计结构：Horne 1986, PASP 98, 609；Naylor 1998, MNRAS 296, 339。
- 误差口径 FLUXERR/MAGERR：SExtractor 官方文档 `https://astromatic.github.io/sextractor/Param.html`「Flux and magnitude uncertainties」**式 (2)**（`MAGERR = 2.5/ln10·FLUXERR/FLUX`，含 `FLUX > 0` 分支与 `99.0` 分支）与 `https://astromatic.github.io/sextractor/Photom.html` 的 `FLUXERR` 式（`FLUXERR = sqrt(Σ(σ_i²+p_i/g_i))`），原文注明「this error estimate provides a lower limit of the true uncertainty, as it only takes into account photon and detector noise」。**引用边界**：Bertin & Arnouts 1996, A&AS 117, 393（DOI 10.1051/aas:1996164）正文**不含** `FLUXERR/MAGERR` 定义，故定义式的出处 = 上述官方文档（现行版 v2.28.0；该「Flux and magnitude uncertainties」是**无编号章节**，不存在「§2.24.2」这样的节号，旧版 LaTeX 手册的 (3.2)/(11.x) 编号体系也对不上）。**差异**：本层 sigma_residual 是逐星定标散度（dex，系综量），不是单源通量误差。
- MAD 有限样本修正 `b_n`：Croux & Rousseeuw 1992, *Computational Statistics* **1**, 411（p.414 表：n=3→1.495、4→1.363、5→1.206、9→1.107；n>9→`n/(n−0.8)`；定义式 `MAD_n = b_n·1.4826·med_i|x_i−med_j x_j|`（`1.4826` 为该文献的截断展示；权威全精度 `1.482602218505602`））。**记号边界**：RC93 JASA（88, 1273）中字母 `b` 指**渐近**一致性常数（b = 1.4826，式 1.2 p.1273），**不是**有限样本 `b_n`；JASA 1993 未重印 `b_n` 表（只在 p.1274/p.1277 交叉引用）⇒ 有限样本表只引 1992 章。**免费永久链接（绕付费墙，HTTP 200）**：<https://wis.kuleuven.be/stat/robust/papers/publications-1992/crouxrousseeuw-timeeffalgosnqn-compstat-1992.pdf/@@download/file/CrouxRousseeuw_TimeEffAlgoSnQn_COMPSTAT_1992.pdf>（索引 <https://wis.kuleuven.be/stat/robust/papers/publications-1992/>）。
- **尺度路线三种并存（先验尺度 vs 同时迭代）——本条引用 Huber & Ronchetti 2009, *Robust Statistics* 2nd ed., Wiley（ISBN 978-0-470-12990-6）三处定位（节号、page 与逐字引文）**真正支撑的就是这一条文字结论**（三种路线在原文中并存）；**它不支撑本行的任何数字**。§6.4 p.133「Simultaneous M-Estimates of Location and Scale」、§6.5 p.137「M-Estimates with Preliminary Estimates of Scale」、§7.7 p.172「In practice, we calculate the estimates β and σ by simultaneous iterations」。**Tukey biweight 的效率/崩溃点常数 `c=4.685065 → eff 0.95 / breakdown 0.119414`的出处 = statsmodels**（**不是** Huber & Ronchetti 2009）：`statsmodels/robust/_tables.py` 的 `tukeybiweight_eff` 表，**版本腿 = tag `v0.15.0` = commit `278ff9950636cdd4939b4055e339a8e681d79cab`**；**并记：该表最近一次改动的不可变提交 = commit `781accd11ec9467b0a14cb1ba0e03197ab7ea9bd`**（GitHub API 一手证据；**本车道未拉树逐字核该表在这两个提交上的实际内容**，故标为「版本腿已登记、内容待核」）。引用该常数必须带 commit。
- Gaia XP/CALSPEC：Gaia Collaboration et al. 2023, A&A 674, A1；Bohlin, Hubeny & Rauch 2020, AJ 160, 21（DOI 10.3847/1538-3881/ab94b4）；Bessell & Murphy 2012, PASP 124, 140。
- Akima 子样条：Akima 1970, J. ACM **17(4), 589–602**（DOI 10.1145/321607.321609；**页域订正 P1-m06**：历史只给首页 589）。
