# 模块 acsd.phase1.photometry

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节、「Phase1 节点流程」一节、
> 「产品基数不变量」口径
> 科学正本：docs/science/photometry/PHOTOMETRY.md（SCI-PHOT-001，含「判据与误差」一节判据与观测统计量、
> 「平移精化」判据）、docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md（ALG-PHOT-001..002
> 「逐符号锚」一节）、`docs/science/noise_snr/NOISE_SNR.md`「不确定度传播」一节
> 数据正本：docs/detail/registry/acsd.phase1.photometry.md（DATA-P1-PHOT 端口表，本页输入输出端口表）
> API 正本：docs/engineering/api/PUBLIC_API.md（API-PHOT-001）、
> docs/engineering/api/abi/ABI.md、API-P1-005（docs/engineering/api/PUBLIC_API.md「分阶段 API 面」）
> 数据对象：`docs/detail/common/unified_model.md`「观测模型」一节

模块级事实以 `lib/algorithms/photometry/README.md` + `lib/algorithms/photometry/module.yaml`
（acsd.p1.photometry，迁移目标 acsd_p1_photometry.dll，entrypoint 未落地）
为准。现状构建 = `lib/algorithms/photometry/cpp/Makefile` + `lib/algorithms/photometry/cpp/build.ps1` → `photometric_calib.dll`，
未编入根 CMake 主构建。缺陷与整改登记 = PHOTOMETRIC_FIT［A-1］。
`lib/algorithms/photometry/wrapper_phase1` 的 Photometer aperture 面为迁移目标面
（测光模块 README 的计划迁移旧符号一节），当前无仓内可复算的用例引用。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_photometry_descriptor）。
职责：对检测源做 **PSF 拟合域**测光（孔径测光仅作显式声明的诊断/交叉验证，
非生产口径——依据最高设计「节点流程」一节的「全链一个通量口径」），把本帧 signal 映射到
统一线性通量尺度，给出 `a_k` 及其不确定度；源星表 ↔ 参考星表双向最近邻配对
（KD-tree，2.0px）+ 星等预过滤 + IRLS/Tukey 稳健零点求解（scale 取
10^(−location)）；质量结构体落位 snr_estimator。

不做：绝对光度定标到物理流量；Phase2 集成（integration）；逐像素 ivar
（边界 = docs/science/photometry/PHOTOMETRY.md）；星点检测；星表缓存管理。相对标度不足时
标记不可跨帧合并，**不用 median stellar flux 静默代替**；**不以物理单位论证
标定因子**——`a_k` / `k_photo` 的绝对值无物理意义，判据 = 测光一致性
（「帧间一致性」是语义目标与报告字段，不是门禁）。

端口 DATA 编目（psf→DATA-P1-PSF / sources→DATA-P1-SOURCES /
fluxes→DATA-P1-FLUX）为编排层词汇，模块合同 DATA 层 = DATA-P1-PHOT
（本页输入输出端口表）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `fluxes` | `DATA-P1-FLUX` | 可 | `UnitId::ADU`（与本页输入输出端口表 psf_flux 一致） | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0（按 DATA 合同）。

输入面：定标信号、variance/ivar、validity、PSF 模型、WCS、检测目录、配置。

输出面：源通量 `F`、通量方差、通量估计不确定度、`a_k`（光度响应）及其不确定度、
颜色项、有效域、测光 flags、通带身份块 `passband_identity`（声明通带名、
`FILTER` 关键字、解析出的库键、曲线自述名与采样点数、波长范围、曲线来源）。
绝对通量锚定由 Gaia XP 合成通量 `F_syn` 与输出 `location` / `scale` 承担（正本
= docs/science/photometry/PHOTOMETRY.md）；配置中无 `zero_point` 字段。仪器/合成流量 →
dex 残差 + `sigma_mag` / `sigma_cal_rel`。`a_k` 随产品输出，供 Phase2 使用。

**精度归属**：测光定标（`k_photo` / 零点 / `F_syn` / `a_k`）与星表匹配属**稀疏与
元数据**面，按最高设计的「数据对象与配置」章内「精度归属」条取**全程双精度**
（生产入口即 `_f64` 变体）；JSON 显式指定位深时以 JSON 为准。像素面属**稠密大面**
（单精度），定标标度施加到该面时按面自身的位深落盘，两类面的精度归属不得互相代入。

### 帧级天区合成颜色项读数键

`p1_phot.json` 的逐帧标度证据分两段：`photscale_fit`（对全部有标度记录的帧无条件
落盘的拟合证据表）与 `photscale_detail`（**已施加**帧的标度证据，与顶层
`photscales` / `photoapplied_artifacts` 同帧集合）。两段都带帧级天区合成颜色项
读数，取值直接来自帧级拟合结果的既有字段，不重算、不改拟合；`photscale_detail` 与
`photscale_fit` 的键同源同值。五个键的语义、单位、参考与来源如下。

| 键 | 单位 | 语义 | 来源 |
|---|---|---|---|
| `synthetic_colour_median_mag` | mag | 锥形搜索星族上的 `median_i( magG_i + 2.5·log10 F_syn,i )`。`magG` 是**星表**的 Gaia G 星等，`F_syn` 由该星 Gaia DR3 XP **绝对**谱经本帧滤光片与量子效率曲线正向合成；由 `2.5·log10 F_syn = C_R − m_syn^R` 推得它等于 `median_i(G−R)_i + C_R`，即**该天区样本的中位颜色项** | 帧级拟合结果，直接落盘 |
| `synthetic_colour_valid` | 1（布尔） | 该颜色项是否可用；`false` 表示不得使用（参与中位数的星数不足求解前提） | 同上 |
| `synthetic_colour_n_stars` | 颗 | 参与中位数的锥形搜索星数 | 同上 |
| `synthetic_colour_scatter_mag` | mag | 星族谱能量分布散布的稳健标准差 `1.482602218505602·MAD(...)` | 同上 |
| `synthetic_colour_reference` | 字符串 | 参考自述：`Gaia DR3 XP absolute spectrum × filter/QE curve`，附 `filter_key` / `curve` / `n_points` / 波长范围 `wl_nm`，供核对「实际使用的曲线是谁」 | 由本帧通带身份字段拼接 |

- **该组是通带/SED 一致性诊断项，不是帧的星等零点**：它的两个输入——`magG`（星表
  星等）与 `F_syn`（谱 × 滤镜/QE 的**绝对**辐照度积分）——**都只来自星表与本帧的
  滤镜/量子效率曲线，不含任何该帧的观测数据**（无像素、无实测流量）。唯一与帧有关
  的是通带曲线 `T(λ)`/`Q(λ)`，那是**仪器配置参数，不是观测数据**。因此它**不可能测到
  帧的增益或透过率**，**不得用作帧零点**，也**不得据此反推增益/口径/曝光**。
- **帧的测光标度是 `k_photo`**：帧级标度 `k_photo` 来自星表匹配对数比的稳健位置，
  是唯一的帧级测光标度；**帧的星等零点由 `k_photo` 换算即得**。该颜色项与 `k_photo`
  不是「同一件事的两条实现」，二者本就不该相互比对一致性——它们量的是不同的东西。
- **旧名（仅作追溯）**：该组键曾名为 `zero_point_mag` / `zero_point_valid` /
  `zero_point_n_stars` / `zero_point_scatter_mag` / `zero_point_reference`。旧名把
  它表述为「本帧合成零点」，曾导致把它与 `k_photo` 当成两条独立实现去比对不一致的
  真实误诊（详见《阶段一测光标定》实验单元的追记）。
- **无跨帧归一化 gauge**：标度规范是帧内的，产物字段 `photscale_spread_gate =
  "none (frame-independent)"`。跨帧 `k_photo` 不同是正常的帧间独立事实，不得据此
  写出「跨帧对齐」「组内公共零点」之类不存在的语义。
- **下游为何需要**：该组键是**通带身份与天区星族**的可核对面（配合 `passband_identity`
  可回答「实际用的是哪条曲线」「该天区的 SED 散布多大」）。它**不**是下游锚定参考通量
  或深度的合法输入；正本不宣称绝对通量刻度，故任何绝对量都不得由内部值"制造"。
- **已登记的下游缺陷（待修，见下）**：噪声与信噪比节点当前把该颜色项当作帧零点用。
- **标准产物**：逐帧 JSON 是节点之间的标准产物，不受开发模式开关管辖，其键面不随
  运行模式变化。

### 已登记的下游缺陷：颜色项被当作帧零点用

噪声与信噪比节点读 `photscale_fit` 的该组键，按
`zp_inst = synthetic_colour_median_mag − 2.5·log10(k_photo)` 复算「本帧仪器零点」，
再据此锚定参考通量 `F_ref = 10^(−0.4·(m_ref − zp_inst))` 与 5σ 深度 `m_5`。
其中 `k_photo` 是帧可比量，而 `synthetic_colour_median_mag` 是**逐天区**的颜色项
（同一指向组跨夜仅变约 0.0004 mag，**跨指向组变约 0.0587 mag**），于是：

- `m_5` 被带入**最多约 0.06 mag 的天区间系统差**——该差来自天区星族颜色，不是帧的条件差异；
- `F_ref ∝ 10^(0.4·zp_inst)` ⇒ 帧间权重 `w ∝ 1/F_ref²` 的**天区间相对误差可达约 11%**；
- 因此受影响的**不止深度，还包括阶段二的帧间权重**。

修法方向见下一节。

### 正确的仪器零点式

帧的仪器星等零点按绝对刻度定义，**不使用任何逐帧的样本颜色项**：

    zp_inst = C_abs − 2.5·log10(k_photo)

推导一行：标准星等满足 `m_std = zp_inst − 2.5·log10(F_instr)`，而 `F_instr = F_abs / k_photo`
（`k_photo` 是把仪器计数换成绝对通量的帧级因子），代入即得上式。因此 `zp_inst` 的**唯一帧相关
输入是 `k_photo`**；帧与帧之间的零点差只反映帧透射率差，这正是「帧间可比」所要的。

#### `C_abs` 的分解：为什么它**分「(滤镜曲线, QE 曲线) 二元组」**

对参考通带 `R` 有 `z^R_i = magG_i + 2.5·log10 F_syn^R,i = C^R + (G−R)_i`，于是

    median(z^R) = C^R + median(G−R)

`median(G−R)` 只由**天区星族**决定（同一指向组跨夜仅约 0.0004 mag，**跨指向组达约 0.0587 mag**）。
若把 `C_abs` 逐运行地取成"本次运行各帧的 `median(z^R)`"，则公共锚随运行与天区漂移，帧间权重
`w ∝ 1/F_ref²` 的天区间相对误差可达约 **11%**。零点式的修法就是**把该锚冻结**，使逐帧 `zp_inst`
的差异只来自 `k_photo`。

**`C_abs` 分「(滤镜曲线, QE 曲线) 二元组」**：不同通带的积分宽度不同，各有独立的绝对刻度常数，因此
**不能**用另一条通带（例如 Gaia G）的测量去代替某通带的 `C_abs`。该路线**作废**——它曾把
「绝对刻度检验的残差 `median(m_syn − magG)`」误当作常数本身，量级差约 14 mag，会使 `m5`
落到物理不可能的量级。

**QE 的贡献大于滤镜（同参考域实测）**：在**同一参考域（16 帧 T2）**上，同帧集**只变 QE**
（`Ideal QE curve`→`KAF-16803`）⇒ `Baader R` 下 **−0.809906 mag**、`Astrodon E-series R` 下
**−0.865137 mag**；**只变滤镜**（`Baader R`→`Astrodon E-series R`）⇒ 未声明 QE 时
**−0.569093 mag**、`KAF-16803` 时 **−0.624324 mag** ⇒ **QE 的贡献大于滤镜**。
⇒ 「分通带」在实现上必须读作**分「(滤镜曲线, QE 曲线) 二元组」**：只按 `filter_passband`
一个字符串取常数，会在 QE 不同时**静默用到为另一 QE 测的常数**（与 `PHOTOMETRY.md §2`
「`Q` 是通带的组成部分而非可选项」一致）。

#### 方案 A 的约定

`C_abs` 冻结为**在文档化参考域上按二元组测定的值**，不逐运行、不逐天区重算：

| 项 | 值 |
|---|---|
| 参考域 | **固定为 16 帧 T2 参考集**（其 `median(z^R)` 的块中位取自已发布的基线产物）；**跨域平移实测约 0.015 mag ⇒ 常数不可跨域复用**，换参考域必须重测重登记 |
| 参考二元组 | **(`Baader R`, 未声明 QE / `Ideal QE curve`)** —— 该二元组的 QE 因子恒 1（库内 `Ideal QE curve` 6 点全为 1.0、波长域 1–2500 nm 覆盖 XP 网格 336–1020 nm，故与 `Q≡1` 数值等价） |
| 数值 | **−14.276811281840642 mag**（MAD 9.212e-03、峰峰 5.872e-02） |
| 适用域 | **仅对该二元组成立**；其它二元组各自测量、各自登记（见《`C_abs` 的通带适用范围与守卫》的《已登记表》） |

**边界（必须如实）**：该常数**包含参考域样本的中位合成颜色**，因此系统的绝对刻度锚在
上述**参考域**，而不是 Gaia G 的绝对通量刻度。两者的差别是**一个常数**，在相对权重里整体
抵消，只影响 `m5` 的绝对数值。**不得**把它描述为"与星族无关的通带常数"。

**颜色项不得进入 `zp_inst`**：`synthetic_colour_median_mag` 一族键只作通带/SED 诊断读数，
它们是**逐天区**量（跨组约 0.0587 mag），进入零点会把天区间星族差异带进绝对刻度。

**声明正确性边界（必须如实）**：登记的是**按声明测量**的值——二元组里两条曲线的物理正确性
**未经外部标准核对**（`Astrodon E-series R` 的来源为负责人指定，见 `configs/README.md` 的登记）。
⇒ 每个登记项必须**并列**给出测量所用的 **(T, Q) 声明对**、参考域与统计量；**声明本身是否正确
由单帧测光一致性残差判据承担**。**不得**把某个二元组的值描述为"本设备滤镜的常数"。

**`C_abs` 的作用域**：它在帧间权重 `w ∝ 1/F_ref²` 中**整体抵消**（全体帧同乘一个因子），
因此不影响相对权重；但它**决定 `m5` 的绝对正确性**，故取值必须有本节依据，不得现场标定。

### `C_abs` 的通带适用范围与守卫

`C_abs` 分 **(滤镜曲线, QE 曲线) 二元组**，而每个登记值只在**它自己的二元组**上测定；曲线库
另有 44 条具名滤镜曲线与 12 条 QE 曲线。因此零点装配处设有**二元组守卫（按声明查表）**：

- **按键判定**：读配置声明的滤镜名（`photometry.fit.filter`，否则块级 `filter_passband`）与
  QE 名（`photometry.fit.qe_name`），组成 `(滤镜曲线, QE 曲线)` 键。QE 未声明（`Q≡1`）与库内
  `Ideal QE curve` **归一到同一键**——数值等价依据：该曲线 6 点全为 `1.0`、波长域 1–2500 nm
  覆盖 XP 网格 336–1020 nm，故对 `F_syn` 积分与 `Q≡1` 逐位等价；
- **命中已登记二元组** ⇒ 用**该二元组**的常数按 `zp_inst = C_abs − 2.5·log10(k_photo)` 产出深度；
- **未命中**（含滤镜名/QE 名取不到、为空） ⇒ **fail-closed**：本运行**不产出**绝对深度 `m5`
  （`m5` 与逐帧零点按未知落盘），**不复用**其它二元组的常数；具名原因行
  （滤镜名为空 ⇒ `filter_identity_unavailable`；否则 ⇒ `cabs_pair_not_registered`，并给出
  运行二元组与已登记对数）写入运行日志供复核。该分支沿用既有的同型契约——「缺该帧标定
  ⇒ 本帧 fail-closed，不伪造、不回退到块级中位数」。

⇒ **已登记二元组可产出 `m5`；未登记二元组 fail-closed。**

**已登记表（当前四对，同一参考域 = 16 帧 T2）**：

| 滤镜曲线 | QE 曲线 | `C_abs` [mag] | MAD | 参考域 / 统计量 |
|---|---|---|---|---|
| `Baader R` | 未声明 QE（`Q≡1`）／`Ideal QE curve` | **−14.276811281840642** | 9.212e-03 | 16 帧 T2 参考集，`median(z^R)` 的块中位（峰峰 5.872e-02） |
| `Astrodon E-series R` | `KAF-16803` | **−15.711040624768852** | 8.381e-03 | 同一 16 帧 T2 参考集（峰峰 3.539e-02；跨域跨 build 复现差 0.0011 mag） |
| `Baader R` | `KAF-16803` | **−15.086716871104473** | 1.126e-02 | 同一 16 帧 T2 参考集（只变 QE 的一对） |
| `Astrodon E-series R` | 未声明 QE（`Q≡1`）／`Ideal QE curve` | **−14.845903818286661** | 9.310e-03 | 同一 16 帧 T2 参考集（只变滤镜的一对） |

⇒ 四对覆盖 A3 四臂的全部组合（①历史口径 / ②只修滤镜 / ③只修 QE / ④全修）。**参考域固定为
16 帧 T2**；跨域平移实测约 **0.015 mag** ⇒ 常数**不可跨域复用**。

**守卫的实测证据**（同一单帧配置，仅改声明）：

| 运行二元组 | 深度 `m5` | 逐帧零点 | 公共锚 | 日志 | 去向 |
|---|---|---|---|---|---|
| (`Baader G`, 未声明)（未登记） | **未知** | **未知** | **未知** | 出现守卫原因行（`cabs_pair_not_registered`） | 既有同型分支（`reference_flux_source = group_median`）｜该行为历史读数；QE 现为必填，等价复跑须声明 `Ideal QE curve` |
| (`Baader R`, 未声明／`Ideal QE curve`)（已登记） | 20.5772 | 26.3330 | −14.2768 | 无守卫行 | 正常产出 |
| (`Astrodon E-series R`, `KAF-16803`)（已登记） | **20.5755** | 26.4036（等价复算） | −15.7110 | 无守卫行 | 正常产出 |
| (`Astrodon E-series R`, `Ideal QE curve`)（已登记） | **20.5756** | — | −14.8459 | 无守卫行 | 正常产出 |

**查表守卫的实测读数**（单帧真实 T2/M1；`m5` 与 `reference_flux_source` 取自该次运行的 `p1_snr.json`）：

**四臂逐臂探针**（A3 四臂的组合，现**全部命中**）：

| 臂 | 配置声明 `filter` / `qe_name` | 归一后二元组 | 守卫 | `reference_flux_source` | `m5` |
|---|---|---|---|---|---|
| ① 历史口径 | `Baader R` / `Ideal QE curve` | (`Baader R`, 未声明) | **命中**（第 1 条） | `fixed_magnitude` | **20.57716689703168** |
| ② 只修滤镜 | `Astrodon E-series R` / `Ideal QE curve` | (`Astrodon E-series R`, 未声明) | **命中**（第 4 条） | `fixed_magnitude` | **20.575565574197906** |
| ③ 只修 QE | `Baader R` / `KAF-16803` | (`Baader R`, `KAF-16803`) | **命中**（第 3 条） | `fixed_magnitude` | **20.57644752216904** |
| ④ 全修 | `Astrodon E-series R` / `KAF-16803` | (`Astrodon E-series R`, `KAF-16803`) | **命中**（第 2 条） | `fixed_magnitude` | **20.575476559797803** |

**未登记二元组的负例**（`Baader G` + `Ideal QE curve`）：守卫**未命中** ⇒
`reference_flux_source = group_median`（既有同型分支）、`m5 = null`（**不产出**），日志
`C_abs 二元组守卫: fail-closed (运行二元组=(滤镜='Baader G', QE='Ideal QE curve'（归一为未声明 QE）), 已登记对数=4, 原因=cabs_pair_not_registered)`。
⇒ **判命中不能看 `exit_code`**：该分支是数据有效性 fail-closed，**不置非零退出码**（四臂与负例
`exit_code` 全为 0）；判据是产物 `p1_snr.json` 的 `reference_flux_source` 与 `m5`。

**结构性事实（已定案）：`sigma_residual_dex` 不含零点路径依赖**。它由测光节点在
`r = log10(F_instr/F_syn)` 上算出，并在 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:371`
赋给 `out.sigma_residual_dex`；该文件对 `C_abs`/`cabs`/`kCAbs` 的命中数为 **0**。`C_abs` 的全部
消费点都在 `lib/infrastructure/scheduler/src/module_adapters.cpp`（24 处），且只进入**深度/SNR
节点**（`p1_op_noise`，写 `p1_snr.json`）——该节点在节点序 `phot→snr→drz→wr` 中位于测光节点
**下游**，而 `p1_phot.json`（承载 `sigma_residual_dex`）在测光节点即写盘。⇒ **两条零点路径
（冻结常数 / 逐运行中位）对该量的贡献结构性恒为 0**，不得把零点路径差异记到该量上。

**剩下的限制（不是静默错误）**：其它二元组的常数**尚未登记** ⇒ 要支持它们，按
「**按二元组登记常数**」逐个做参考域测量并登记（本页的《已登记表》即该机制的登记面；
守卫按二元组查表，未命中即 fail-closed，见上）。另一条精化路线（**双通带绝对锚定**）仍待做：

1. **双通带绝对锚定**（精化）：用 `C_abs = median(z^R) − median(G−R)_synth` 把常数锚到 Gaia G
   的绝对通量刻度，从而**通带无关**地定义绝对刻度。

**精化项登记**：第 1 条归入测试 `photometry_gpassband_absolute_constant`；其判据在原有两条
（G 通带下 `median(m_syn − magG)` ≈ −0.0037、MAD ≈ 0.0033、n=11272；以及由 `C_abs` 反推的
`zp_inst` 必须落在物理合理区间）之上，**补上**「**同一参考域**下 `median(G−R)_synth` 的测定，
以及它与方案 A 常数之差」。该测试不在生产路径上运行。

**冻结锚消除了跨天区的混入**（在 t2 参考集上按指向组中位分解；`Z` = 天区颜色项、`A = −2.5·log10(k_photo)`
为该帧真实透过率项；两者由 `Z_i = 旧 zp_inst_i − A_i` 逐帧反推，按组得到的 `Z` 与独立测量一致）：

| 量 | 组间极差（mag） |
|---|---|
| `Z`（天区颜色项） | 0.05861 |
| `A`（真实透过率项） | **0.12197** |
| 旧零点 = `Z + A` | 0.09706 |
| 新零点 = `C_abs + A` | **0.12197**（与 `A` 的极差相等，**颜色项贡献为 0**） |

方差分解给出 `Var(旧) = Var(Z) + Var(A) + 2·Cov(Z,A)`，其中 `Cov(Z,A) = −0.000489`、相关系数
`r = −0.470` ⇒ 旧的表观一致**部分是两个独立来源相互抵消的假象**。冻结锚的零点离散**恰等于真实
透过率的离散**，天区颜色项的贡献为零，这正是方案 A 的设计意图。

### 数值落地口径

通量尺度、误差传播与判据的推导正本 = SCI-PHOT-001 与 ALG-PHOT-001..002；本页
只记落地方式：

- 孔径测光（孔径定义、sky 环、误差传播）仅作显式声明的诊断/交叉验证，不是
  生产口径；PMM 用的正是孔径口径，**不照搬**；
- 生产口径 = PSF 拟合域测光：全链各模块共用同一个 `flux` 口径，通量为 PSF
  拟合域下的最大似然估计，其不确定度为信息权重的倒数（与 noise_snr 联动）；
- 相对标度不足 → 标记"不可跨帧合并"，不静默降级。

**测光一致性判据（单帧、尺度无关、双边界）**：判据只有一条 —— 施加后星点星等
与 Gaia 残差的散度小（「帧间一致性」是语义目标与报告字段，不是门禁）。门判据
= **单帧标定是否可信**，与其它帧无关。阈值只能从本帧误差预算导出，拍脑袋的
固定阈值属另一口径。

落地形态为三个量：**观测量散度**（本帧匹配星 — Gaia inlier 残差的 MAD，按
SCI-PHOT-001 的换算因子还原为标准差）、**上界**（本帧九项误差预算合成的
预算上限，按本帧匹配星数放宽）、**下界**（本帧匹配星数给统计涨落设的物理
下限，按星数收缩）。判定 = 观测量落在下界与上界之间；三个量与逐项分解无论
通过与否都报出。九项预算为：拟合（白 / 含天光结构）、生产拟合器对独立孔径的
系统比值散度（系统主导项）、颜色项（QE 未建模的通带失配，生产取 QE ≡ 1）、
天光残差、Gaia、平场高通、量化；逐项定义、取值与出处正本 =
docs/science/photometry/PHOTOMETRY.md。MAD→标准差估计量的相对标准误 SD 因子取闭式
`c_se = 1/(4φ(d)·d) = 1.1663872874444212`（`d = Φ⁻¹(3/4)`；文献登记值 `1.361`
只按 3 位有效数字使用，其平方与闭式相差约 −0.04%，在 RC93 JASA 未给 MAD 绝对
渐近方差前不作为精确式），3 倍作抽样允差（唯一约定性选择）。

**判不了的项按不加处理**（上限偏严 ⇒ fail-closed）：平场大尺度残差（缺
repeat-flat / sky-flat 对照）、Gaia XP 合成通量 `F_syn` 定标误差、本仓 `*.xpsd`
的 `magBP` / `magRP` 实测恒为 0、望远镜光学/大气/差分消光项（无仓内曲线）。

**落地状态**：该门判据尚未在代码中生效。本模块须先产出 ① 逐星残差表
（`star_id`, `F_instr`, `G_Gaia`, `F_syn`, `r_i`, `inlier`）、② 每帧的
系统比值散度、③ 冻结的误差预算表（九项 + 出处，作表而非散落常数）、④ 能红能绿
负例（见本页「独立 synthetic 验证命令与容差」的 Oracle 面）。在 ①–④ 具备前，
测光一致性门取「未生效」形态，固定 mag 阈值
不以任何形式进入实现、测试与文档。

**空间场噪声底只作诊断，不作门**：`spatial_gain` 的 `noise_floor_dex` /
`noise_floor_max_dex` 是空间场**系数**的解析噪声底（`S·sqrt(mean_pix(B̃ᵀH⁻¹B̃))`
与 `max_pix` 对应量），只含系数估计的统计误差，**不含逐星测光误差**，故
`sigma_spatial_residual_dex / noise_floor_dex` 恒大于 1，其数值**不构成达标判据**。
测光一致性判据仍为「施加标定后星点星等与 Gaia 残差的散度落在本帧误差预算的
下界与上界之间」（本页「测光一致性判据」一节）；其中"不可再降"的参照量是
**逐星光子噪声**（由 `p1_flux` 的 `flux` / `flux_error` 按 `0.4343·σ_F/F` 换算）。

### 键名订正与兼容窗口（A11）

帧级 SNR 键 `snr_reference.reference_pair_zero_point_mag`（`C_abs` [mag]）记录的是
**按「(滤镜曲线, QE 曲线) 二元组」在文档化参考域上冻结的常数**（登记表见本页
《`C_abs` 的通带适用范围与守卫》的《已登记表》），与帧无关、不随星族/天区变化。

- **旧名**：`reference_zero_point_syn_mag`。`syn` 既不表意、又暗示该量与「合成」而非
  「参考域冻结」有关，会被读成逐帧算出来的量 ⇒ 订正为现名（把「**参考域**」与
  「**二元组**」两件事说出来）。
- **兼容窗口**：读侧**接受旧键**（旧产物仍可读，统一模型 schema 的 `frame_snr` 同时
  列出两个属性）；**写侧只写新键**。窗口内的新旧键**同义同值**，不得并存于同一产物。
- **退役条件**：仓内不再存在含旧键的产物与消费者（grep 归零）后，删除 schema 中的旧
  属性 —— 不留「永远兼容」的悬空约定。
- **值不变**：改名不改任何数值/判据/口径；同一帧同配置下新旧键取值**逐位一致**。

### 外部参考实现的引用纪律

PhotometricMosaic（PMM）的使用面 = 只读方法研究：可读源码做方法研究；代码只读、
保持原样；引用面 = 方法学描述；分发面 = 无。测光口径**不照搬** PMM。可借鉴的
是其报告范式：跨帧一致性只作 warning / 报告字段，不作拒绝帧的门（实现字段
`photscale_spread_dex` / `photscale_spread_warn` / `photscale_spread_gate =
"none (frame-independent)"`）。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-PHOT-001（docs/engineering/api/PUBLIC_API.md 测光节；头
`lib/algorithms/photometry/cpp/include/photometric_calib.h`，6 导出符号，
生产入口 `pc_calibrate_simple_with_gaia_f64` / `_v2`）；编排级 API =
API-P1-005（phase session extern "C"）；生命周期 create→validate→run→inspect→
destroy。

C ABI 规则：`PC_API` / `extern "C"` 不抛异常；`gaia_client_handle` 为 opaque
borrow，模块不持有；`spec_stars` / `spectra_buf` 在调用内释放；`out_*` 由调用方
分配与释放。

### Production callers

生产调用 = orchestrator.cpp `run_stage_photometric` → `pc_calibrate_simple_with_gaia_f64_v2`
/ `_v2`（dll_loader.cpp 加载）。

### 源文件

`lib/algorithms/photometry/`（生产面 `cpp/`；aperture 诊断面
`wrapper_phase1/photometer.h`）。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.photometry`（registry descriptor 口径）；模块合同
module.yaml 登记 `acsd.p1.photometry`。execution_class=`cpu_heavy`;
parallel_ok=True。配置 = phase config JSON：

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `mode` | `psf` | —— | 生产口径 = `psf`（PSF 拟合域）；`aperture` 仅诊断/交叉验证，须显式声明 |
| `aperture_radius` | 4.0 | px | 孔径测光半径——仅诊断/交叉验证。取值口径 = 孔径测光实现 `wrapper_phase1/photometer.h` 的 `Photometer` 构造默认值；配套 sky 环 6.0–10.0 px |
| `sky_annulus` | —— | px | sky 环——仅诊断/交叉验证；生产口径的局部背景由 PSF 拟合域承担 |

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是（资源门拒绝 heavy+serial 组合）; worker 数 =
ThreadBudget.max_workers（唯一取值源）。现状帧级串行，无共享状态。

确定性 = NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同（bounded）; I-O 单 writer。

所有权 = 输出结构由调用方释放；输出 buffer 由调用方分配。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）。
输入校验失败 → 负返回码（API-PHOT-001）。参考星 / PSF 星 / 光谱星不足或滤光片
失败 → 退化恒等校正（scale = 1.0、rc = 0，diag/records显式登记，测光模块 README 的错误语义一节）。

边界：源太暗/太亮 → 测光 flags，不产出无意义通量；`a_k` 不确定度缺失 → 标记
不可跨帧合并；饱和/拖线源标记，不进默认路径。

**失败作用域（帧级 vs 全局，各自具名；正本见日志错误系统落地设计的失败作用域一节）**：

- **帧级失败**（该帧自身条件不成立）⇒ 该帧记 `status=fail` + `error_domain` /
  `error_status` / `error`、不产出 `photoapplied_<base>`、不进 `photscales`，
  其余帧照常拟合与施加。稳定错误码：`PHOT_SOURCES_FRAME_MISSING`、
  `PHOT_WCS_UNUSABLE`、`PHOT_WCS_SIP_INVALID`、`PHOT_FRAME_UNREADABLE`、
  `PHOT_FIT_NO_SCALE`、`PHOT_SCALE_NON_PHYSICAL`、`PHOT_SCALE_MISSING`、
  `PHOT_SCALE_NO_FIT_EVIDENCE`、`PHOT_FIT_IMPLAUSIBLE_SCATTER`、
  `PHOT_PASSBAND_IDENTITY_INCONSISTENT`（该帧曲线身份与组内首帧不一致；同时置
  `passband_identity_inconsistent` 标志）。帧级失败**不是降级**，不写
  `degraded_reason`；
- **全局失败**（星表不可读、曲线库目录不可达或为空、目录内曲线文件不可解析或身份核对
  失败、名字不在扫描集（`unknown_filter`）、`gaia_data_dir` / `filter` / `qe_name`
  缺项、冻结 C 入口返回非零）⇒ **中止运行**
  （`ErrorDomain::CONFIG` / `IO` 上行到 CLI 收敛为退出码），不把整批帧逐帧判 fail；
- **运行级判红**由产品基数不变量给出（最高设计「输出合同」一节「任何一帧未被处理、跳过
  或失败都显式判红」）：失败帧没有 HiPS 产品 ⇒ `write_hips` 上抛
  `SCIENCE_PRECONDITION`（退出码 4）且**不发布** `p1_products.json`；其他帧已
  写出的产品保留在磁盘上；
- **逐帧真相**在 `p1_phot.json.frames[]`（`DATA-P1-PHOTPROV-001`）；组级
  `photometry_applied` 只表示「至少一帧已施加」（`pixel_scaling` ∈
  `applied` / `partial` / `none`），下游按 `frames[]` 逐帧选择输入面；
- **通道未配置**（无 `photometry.fit` 段且无 `p1_photscale.json`）⇒ 显式降级
  `degraded_reason=photscale_absent`（中性 photscale = 1.0，下游按未归一化 ADU
  消费并如实登记）。

取消 = 协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成
→ exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint。
- **负例与归零分支 N25（T05–T07 负向轮，本卡死值与静默 scale）**：负例输入构造 =
  相对标度不足（`a_k` 不确定度缺失）的帧。预期行为 = 标记不可跨帧合并，不静默
  降级、不以物理单位论证标定因子绝对值。落盘标记 = 不可合并标记 + 误差预算三量
  （观测量 / 上界 / 下界）；静默降级或跨帧一致性门 ⇒ 判红；

## 独立 synthetic 验证命令与容差

测试标识 = `TEST-P1-PHOT-001`（registry descriptor 单源）；执行证据 = NOT_VERIFIED
（未取得验收证据）；容差 = NOT_VERIFIED（同源）。测试设计 = `TEST-PHOT-DESIGN-001`
（PHOTOMETRIC_FIT［A-1］，冻结容差：fixture F1–F6、不变量 I1–I6、负面矩阵、
SCI-PHOT-001 容差 —— 注入 rtol 1e-4、20% 离群 Δlocation < 0.1 dex、NumPy
rtol 1e-9）。

已取证的相邻锚：Photometer 4 组读数与对齐回归读数；其载体不在本仓可复算路径上，
引用时只作背景。

Oracle 面：

- 注入已知通量源 → 通量恢复的 bias / variance 符合理论；
- PSF 测光与独立孔径测光交叉；
- 通量不确定度 = 信息权重倒数的理论一致性；
- `a_k` 不确定度传播到 Phase2 covariance 验证；
- **负例与归零分支 N01（T05–T07 负向轮，星数不足记 NaN）**：负例输入构造 = 匹配后
  `|r_consistent| < 3`（或有效参考星 / PSF 星 / 光谱星任一不足）。预期行为 = 不拟合、
  不写标度，`scale = 1.0`、`fit_used = 0`、`sigma_residual = 0`（含义 = 无不确定度
  可用，不是零不确定度），该帧 `status = fail` + 具名 `error_status`
  （如 `PHOT_FIT_NO_SCALE`），不产出 `photoapplied_<base>`、不进 `photscales`。
  落盘标记 = `p1_phot.json.frames[]` 该帧 `status/fit_used/sigma_residual` 三键齐全；
  把该分支改成静默用 `median stellar flux` 代替 ⇒ 判红；
- 测光一致性判据的负例（能红能绿，判据生效的必备条件）：① 伪造常数残差表
  （`r_i` 全等）⇒ 观测量低于下界 ⇒ **必须判红**；② 把某帧 `F_instr` 乘随机
  因子（人为注入 0.2 mag 散度）⇒ 观测量高于上界 ⇒ **必须判红**；③ 健康帧
  （合成与真实各一帧）⇒ **必须判绿**；④ 人为加入跨帧 k/scale 一致性门 ⇒
  **必须判红**（`PHOT-GATE-DROP-001`：组间一致性只作报告字段）。在 ①–④ 具备前
  通过/不通过门取「未生效」形态。

## 对外投影 API 契约：`pc_project_sky_to_pixel`

本模块对外提供一个天球到像素的投影入口，供阶段一的星表引导检出使用。它与解算器共用同一套畸变系数与展开实现，是"完整变换"的唯一对外面。

**签名**

```c
int pc_project_sky_to_pixel(
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order, const double* sip_a, const double* sip_b,
    const double* ra, const double* dec, int n,
    double* out_x, double* out_y);
```

**入参语义与单位**：`crval1/crval2` 为参考天球坐标，单位度；`crpix1/crpix2` 为参考像素，**FITS 1 基**，与本帧解算产物的 `crpix` 同口径；`cd11..cd22` 为线性变换矩阵元，单位度每像素；`sip_order` 为畸变阶数，合法域 `[0,5]`，`0` 表示无畸变项；`sip_a/sip_b` 为前向畸变系数，**长度 36**，按 `i*6+j` 索引（`i` 为横轴幂次、`j` 为纵轴幂次，项为 `x^i·y^j`），自变量是相对 `crpix` 的像元偏移，量纲 `pixel^(1−i−j)`，先与偏移相加再乘 `cd`；`sip_order = 0` 时二者可为 `NULL`；`ra/dec` 为待投影的天球坐标数组，单位度；`n` 为数组长度。

**输出**：`out_x/out_y` 为**0 基像素坐标**（像素中心即整数下标），调用方**不得再做 −1 换算**。该约定与天球引导检出的预测位置口径一致。

**返回码与异常**：返回 `0` 表示成功；`−1` 表示入参非法（指针为空或 `n < 0`）；`−2` 表示 `sip_order` 越界（不在 `[0,5]`）；`−3` 表示内部构造失败。**本函数不抛异常**，所有失败以返回码表达。

**上游下游**：上游是 `acsd.phase1.wcs-platesolve` 的完整解算产物（含畸变系数），本入口按该产物的系数构造变换；下游是阶段一的星表引导检出，它用本入口把参考星表逆投影到像素域，再在预测位置上做质心与椭圆高斯拟合。引导路径对预测位置不做搜索，因此预测偏差必须小于星像尺度——口径变更（例如把完整变换换成丢畸变的线性近似）会直接导致场边参考星检不出，影响面覆盖阶段一的星表引导检出与依赖其星表的后续阶段。

**为什么不提供近似回退**：畸变在场边可达二十余像素、远超星像尺度，丢畸变的近似在场边必然失败；这类"本来就应该正确"的行为不设可开关的兼容分支，避免两套行为长期并存而无法归因。

## 已知限制

- 缺陷与整改登记 = `docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`［A-1］；
- 测光一致性门判据尚未在代码中生效（缺逐星残差表、每帧系统比值散度、冻结的
  误差预算表、能红能绿负例四项前置产物）；
- 平场大尺度残差、Gaia XP 合成通量定标误差、光学/大气/差分消光三项误差无实测
  依据，按不加处理；
- 现状构建产物 `photometric_calib.dll` 未编入根 CMake 主构建；目标交付形态
  acsd_p1_photometry.dll 的 entrypoint 未落地；
- `wrapper_phase1` 的 Photometer aperture 面为迁移目标面，仅作诊断/交叉验证；
- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。
