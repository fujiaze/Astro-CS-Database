# Calibration Science (SCI-CAL)

> 上游：ACSD_DESIGN.md §4.2（Phase1 节点流程）、§4.6（硬约束）

> 本文件条款为冻结定义，变更走变更流程。

## 1 目的与非目标

- **目的**：去除仪器签名（bias/dark/flat/cosmetic），使多帧信号在同一物理标度下可比较，为后续 PSF/astrometry/photometry、Drizzle、Phase2 UPM/rejection 产出提供校准后帧。
- **非目标**：不校正非线性/电子增益（测光定标层处理）；不估计科学噪声方差（`snr_estimator` 独立估计）；不做天光背景扣除（Phase2 控制采样/UPM 处理）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `raw`/`light` | 单帧观测亮场 | `ac_calibrate_frame` 输入 |
| `bias` | 零曝光本底母版（读出偏置 + 偏置结构）；与 light 同标度 | `ac_generate_master_bias` |
| `dark` | 暗电流母版，**已减 bias**（zero-corrected）；与 light 同标度 | `ac_generate_master_dark` |
| `dark_total` | 兼容形态：**含 bias** 的暗场母版 `dark_total = bias + dark`；仅当显式 `dark_optimization=true` 时接受 | 外部流水线母版（如 PixInsight/WBPP） |
| `flat` | 平场帧 | `ac_generate_master_flat` |
| `flat_norm` | 归一后平场 `median=1.0, floor 0.1` | `normalize_flat` |
| `K` | 暗场缩放因子 `K=t_light/t_dark` | `calibrate: k_init` |
| `cal` | 校准后信号 | 输出 `out` |
| `t_light`, `t_dark` | Light/Dark 曝光时长 (EXPTIME) | 调用方 FITS 头 |
| `sigma_low/high`, `max_iter`, `combine` | sigma-clip 阈值/迭代/合并方式 | `generate_master` |
| `hot_sigma/cold_sigma/method/max_structure_size` | 坏点检测/修复参数 | `ac_correct_frame` |

## 3 物理量和单位

- `raw/bias/dark/flat/cal`: ADU（同滤镜/增益下标度）；`t_expo`: s；`K`: 无量纲；`flat_norm`: 无量纲（median=1.0, floor 0.1）；`sigma`: 无量纲倍数（以 MAD 转 sigma）；像素坐标无量纲。
  - **ADU 域定义（冻结）**：ADU 域 = FITS 物理值域，即 `物理值 = BSCALE·样本 + BZERO`。
    权威（一手）：**FITS Standard 4.0 §4.4.2.5「Keywords that describe arrays」的 `BUNIT`**（4.0 PDF p.20 / 印刷 p.14；
    3.0 同节 PDF p.19；§4.3 是 "Units"、§4.2.1 是 "Character string"，**不是**本条所在节）——
    「The value field shall contain a character string describing the physical units in which the
    quantities in the array, **after application of BSCALE and BZERO**, are expressed.」
    （原文经 `fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf` 逐字核验）。
    **该引文支持的与不支持的（两层分开）**：它确立"**带物理单位的是 `BSCALE`/`BZERO` 应用后的物理值**"；
    它**不**规定该物理值的单位名（`BUNIT` 值域任意且为可选关键字），也**不**给出任何数值区间。
    ⇒ **「本仓 ADU 域 = 该物理值域」是项目约定**（约定本身由 `BUNIT='ADU'`/`DATA_SEMANTICS` 承载），
    FITS 标准只提供"物理值 = `BSCALE·样本 + BZERO`"这一等式（§4.4.2.5 Eq. 3）。
    ⇒ **「同标度」的可判定含义 = `BSCALE`/`BZERO` 应用后的物理值处于同一 ADU 域**；
    未经该换算的原始样本（含 XISF 浮点 `[0,1]` 表示）**不**处于 ADU 域（§3 下条）。
  - **实测锚（本仓真实亮场）**：`testdata/Victory_Nebula_T4_Flying_Dutchman/lights/` 的 180 s Lum 帧
    头值 `BITPIX=16`、`BSCALE=1.0`、`BZERO=32768.0`、`EXPTIME=180 s` ⇒ 读入后处于 ADU 域
    （帧身份按 `testdata/` 索引的具名条目给出；观测中位数与 blank-sky 稳健尺度的读数正本已随实验域运行结果归档移除，须由对抗性审核重新推导并构造反例确认）。
  - **量纲与标度类别**：本层输出 `cal` 的标度类别 = **`calibrated_adu`**（ADU，像素域，量纲无 sr 幂）；
    逐帧测光标度 `x′ = α·x`（标度类别 `photo_scaled_adu`）由下游 photometry 节点施加，**本层不施加**；
    HiPS 产品面为 `surface_brightness`（`ADU/sr`）。标度词表、线性标度律与面亮度标度律见
    `docs/engineering/NUMERIC_STANDARD.md`「量纲与标度」节。
- **标度/域声明**：本层（calibration C ABI）是**单位盲**的逐像素算术
  层——入参按 §5 输入合同已经同标度；**把磁盘文件解释成该合同是调用方（io_read/编排）的义务**。
  两类母版文件的解释规则：
  1. **FITS 整数帧**：物理值 = `BSCALE·样本 + BZERO`（FITS 标准 §4.4.2.5 Eq. 3 "physical value =
     BZERO + BSCALE × array value."；16 位相机的
     `BZERO=32768` 伪无符号约定同此）。本仓 `aio` 读路径对非 fpack FITS 恰应用一次，得 [0,65535] ADU。
  2. **XISF 浮点帧**：`Image` 元素的 `sampleFormat="Float32"` 配 `bounds="lower:upper"`
     声明的是**可表示域（黑点:白点，渲染域）**，XISF 1.0 **§11.5.1「Mandatory Image Attributes」**规定
     浮点实型图像 **必须** 带 `bounds`，其语义由 **§8.5.5「Representable Range」**定义
     （"the range of pixel sample values that can be represented on display devices"；无默认域）；它**不携带物理单位**，也不给出「该 [0,1] 对应多少 ADU」的换算因子。
     PixInsight 实践是把原生 16 位整数样本除以 `2¹⁶−1=65535` 归一（PCL 参考实现
     `UInt16PixelTraits::MaxSampleValue()=65535`，`NormalizeSamples` 把 [lower,upper] 映到
     [0, MaxSampleValue]），故此换算因子取 65535——但**必须由调用方显式声明**，而非由文件后缀
     或目录名推断（`eng/contracts/data/phase_product_exchange_matrix.json` R-NO-NAME-BINDING）。
- **标度一致性门（具名拒绝）**：母版与亮场标度不一致（典型：XISF [0,1] 归一化母版 + ADU 亮场）时，
  按原样相减/相除会得到标度错的产物（实测 T2：master_flat median 0.206381 ⇒ 整帧被 ×4.845；
  master_bias 中位 0.015288 ⇒ 本底只减 0.0153 而应减 1001.87 ADU）。消费边界必须
  **声明 + 校验 + fail-closed**（§5/§6/§8、`docs/science/DATA_SEMANTICS.md` §9 DATA-P1-CAL 标度条款）。

## 3a 坐标 frame

校准为逐像素独立算术，**无坐标变换、无 WCS 处理**：输入帧与输出 `cal` 为同一 frame identity（`frame_id` 定义见 `docs/science/DATA_SEMANTICS.md §5`，payload 变化才变）；像素坐标语义沿用词典 `pixel_coordinate`（内部 0-based）。

## 4 输入有效域

- 维度: `w>0, h>0, n_frames>=1`；数组指针非空（空指针返回 `AC_ERR_PARAM`）。
- 母版分组: 按曝光时长/滤镜已由 orchestrator 层分组，calibration 层仅接收已分组母版；`t_light` 在母版覆盖范围内。
- 数值: 输入含 NaN 时该像素的 median/MAD 计算跳过 NaN（`generate_master` 工作缓冲仅纳非 NaN），输出不传播 NaN 为伪有效值（见 §9）。
- 平场: `flat` 可为 NULL（跳过除法）；非 NULL 时 `normalize_flat` 将 median<=0 的平场保持原样不归一（避免除零）。

## 5 连续定义

```text
母版约定（输入合同；顺序与"dark 已减 bias、按曝光比缩放"三点与 IRAF/astropy ccdproc、LSST ip_isr 同向，
            平场归一化口径为本仓 median=1.0，与 ccdproc 的 mean 归一不同；见 §14）:
  master_bias  零曝光本底母版（ADU，与 light 同标度）
  master_dark  已减 bias 的暗电流母版（zero-corrected；ADU，与 light 同标度）
  master_flat  已归一平场（median=1.0；逐像素 floor 0.1 由 calibrate 施加）

dark_opt=0 (默认, 标准式；master_dark 已减 bias):
  cal = (raw − bias·[bias≠NULL] − K·dark·[dark≠NULL]) / max(flat, 0.1)

dark_opt=1 (兼容式, 显式 Bias/Dark 分离；master_dark 含 bias；要求 bias 与 dark 均在位):
  cal = (raw − bias − K·(dark − bias)) / max(flat, 0.1)

K = t_light / t_dark                        # 无量纲曝光比，两分支同一 K

flat_norm = max(flat / median(flat), 0.1)   # median→1.0, 逐像素 floor 0.1
                                            # median<=0 时不归一，保持原样
```

- **floor 0.1 的适用域（冻结，含量纲）**：`flat` 与 `flat_norm` 均为**无量纲相对
  响应场**（`flat_norm` 的 median = 1.0）；`0.1` 是**无量纲响应下界**，语义是
  "平场响应的消费下界 = 中位响应的 10%"。由 `cal = num / max(flat_norm, 0.1)`，
  真实响应 `f < 0.1` 的像素被按 `0.1` 相除 ⇒ 该像素的校准值相对误差为
  `(0.1/f − 1)`，**单边偏低（欠校正）**，且**不可由任何后续标度声明恢复**。
  **实测（驱动生产静态库 `acsd_calibration`，raw = 1000 ADU、bias=dark=NULL、K=1，
  直接调用本仓生产静态库 `acsd_calibration` 的 `ac::calibrate`）**：

  | `f`（已归一响应） | `cal` [ADU] | 真值 `1000/f` [ADU] | 相对误差 |
  |---|---|---|---|
  | 1.0 / 0.5 / 0.2 / **0.1** | 1000 / 2000 / 5000 / **10000** | 同 | **0.00%** |
  | 0.05 | 10000 | 20000 | **−50.00%** |
  | 0.02 | 10000 | 50000 | **−80.00%** |
  | 0.01 | 10000 | 100000 | **−90.00%** |

  ⇒ **适用域 = 平场响应处处 ≥ 0.1·median(flat)**。超出该域（暗角/遮挡/坏平场区
  响应 < 10%）时 floor 生效，产物在这些像素上系统性偏低，**必须**在 provenance
  中标注"floor 生效像素占比"；"平场响应已完整校正"的成立条件 = 该占比为 0。
  同理，ALG-CAL-002 步骤 1/3 的 `max(dst/frame_med, 0.1)` 会把低响应像素**抬**到
  0.1，使母版本身在该区域偏高（实测：直接调用生产 `ac::normalize_flat`，输入 → 输出：
  `{1.0,0.5,0.1,0.05,0.01,2.0}` → `{3.3333,1.6667,0.3333,0.1667,0.1000,6.6667}`，
  响应 0.01 的像素被抬到 0.1 = 10×）。

- `K` 是**暗电流的曝光线性缩放因子**（§6「dark 与曝光线性」），不是增益、不是本底缩放；
  **两个分支都必须施加同一个 `K`**（`calibrator.cpp`：`k = k_init`，
  两分支同一变量、同一回写值）；**`K` 必须由调用方从 FITS `EXPTIME` 得出**
  （`K = t_light/t_dark`），缺 `EXPTIME` 时调用方 fail-closed
  （`module_adapters.cpp`），模块层不设 `K` 默认值、
  不把 `K` 当可优化搜索量（`docs/science/algorithms/CALIBRATION_ALGORITHMS.md` §3.3 F3.1/F3.2）。
- `dark_opt=1` 与 `dark_opt=0` 的唯一差别是**入参 master_dark 的约定**：
  `dark_opt=1` 接受含 bias 的暗场母版（`dark_total = bias + dark`），先分离再缩放；
  `dark_opt=0`（默认）接受已减 bias 的暗电流母版。二者在 `K=1` 时**代数恒等**
  （`(raw−bias)−(dark_total−bias) ≡ raw−dark_total`）；FP32 下是否逐位相等取决于
  数据是否整数可表，**不作为判据**（判据见 §11）。
- `bias` 未提供 ⇒ bias 项为 0（本底不去除）；`dark` 未提供 ⇒ dark 项为 0；
  `flat` 未提供 ⇒ 跳过除法。三者独立可为 NULL。
- 与 `lib/algorithms/calibration/src/calibrator.cpp` 及
  `lib/algorithms/calibration/src/master_generator.cpp` 一致。

## 6 假设

- `bias` 与曝光无关；`dark` 与曝光线性（**两分支**均经 `K=t_light/t_dark` 线性缩放）。
  - **文献支撑**：暗场随曝光的线性（稳健）回归处理见 Hochedez, J.-F. et al. 2013, arXiv.1437（暗场构造对曝光时间的线性拟合/稳健回归实践）；ACS Data Handbook 亦以暗场按曝光时间线性缩放为其基准流程（darkframe = dark·t 缩放约定）。本仓 `K=t_light/t_dark` 与该族约定同向，检验判据见本节可检验形式。
  - **可检验形式（冻结）**：`median(master_dark, t) = b0 + I_d·t`，其中 `I_d > 0` 且拟合残差 ≤ 该组自身噪声量级；
    `b0` 是**与曝光无关的截距**——已减 bias 的母版应给出 `b0 ≈ 0`，含 bias 的 `dark_total` 应给出 `b0 ≈ median(master_bias)`。
  - **非退化要求（冻结）**：线性前提的检验**至少需要 3 个不同曝光档**。2 点拟合必然过两点、残差恒为 0
    ⇒ **2 档时该判据退化、无证据资格**；线性成立的主张以曝光档数 ≥ 3 为成立条件。
  - **真实数据实测**（本仓 `testdata/{T2,T3,T4} calibration files/masterDark_*.xisf` 与 `masterBias_*.xisf`；
    XISF Float32 `bounds="0:1"`，×65535 换到 ADU；逐帧稳健统计与拟合的读数正本已随实验域运行结果归档移除，须由对抗性审核重新推导并构造反例确认）：

    | 组 | 曝光档 [s] | `median(master_dark)` [ADU] | 拟合 `I_d` [ADU/s] | 截距 `b0` [ADU] | 最大残差 [ADU] | `b0 − median(master_bias)` [ADU] |
    |---|---|---|---|---|---|---|
    | T4（4500×3600） | 180 / 300 / 600 | 1097.517 / 1148.033 / 1274.133 | **+0.42048** | 1021.855 | **0.034** | **+105.70**（bias 916.156） |
    | T2（4096×4096） | 600 / 1200 / 1800 | 1008.633 / 1017.300 / **962.750** | **−0.03824** | 1042.111 | 21.072 | +40.24（bias 1001.867） |
    | T3（4096×4096） | 600 / 1200 | 1008.367 / 1015.833 | +0.01244 | 1000.901 | 1.1e-13（2 点，**退化**） | +8.40（bias 992.500） |

    - **T4 组线性成立**（残差 0.034 ADU ≤ 该组噪声量级），但**截距比 `master_bias` 高 105.70 ADU**
      ⇒ T4 的 `master_dark` **不满足 §5 默认约定「已减 bias」**（否则 `b0 ≈ 0`），须走 `dark_opt=1` 且显式声明 `dark_optimization`（§8 表）。
    - **T2 组线性前提被违反**：`median` 非单调（1800 s 档低于 600 s 档），拟合斜率**为负**
      （−0.03824 ADU/s，暗电流物理上不可能为负）⇒ 该组母版的消费路径 = fail-closed 或更换母版（`dark ∝ 曝光` 不适用于该组）。
    - **T3 组不可判定**：仅 2 个曝光档 ⇒ 判据退化（见上「非退化要求」）。
- `flat` 光谱形状与 Light 滤镜匹配（**文献支撑**：平场对科学帧光谱形状的适用性须以标准星测光检验的实践依据见 Marshall, J. L. & DePoy, D. L. 2005, arXiv:astro-ph/0510233——其结论支持"平场误差须在测光检验中量化，不能由平场自身自证"；本仓将该前提的可判定性交给上游滤镜匹配与 §6a 门，正是同一逻辑）。**适用域与可判定性**：该前提**不可**由本层
  逐像素算术机器判定（本层无波长/滤镜信息）；可判定的代理量只有"母版分组按
  滤镜与曝光时长匹配"（orchestrator 侧）与"平场归一化判定带"（§6）。滤镜误配
  的后果是**通带相关的乘性残差**（平场形状错配），其量级必须由测光层
  （`docs/science/PHOTOMETRY.md` 的星等残差）暴露；本层判绿的结论面不含
  光谱形状已匹配；
- **单位一致（机器门）**：`raw/bias/dark/flat` 与 `cal` 同标度、同增益（ADU；§3）。
  母版与亮场标度不一致（如 XISF [0,1] 归一化浮点母版配 ADU 亮场）属**输入合同违背**：
  本层仍不作自动换算（C ABI 单位盲），但**消费边界（io_read/编排）必须显式声明标度并校验，
  违反即 fail-closed**（结构化诊断点名文件 + 观测中位数 + 应声明项；见 §8 表与 `docs/science/DATA_SEMANTICS.md` §9 DATA-P1-CAL 标度条款）。
  声明面：`master_units`（各帧类单位 token）/`master_scale`（到 ADU 的线性换算因子）/
  `master_flat_normalize`（平场是否按 median 归一，枚举 `none`|`median`）；
- **平场已归一（机器门）**：`flat` 约定为 `median≈1.0`（ALG-CAL-002 产物）。
  **平场的标度语义（冻结）**：`flat` 是**无量纲相对响应场**，其**绝对标度不是
  物理量**——`cal = num/flat_norm` 中平场的整体因子被约掉，只有**形状**进入
  结果。因此 `master_flat_normalize="median"`（除以自身中位数）**不是**"用一个
  未知因子掩盖标度错"：对平场而言不存在"真标度"可供对照，归一化正是它的
  定义域转换。**该性质不适用于 bias/dark**——后两者的绝对标度**是**物理量
  （ADU 电平），必须由 `master_units`/`master_scale` 声明并核验（U1/U4）。
  消费边界按 `master_flat_median_range`（冻结默认 `[0.5, 2.0]`，见 `eng/packaging/config/defaults.json`
  `calibration.master_flat_median_range`）判定：中位数落在区间内即视为已归一；区间外**必须**
  显式声明 `master_flat_normalize="median"`（等价于 §5 `flat_norm` 的 `flat/median(flat)`，
  对已归一平场幂等，§7）才允许消费；既不归一又不落区间的整帧**拒绝**（整帧 `1/median` 缩放路径 = 关闭）；
- **坏点分两种形态，可分性判据不同（"坏点稀疏且与天体源不混淆"的前提**只对①成立，对②不成立**）**：
  - **① 稀疏点状缺陷**（热/冷像素、宇宙线斑点）：**连通域大小过滤可用**，但其口径是
    "尺寸上界 + 幅度判据并用"，**不是"尺寸分布不重叠"**。
    量化口径（同一把尺：8 连通、阈值 = 帧内 `median ± 5·MAD`；测量与复跑入口已随实验域运行结果归档移除，须由对抗性审核重新推导并构造反例确认）：
    - 点状缺陷连通域尺寸（真实 T3 母版，n=223241 / 886 / 2192 / 486）：`masterDark`
      hot **99.83% ≤ 4 px**（p50=1、p90=2、max=40071）；`masterDark` cold **68.2% ≤ 4**
      （p50=2、p90=14，**冷像素有成团**）；`masterBias` hot 89.4% ≤ 4；`masterBias` cold **100% ≤ 4**。
    - 天体源连通域尺寸（同一科学帧，阈值 = 背景 + k·σ_bg，σ_bg 由帧内 MAD 定）：
      k=5σ → p50=1、p90=16、p99=99；k=20σ → p50=5、p90=38、p99=194；最大 354854（星云大尺度结构）。
      ⇒ **暗弱源的小尺寸端与点状缺陷重叠**（5σ 门限下 76.7% 的源连通域 ≤ 4 px）。
    因此该类可分性的真实依据是两件事并用：**(a) 幅度判据在母版差分上做**——母版里没有
    天体源，源被构造性排除；**(b) 尺寸过滤只作为兜底**，用于挡掉成团缺陷、陷阱与宇宙线
    拖尾。四家实现同口径：**IRAF** `iraf-community/iraf` main @`b80c8df1`
    `noao/imred/ccdred/ccdmask.par:3-9`（`ncmed=7`、`ncsig=15`、`lsigma=hsigma=6`、`ngood=5`，
    按像素幅度判）、`pkg/xtools/fixpix/xtfp.gx:139-157`；**Siril** @`6284dc9`
    `src/filters/cosmetic_correction.c,545,558`（`.cosme` 逐条记录 `P`/`L`/`C` 缺陷）；
    **LSST DM** `afw` w.2026.39 @`41b6eb5` `include/lsst/afw/image/Defect.h`
    （缺陷 = 显式 bbox 的缺陷表）；**astropy ccdproc** @`0c21068` `ccdproc/core.py`
    （`ccdmask`）。
  - **② 列状缺陷**（坏列/暗列，整列或准整列）：**连通域大小过滤无效**，判据是
    **列统计量的跨列跳变**（`cs[x] = median_y data`；`d[x] = cs[x] − cs[x−1]`；
    `σ_d = 1.482602218505602·MAD(d)`；`J = {x : |d[x] − med_d| ≥ column_sigma·σ_d}`；反号就近配对成段，
    段长 ≤ 2k−1 且不满宽才判坏）。"尺寸面无效"有两条互相独立的实测理由：
    - **大尺寸端重合**：注入整列 +300 ADU ⇒ 该列所在连通域 size=**10601**（帧高 4096），
      而**天体源最大连通域 354854** ⇒ 二者在同一分布的重尾里，尺寸不可区分；
    - **小尺寸端也不成立**：真实坏列 3321 / 1938 在整帧 5σ 门限下沿列的**最长连续超阈段
      只有 51 px / 10 px**（帧高 4096）——列缺陷在整帧阈值下**根本不是**一个连续大连通域，
      尺寸判据既抓不到它，也不能把"被抓到的碎片"与大源碎片区分。
    四家实现同口径：**IRAF** `noao/imred/ccdred/src/t_ccdmask.x:128-130,213`
    （"Sums of pixels along columns are checked at various scales from single pixels to
    whole columns with the sigma level set appropriately"，"Reject over column sums at
    various scales"）+ `ccdmask.par:3-9`；**astropy ccdproc** @`0c21068`
    `ccdproc/core.py`（中值滤波 `medsub` → 逐列求和 `csum.append` →
    `_sigma_mask(csum, csum_sigma, lsigma, hsigma)`）；**Siril** @`6284dc9`
    `src/filters/cosmetic_correction.c`（`C` 记录 = 坏列，整列替换）；
    **LSST DM** `pipe_tasks` w.2026.39 @`e6ec3c74` `repair.py` +
    `meas_algorithms` w.2026.39 @`4a7591d` `src/Interp.cc`（宽缺陷
    `≥ WIDE_DEFECT = 11` 走常数回填）。

## 6a 暗场-亮场曝光容差的科学判据（冻结）

> 上游：`ACSD_DESIGN.md` §4.3（母版标度红线与暗场-亮场曝光容差判定）、§4.5（暗场与亮场曝光差超出容差 = 🟠 warn，不阻塞）。
> **两个判定面必须分开（冻结）**：①**预检面** = `calibration.dark_light_exposure_tolerance`
> （`eng/packaging/config/defaults.json`，值 5、单位 **s**），判定变量是 `|t_light − t_dark|`，
> 结论是 🟠 warn（提示、不阻塞、不参与科学可信判定，`ACSD_DESIGN.md`）；
> ②**科学面** = 本节的 `|K·Δb| ≤ ε·σ_frame`，判定变量是**与曝光差无关的截距失配**，
> 量纲为 ADU，`ε` **无量纲**。两个面的判定变量不相关（下表给出反例），
> **两面各自独立判定**。本节只定义科学面的判据口径。

**误差来源（推导）**：由 §6 的 `master_dark(t_d) = b0 + I_d·t_d` 与 `K = t_light/t_d`，

```text
cal_pipe − cal_true = (t_light/t_d)·(b_light − b0) = −K·Δb          Δb ≡ b0 − b_light
```

- 暗电流项 `I_d·t` 经 `K` 缩放**精确线性**（T4 实测残差 0.034 ADU）⇒ **曝光比 `K` 本身不是暗电流项的误差来源**；
- 唯一误差项是**与曝光无关的截距失配 `Δb` 被乘以 `K`**。`Δb ≡ 0` 时残留恒为 0，**与曝光差无关**。

**判据（冻结）**：

```text
|K·Δb| ≤ ε·σ_frame          ⇒  容差 Δt_max = t_d·(ε·σ_frame/|Δb| − 1)   (t_light > t_d)
```

- `Δb` 由**母版自身**实测：`median(master_dark)` 对曝光档线性拟合的截距 − `median(master_bias)`；
- `σ_frame` 由**亮场自身**实测：blank-sky 稳健尺度；
- `ε` = 容许分数（**无量纲**：容许的 `|K·Δb|` 占亮场稳健尺度 `σ_frame` 的份额）。
  **登记状态**：`eng/packaging/config/defaults.json` 的 `fields[]` 内**没有**承载 `ε` 的项
  （calibration 键只有 s 量纲的曝光容差与无量纲的平场判定带）⇒ `ε` 的数值面**未冻结**，
  须登记冻结后使用；在登记前，本判据**只能**给出"给定 `ε` 下的判定"；**只有登记完成后才**
  "已按冻结默认值判定"。**适用域的另一半**：`ε` 是"容许份额"，因此
  `|Δb| > ε·σ_frame` 时 `Δt_max < 0`——即**连 `Δt = 0` 都不满足判据**，
  该情形必须显式登记为"该组母版按本判据不合格"，而不是取一个更大的 `ε` 使之通过。

**为什么不能只用「曝光差阈值」**：阈值的判定变量是 `|t_light − t_d|`，而误差的判定变量是 `K·Δb`——**二者不相关**。
实测口径（T4 真实母版 + T4 真实 180 s Lum 亮场）给出**反例**：「纯曝光差阈值判定」在
`Δt = 0` 与 `Δt = 5 s` 两处都判 PASS，而这两例的残留 `|K·Δb|` 都超过 `ε·σ_frame`；
`Δb ≡ 0` 的负例在任意 `Δt`、任意 `K` 下残留逐位为 0，却被该阈值判 WARN。
⇒ **阈值判 PASS 的两例残留不为零、判 WARN 的一例残留恰为零** ⇒ 该阈值单独使用**不具备判别力**。
逐项读数（`Δb`、`σ_frame`、`K`、残留与两种判定）正本已随实验域运行结果归档移除，须由对抗性审核重新推导并构造反例确认。

**负例（真值无效应）**：`Δb ≡ 0`（母版暗场截距等于亮场侧 bias 电平）⇒ 残留对**任意** `Δt`、任意 `K`
**逐位为 0**（实测 `(−0.0, −0.0, −0.0)`）⇒ 该情形下任何容差阈值都无科学效应（度量归零）。

**适用域**：本条只适用于「`master_dark` 与亮场经 §5 双分支之一校准」的情形。`Δb` 不可测
（无 `master_bias`，或母版曝光档 < 3 而无法分离截距）时，曝光容差判定**必须**显式降级为「不可判定」并登记，
阈值判定面 = 关闭（该降级即判定结果）。

**证据**：判据推导链、真实数据表与负例的读数、脚本正本，以及母版线性与截距、亮场 `σ_frame` 与 ADU 域的复算入口，已随实验域运行结果归档移除，须由对抗性审核重新推导并构造反例确认。

## 7 独立不变量

- **常量场不变量**：常数输入 `raw=C, dark=D, flat=1.0` 时 `cal = C−D`（`dark_opt=0`）在全帧恒定，无空间调制。
- **空平场不变量**：`flat=NULL` 时退化为减法校准，不引入除法伪影。
- **幂等归一不变量**：对同一 `flat` 连续两次 `normalize_flat` 结果一致（median 已为 1.0，二次归一不变）。
- **确定性不变量**：相同 `raw/bias/dark/flat/K` 输入顺序改变不改变 `cal`（逐像素独立算术，无跨像素归约）。
- **bias 参与不变量**：`dark==NULL` 时，提供 `bias` 与不提供 `bias` 的 `cal` 逐像素差恒为
  `bias / max(flat, 0.1)`（`flat==NULL` 时为 `bias`）；`bias≠0` 时该差必须非零。
- **K 参与不变量**：`dark≠NULL` 时，`cal` 对 `K` 的依赖为 `−K·dark′/max(flat,0.1)`
  （`dark′=dark` 标准式、`dark′=dark−bias` 兼容式）；`K=1` 不是默认值而是
  `t_light==t_dark` 的特例；缺 EXPTIME 时 `K` 的取值 = 显式声明值（`K=1` 的语义 = `t_light==t_dark`）。
- **约定等价不变量**：`K=1` 且 `dark_total` 含 bias 时
  `(raw−bias)−(dark_total−bias) ≡ raw−dark_total`（代数恒等；FP32 逐位性不作判据）。
- **median 鲁棒性**：`generate_master` 在单帧 `n_frames=1` 时直接拷贝，不做 sigma-clip（ `master_generator.cpp: single frame copy`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `w<=0`/`h<=0`/空指针 | 返回 `AC_ERR_PARAM`，不写 `out` | `ac_generate_master_bias` 参数校验 |
| `median(flat)<=0` | `normalize_flat` 不归一，保持原样 | `calibrator.cpp` `if(!(med>0)) return` |
| `flat_norm` 过小 | `max(...,0.1)` floor 避免极大放大 | `calibrator.cpp` |
| `MAD=0` (无离散度) | sigma-clip 提前终止，不再剔除 | `master_generator.cpp: sigma<=0 break` |
| `t_light/t_dark` 极端 | `K` 仍按比值应用，溢出由 FP32 饱和语义界定，不静默 clamp | `calibrate: k=k_init` 直通 |
| `bias` 未提供而 `dark` 在位（标准式） | 本底不去除：`cal = (raw − K·dark)/flat`；运行必须在预检/manifest 显式登记（`optimize`/error 行） | §5；ALG-CAL-003 F3.2 |
| `dark` 未提供而 `bias` 在位 | `cal = (raw − bias)/flat`，`K` 不进入算术 | §5 |
| 母版与亮场标度不一致（含 XISF [0,1] 归一化母版配 ADU 亮场，且未声明标度） | **fail-closed**：消费边界 DATA 拒绝（rc=2），诊断点名文件 + 观测中位数 + 缺失声明项；消费面 = 拒绝 | §3/§6；ALG DISP-CAL-013；`p1_op_calibrate` 前置校验 |
| 母版标度与亮场不一致但**已显式声明** `master_units=normalized` + `master_scale`（如 65535） | 按声明换算到 ADU 后消费；换算因子与声明写入 manifest（可审计） | §3；ALG DISP-CAL-013 |
| 平场母版未归一（`median` 落在 `master_flat_median_range` 外）且未声明 `master_flat_normalize` | **fail-closed**：DATA 拒绝（rc=2），诊断点名文件 + 实测 median + 区间 | §6；ALG DISP-CAL-013 |
| 平场母版未归一但已声明 `master_flat_normalize="median"` | 按 §5 `flat_norm` 归一（幂等）后消费；归一动作写入 manifest | §5/§7 |
| 坏点全帧 | `ac::interpolate_pixels`（经 `ac::correct_frame` 编排）仅修复 `bad_mask=1` 像素，其余不变 | `lib/algorithms/calibration/src/cosmetic_corrector.cpp`（`correct_frame` 编排在其内） |

## 9 精度策略

- 母版算术与校准核心为 FP32（`float`）；FP64 双精度 ABI `*_f64` 在像素级算术路径使用 `double` 不降级（`calibrator.cpp calibrate_d`），其余统计/mask 路径经 `float` 中转（注释已明示）。
- `flat` 除法前 `max(...,0.1)` 保证除数下界 0.1，避免 `1e-7` 量级噪声放大。
- 中位数用 `std::nth_element` O(n)；MAD 转 sigma 系数为唯一全精度写法 `1.482602218505602`（= 1/Φ⁻¹(3/4)，高斯假设；单精度存储时写作 `1.482602218505602f`，即舍入到 `1.4826022f`，相对差 +1.36e-08）。
- 不传播母版方差至 `cal` 的方差项（ivar 由 `snr_estimator` 独立估计，见 NOISE_MODEL）。

## 9a 口径问答：母版标度、曝光比与噪声归属

- **bias/dark/flat/pedestal**：bias=零曝光本底母版、dark=**已减 bias** 的暗电流母版（`dark_opt=1` 兼容形态 `dark_total` 含 bias，见 §2）、flat=像素响应场；本合同**不加 pedestal**
  （`signal` 不自动加 pedestal，见 GLOSSARY/DATA_SEMANTICS §4）。**bias 必须显式进入算术**
  （§5 标准式）：母版 dark 已减 bias 是**默认约定**，此时再忽略 master_bias 会残留整帧本底
   pedestal（bias 电平读数见 ALG-CAL-001 的 `DISP-CAL-012` 登记面）。
- **曝光/gain**：曝光仅以 `K=t_light/t_dark` 比值进入暗场缩放，**不是增益校正**；gain 不在本层建模（非目标 §1），信号保持原 ADU 标度（GLOSSARY `adu`）。
- **read noise**：校准层不建模、不传播；噪声建模归 `snr_estimator`（`docs/science/NOISE_MODEL.md`）。
- **负值**：`raw−dark` 可为负，保留不 clamp（DATA_SEMANTICS §4 负值保留）。
- **saturation**：FP32 饱和语义界定，不静默 clamp（§8 `t_light/t_dark` 行）。
- **mask**：坏点掩膜为 `bad_mask`，极性 **1=坏点**（GLOSSARY `bad_mask`，实现锚 `lib/algorithms/calibration/src/cosmetic_corrector.cpp`）。
- **variance 传播**：本层不传播母版方差至 `cal`（§9）；ivar 由 `snr_estimator` 独立估计，产品位见 DATA_SEMANTICS §4a。

## 10 不可接受变化

- 改变 `flat_norm` 的 `median=1.0` 或 `floor 0.1` 语义而无 SCI 冻结变更；
- `K` 必须取 `t_light/t_dark` 比值；优化搜索值路径不属于本合同；
- 在 `cal` 层引入非线性/增益校正或背景扣除；
- 使 `normalize_flat` 在 `median<=0` 时仍归一导致除零/Inf 传播。

## 11 验证 Oracle

- **解析解**：常数场 `raw/dark/flat` 组合验证公式精确性（`max_abs==0`）。
- **Python 参考**：NumPy 对同一 `raw/bias/dark/flat/K` 的双分支公式逐像素比对，FP32 `rtol=1e-6, atol=1e-7`。
- **不变性门**：常量场、空平场、幂等归一、确定性四门（见 `TST-CAL-INV-*`）。
- **bias 参与门**：同一输入下「提供 master_bias」与「不提供 master_bias」
  的 `cal` **必须不同**（逐像素判据；`dark==NULL` 时差值恒为 `bias/max(flat,0.1)`，
  §7）。凡产物逐位相同即判红——这是「标定输入被静默忽略」的机器可判据。
- **K 参与门**：`dark≠NULL` 时，改 `K` 必须改变 `cal`；
  缺 EXPTIME 时必须 fail-closed 而非静默取 `K=1`。
- **失败注入**：空指针/零维度/NaN 输入显式错误码 `AC_ERR_PARAM`（见 `TST-CAL-FAIL-*`）；
  **ignore-bias 变异注入**：把实现里的 bias 项去掉后，上面两道门必须判红（可执行负例入口）。
- **标度/归一化门判据（U1–U4，载体见 门禁注册面（G08-10 重建））；可执行正负例入口 `--self-test`**：
  以真实数据的独立统计为判据（不依赖文件名/目录名）：
  (a) **单位门（U1）**：亮场观测中位数 > 1 ADU 而某 bias/dark 母版观测中位数 ≤ 1.0 且未声明
      `master_units=normalized` + `master_scale` ⇒ 判红（拒绝）。**适用域与两个已实测盲区
      （冻结；实测方式 = 直接调用 `master_unit_guard.h` 的 `check_master_domain`）**：判据本体是
      `master.median > 1.0 ⇒ 通过`（`lib/include/acsd/core/master_unit_guard.h`），
      且只在 `bias`/`dark` 两类上调用（`module_adapters.cpp`）。故：
      ① **反向混标度不判红**——母版在 ADU 域（median 1001.867）而亮场在 [0,1] 域
      （median 0.05）且两者都未声明 ⇒ 实测**判绿**，随后 `(raw − bias)/flat` 会得到
      量级错的产物；② **两侧同为 [0,1] 域且未声明** ⇒ 实测**判绿**，产物 `cal` 落在
      归一化域而合同标度类别是 `calibrated_adu`。⇒ 本门是**单向门**：只覆盖"母版像
      归一化值、亮场像 ADU 值"这一种不一致。生产链路的其余标度正确性由
      `provenance.units`（硬编码 `["ADU"]`）与人工声明承担；U1 判绿的证据面限于上述单向门，不足以支撑
      推断"亮场与母版已同标度"；
  (b) **平场归一化门（U2）**：`median(flat)` ∉ `master_flat_median_range`（默认 [0.5,2.0]）且未声明
      `master_flat_normalize="median"` ⇒ 判红（拒绝）。**适用域与盲区（冻结）**：判定带
      区分的是"**已归一（median≈1）/ 未归一**"两种数量级状态，**不是**"平场是否可用"。
      实测（直接调用 `check_flat_normalized`）：`median=13525.15`（T2 `masterFlatRed` 换到 ADU）⇒ 判红；
      `median=1.0` ⇒ 判绿；但**物理近零的退化平场（`median=1.5` ADU 量级）同样落在带内
      ⇒ 判绿**——该情形下 `cal = num/1.5` 近似"未做平场校正"，本门**不覆盖**。
      退化平场的拦截归消费边界的整帧退化判定（`module_adapters.cpp`：
      全零 / `median<=0` / 任一非有限像素 ⇒ DATA 拒绝），该判定同样**不覆盖**
      "median 为正但远小于 1"的形态；
  (c) **dark bias 约定门**：提供 `master_dark` 而未显式给出 `dark_optimization`（bool）⇒ 判红（拒绝）；
  (d) **dark bias 约定门**见 (c)：提供 `master_dark` 时该 bool 是**科学输入**（K≠1 时两式
      产物不同）；该 bool 的取值面 = 显式声明。
  (e) **声明自洽门（U4）**：声明 `normalized` 却未给到 ADU 的换算因子、bias/dark 换算因子不一致、
      声明非 ADU 的亮场未给换算 ⇒ 判红（拒绝）——即「**声明本身错走具名拒绝**」；
  (f) **正例对照（必须全绿，防过度拒绝）**：① 同一组归一化母版 + 显式声明（`master_units`/`master_scale`/
      `master_flat_normalize="median"`/`dark_optimization`）⇒ 通过，且校准产物中位数落在 §5 公式的
      ADU 预测值附近。**该一致性判据的量纲与登记面（冻结）**：判据量是
      `median(cal_obs)` 与逐像素 oracle `median[(raw−bias−K·(dark−bias))/flat_norm]` 的差，
      **量纲 = ADU**；因此判据形式必须是**无量纲相对差**
      `|median_obs − median_pred| / max(1 ADU, |median_pred|) ≤ tol`（`tol` 无量纲）。
      **`eng/packaging/config/defaults.json` 内不存在承载 `tol` 的登记项**——该表
      `fields[]` 的 calibration 键只有 `calibration.dark_light_exposure_tolerance`
      （值 5、**单位 s**、语义 = 暗场/亮场**曝光时长**容差）与
      `calibration.master_flat_median_range`（无量纲、平场归一化判定带）；
      **s 量纲的键与 ADU 量纲的一致性容差各自具名**。当前生产门脚本按每例
      1e-2..3e-2 的相对差判定（标度/归一化门判据），
      该数值**未登记**，属待定项：本判据的数值面在登记前，冻结主张的成立条件 = 登记完成。
      T2 NGC1727（曝光 600 s）逐像素 oracle `median[(raw−bias−K·(dark−bias))/flat_norm]` =
      **436.2 ADU**，现行错误实现 7048.6 ADU ⇒ **16.2×**；T4（曝光 180 s）361.2 vs 3650.4 ADU ⇒ 10.1×）；
      ② 母版本就 ADU（观测中位数 > 1）+ 平场本就归一（`median(flat)` ∈ 带内）+ 约定已声明
      ⇒ **无需任何标度声明**即通过（回归锚：门不是「一律拒绝」）。
  四条负例（a/b/c/e）必须能同时判红，两条正例（f①②）必须同时判绿（红→绿对照由
  标度/归一化门判据的 `--self-test` 给出）。

- **暗场线性门（非退化，可执行）**：`median(master_dark)` 对曝光档的线性拟合必须同时满足
  (i) **至少 3 个不同曝光档**（2 档判据退化，判绿条件 = ≥ 3 档）；
  (ii) 斜率 `I_d > 0` 且拟合残差 ≤ 该组自身噪声量级；
  (iii) 截距 `b0` 与 `median(master_bias)` 的一致性决定 `dark_optimization` 分支（§5/§8）。
  任一条不满足 ⇒ 判红或显式降级。**真实数据实证**（§6 表）：T2 组斜率 −0.03824 ADU/s ⇒ 判红；
  T3 组仅 2 档 ⇒ 不可判定（判据退化，实测残差 1.1e-13 = float64 舍入级，**不构成证据**）。
- **曝光容差门（非退化，可执行）**：必须按 §6a 的 `|K·Δb| ≤ ε·σ_frame` 判定，判据输入 = 母版实测 `Δb` + 亮场实测 `σ_frame`。
  **仅按 `|t_light − t_dark|` 判定的门不具备判别力**（§6a 表：判 PASS 的两例残留为 1.83σ / 1.88σ 非零，
  判 WARN 的一例残留逐位为零）；科学判据面 = §6a 的 `|K·Δb| ≤ ε·σ_frame`。
- **标度类别门（可执行）**：`cal` 面与所消费母版的标度类别必须同属 `calibrated_adu`（`docs/engineering/NUMERIC_STANDARD.md`
  标度词表）；标度不可判定 ⇒ 显式拒绝（`rc=2`）。

## 12 关联 ALG ID

- `ALG-CAL-001` MasterBias/Dark 生成（sigma-clip+合并）
- `ALG-CAL-002` MasterFlat 生成（减 Bias→逐帧归一→sigma-clip+mean→再归一）
- `ALG-CAL-003` 单帧校准 `calibrate/calibrate_d`（双分支除法+floor）
- `ALG-CAL-004` 坏点检测/修复 `ac::detect_hot_pixels` / `ac::detect_cold_pixels`（`cosmetic_corrector.cpp` 两处）+ `ac::interpolate_pixels`（同文件）；C ABI 面 = `ac_correct_frame` / `ac_correct_columns` / `ac_detect_bad_columns_from_master` / `ac_column_variance_inflate`（均在 `astro_calibration.h`）。`cc_*` 命名（`cc_correct_median` / `cc_detect_hot` / `cc_detect_cold`）属**已退役通道**，不在任何 CMake 目标内，不再作在役证据引用

## 13 追溯与测试

- 权威文件: `docs/science/CALIBRATION.md` (SCI-CAL-001)
- 实现: `lib/algorithms/calibration/src/calibrator.cpp` (`normalize_flat, calibrate, calibrate_d`), `lib/algorithms/calibration/src/master_generator.cpp` (`generate_master`), `lib/algorithms/calibration/src/cosmetic_corrector.cpp`
- 公开 API: `lib/algorithms/calibration/include/astro_calibration.h`（`ac_generate_master_bias/dark/flat`、`ac_calibrate_frame`、`ac_correct_frame` 及其 `_f64` 变体；坏列家族 6 导出：`ac_correct_columns`、`ac_detect_bad_columns_from_master`、`ac_correct_columns_ex`、`ac_correct_columns_ex2`、`ac_column_variance_inflate`、`ac_correct_columns_f64`）
- 测试: `TST-CAL-001` 常量场、`TST-CAL-INV-001` 幂等归一、`TST-CAL-FAIL-001` 参数校验（映射见 `docs/engineering/TRACEABILITY_SPEC.md §10`）

## 14 Primary literature（引用均已核对原文定位）

1. Newberry, M. V. 1991, PASP, 103, 122, "Signal-to-Noise Considerations for Sky-Subtracted CCD Data"（DOI 10.1086/132801）。定位：全文 S/N 模型含 bias/dark/read-noise 分量。**本合同不引用其具体公式号**（原文公式映射未在本次核验范围内逐式确认）；§5 连续定义为 Project-defined derivation，文献仅作概念上下文；本合同 §5 的权威面 = Project-defined derivation。
2. Janesick, J. R. 2001, *Scientific Charge-Coupled Devices*, SPIE Press Monograph PM83（ISBN 0-8194-3698-4），Ch.2 photon transfer（gain/read-noise 测量上下文；Ch.2 定位经 Janesick et al. 2004 EM-CCD 论文二次引用核对）。
3. HST ACS Data Handbook §4.4 "Flat-Field Reference Files"（<https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-4-flat-field-reference-files>；URL 即节定位）：flat-field=像素响应校正、P-flat 结构与低频修正分离的实践上下文。
4. **ccdproc (astropy affiliated package), "Reduction toolbox"**（<https://ccdproc.readthedocs.io/en/latest/reduction_toolbox.html>）：原文（逐字核验）"Assume in this section that you have created a master bias image called master_bias and a master dark image called master_dark **that has been bias-subtracted** so that it can be scaled by exposure time if necessary."；`subtract_dark` API 文档（<https://ccdproc.readthedocs.io/en/latest/api/ccdproc.subtract_dark.html>）参数 `dark_exposure`/`data_exposure`/`scale` 确实存在（逐字核验）。
**该引文支持的与不支持的（两层分开）**：所引那一句只支持"**master_dark 是已减 bias 的暗电流母版**"与"**可按曝光时间缩放**"两件事；它**不**支持归约**顺序**。顺序的证据是同一页的小节顺序与示例（trimming/overscan → `subtract_bias` → `subtract_dark(..., scale=True)` → `flat_correct`），须按此引用。
**差异声明（两口径各自具名，不与 ccdproc 混同）**：ccdproc 的 `flat_correct` 默认按**均值**归一（`norm_value` 缺省 = 平场均值）后相除，本仓冻结的是 **median = 1.0**；二者在平场分布偏斜时给出不同结果。
**据此冻结：master_dark 是"已减 bias 的暗电流母版"，bias 必须显式减除，dark 按曝光比缩放，flat 最后除。**
5. **LSST Science Pipelines `lsst.ip.isr`**（ISR = instrument signature removal；仓库 <https://github.com/lsst/ip_isr>，**版本腿 = commit `28faec7dd2297d2ff9f108e543b2d55fdb046345`**，该仓只发 `w.YYYY.WW` 周版本、无 release tag；<https://pipelines.lsst.io/modules/lsst.ip.isr/index.html>）：模块自述 "corrections for overscans, crosstalk, **bias and dark frames**"；源码 `python/lsst/ip/isr/isrFunctions.py`（钉到该 commit 的不可变 URL：<https://raw.githubusercontent.com/lsst/ip_isr/28faec7dd2297d2ff9f108e543b2d55fdb046345/python/lsst/ip/isr/isrFunctions.py>）：`biasCorrection` 执行 `maskedImage -= biasMaskedImage`，`darkCorrection(maskedImage, darkMaskedImage, expScale, darkScale, ...)` 的 Notes 逐字为 "The dark correction is applied by calculating: maskedImage -= dark * expScaling / darkScaling"（即 `−dark·t_light/t_dark`），`flatCorrection` 执行除法，且其**函数**层面 flat 标度取自数据（`scalingType` MEAN/MEDIAN/USER）；`isrTask.py` 的处理顺序清单（引入语为 "The steps with debug points are:"）为 doBias → doCrosstalk → doBrighterFatter → **doDark** → doFringe → doStrayLight → **doFlat**（该清单与 `IsrTask.run` 的实际执行点顺序一致）。
**该引文支持的与不支持的（两层分开）**：所引原文均逐字成立（`isrFunctions.py` 的 `biasCorrection`/`darkCorrection` Notes/`flatCorrection`，`isrTask.py` 的 `flatScalingType` 与顺序清单，commit `28faec7dd2297d2ff9f108e543b2d55fdb046345`）。限定三条：① "flatCorrection 不假设已归一"只对**该函数**成立——`IsrTask` 的**默认** `flatScalingType='USER'` 配 `flatUserScale=1.0` ⇒ **默认流水线确实假设平场已归一**，与本仓 `median≈1.0` 约定同向；② LSST 的暗场缩放因子取自 `getDarkTime()`（暗电流时间），**不是** `EXPTIME` 直接相除，与本仓 `K=t_light/t_dark` 是**同向但不等价**的口径；③ `fringeAfterFlat=True` 时 fringe 排在 flat 之后。
**据此冻结：bias → dark(×K) → flat 的顺序与 K 的物理含义（曝光比）。**
6. **XISF Version 1.0 Specification**（PixInsight/Pleiades Astrophoto；<http://pixinsight.com/xisf/xisf-1.0.xsd> 为随规范发布的 XML Schema）：**§11.5.1「Mandatory Image Attributes」**——"This attribute shall be specified for all Image elements serializing floating point real pixel data. … The bounds attribute defines the representable range of a real or integer image … `lower` … the black point … `upper` … the white point"；**§8.5.5「Representable Range」**——"There is no default representable range for real images whose pixel samples are encoded as floating point scalars, so in these cases the representable range must be declared explicitly"，且该节把可表示域定义为"可在显示设备上表示的像素样本值范围"；整数图像默认可表示域 `[0, 2ⁿ−1]`。`FITSKeyword`（§11.6）只是 FITS 兼容元数据层：**像素样本域的规范力来自 §8.5.5/§11.5.1 的 `bounds`，不来自 `FITSKeyword`**（§11.6 允许把 `BUNIT` 存为 FITS 关键字，故本条只限定其规范力，不限定其可存性）。**据此冻结：XISF 浮点 `bounds="0:1"` 是渲染可表示域，不是 ADU；ADU 换算因子必须由调用方声明。**
7. **PCL（PixInsight Class Library）参考实现 `src/pcl/XISFReader.cpp`**（仓库 <https://gitlab.com/pixinsight/PCL>；**版本腿 = master HEAD commit `5a3902196a7d7a701385a7113cbdce2976ae1a85`**——该仓 `git ls-remote --tags` 实测 **tag 数 = 0**，故版本钉到 commit；该文件头横幅逐字自述 "PCL 2.11.0" 与 "Released 2026-09-27T21:02:13Z"；许可证 `LICENSE` 逐字为 "PixInsight Class Library License Version 2.0.1, 29 December 2025"；钉到该 commit 的不可变 URL：<https://gitlab.com/pixinsight/PCL/-/raw/5a3902196a7d7a701385a7113cbdce2976ae1a85/src/pcl/XISFReader.cpp>）：`NormalizeSamples`/`NORMALIZE_FLOAT_IMAGE` 把文件可表示域线性映射到目标类型域——浮点目标 `*i=(*i−lower)/range`（→[0,1]），整数目标 `*i=(*i−lower)·MaxSampleValue/range`（→[0,2ⁿ−1]）；`UInt16` 的 `MaxSampleValue()=65535`。同一物理数据在 Float32 [0,1] 与 UInt16 [0,65535] 两种表示间的换算因子即 **65535**。**据此冻结：XISF Float32 `bounds="0:1"` 母版换算到 16 位 ADU 的声明因子取 65535（声明制，非推断制）。**
8. **FITS Standard 4.0 §4.4.2.5**（`BSCALE`/`BZERO`：Eq. 3 "physical value = BZERO + BSCALE × array value."；16 位相机的 `BZERO=32768` 见同节 BLANK 段 "…by setting BZERO = 32768 and BSCALE = 1" 与 Table 11，属**存储约定**（把有符号 16 位字段当无符号用），**不是**标准强制值；3.0 版同节 PDF p.19）。本仓真实亮场（T2/T4）实测 `BITPIX=16, BZERO=32768`，读入域 [0,65535] ADU——这就是亮场一侧的"ADU 域"定义。**前置条件**：该 [0,65535] 结论依赖 `BSCALE=1 ∧ BZERO=32768` 同时成立；其他位深或关键字组合下读入域随之改变；其他组合的读入域须重新判定。
9. IRAF `ccdred` 家族（NOAO/IRAF：zero → dark → flat 的经典归约顺序，dark 帧先在 zero 校正后合并）。**源码级佐证（上游官方源码树 `iraf-community/iraf`（提交 `b80c8df1`）本地完整镜像逐字核验）**：`noao/imred/ccdred/zerocombine.cl`——zero 档组合任务，缺省 `combine="average"`、`reject="minmax"`、`nlow=0`/`nhigh=1`、`scale="none"`；`noao/imred/ccdred/darkcombine.cl`——dark 档组合任务，缺省 `reject="minmax"`、`scale="exposure"`（dark 按曝光时长缩放）且 `process=yes`（合并前先过 `ccdproc`，即先做 zero 校正，印证「dark 先在 zero 校正后合并」）；两任务共用 `noao/imred/ccdred/combine.par` 参数文件（`combine=average`、`reject` 值域 `none|minmax|ccdclip|crreject|sigclip|avsigclip|pclip`、`lsigma=3.`/`hsigma=3.`、`nlow=1`/`nhigh=1`、`nkeep=1`、`mclip=yes`、`sigscale=0.1`、`pclip=-0.5`）；坏像元掩膜任务参数文件 `noao/imred/ccdred/ccdmask.par`（`ncmed=7`、`ncsig=15`、`lsigma=6.`/`hsigma=6.`、`ngood=5`）。本仓母版约定的冻结判据以第 4、5 条为准，IRAF 家族为经典顺序、dark 曝光缩放与坏像元掩膜语义的源码级佐证。

## 14a 参考文献与参考代码库（含许可证）


- **母版约定（master_dark 已减 bias、dark 按曝光比缩放、flat 最后除）**：ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc，tag 2.5.1）`ccdproc/core.py` 的 `subtract_dark` / `flat_correct`（reduction_toolbox 对应面）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr，commit `28faec7d`）`python/lsst/ip/isr/isrFunctions.py` 的 `biasCorrection` / `darkCorrection` / `flatCorrection`。仅行为对照。
- **探测器噪声/gain/read noise**：Janesick 2001（SPIE PM83）Ch.2；Newberry 1991 PASP 103, 122；Howell, S. B. 2006, Handbook of CCD Astronomy, 2nd ed., CUP（ISBN 978-0-521-85215-9）Ch.4。
- **MAD→σ 常数 1.482602218505602 = 1/Φ⁻¹(3/4)**：标准正态分位恒等式（教科书级；
  实测 `1/0.6744897501960817 = 1.482602218505602`，16 位十进制逐位相同；
  `float32` 舍入为 `1.4826022386550903`，相对差 **+1.359062e-08**）。
  **本仓口径（冻结）**：使用**渐近常数**，**不做** MAD 的有限样本偏差校正。
  **归属（两层核验结论）**：Rousseeuw, P. J. & Croux, C. 1993, JASA **88(424), 1273–1283**
  （DOI 10.1080/01621459.1993.10476408）——卷页核验成立（Crossref + OpenAlex 双源），
  出版方摘要逐字含 `MAD_n = 1.4826 med_i{|x_i − med_j x_j|}`（`1.4826` 为该摘要的 4 位截断展示；权威全精度 `1.482602218505602`）；但该文构造并研究的是
  **S_n / Q_n** 两个估计量及其有限样本偏差校正的**粗糙近似**，文中把 `1.4826·MAD`（4 位截断展示；权威全精度 `1.482602218505602`）当作
  **既有对照基线**引用。⇒ **该文的引用面 = 既有对照基线**；
  若将来要做 MAD 的有限样本校正，来源应为 Akinshin 2022（arXiv.12005 与
  arXiv.12268）或 Park, Kim & Wang 2020（DOI 10.1080/03610918.2019.1699114）。
  **核验状态**：卷页与摘要逐字已核；R&C 正文（付费墙，Unpaywall `is_oa=false`、出版商 403）
  **未能取到**，上列"研究 S_n/Q_n"结论来自其摘要逐字 + 二次文献（Akinshin）明述。
- **flat-field 像素响应与低频边界**：HST ACS Data Handbook §4.4；IRAF ccdred 家族归约顺序（IRAF/NOAO 许可，非 OSI 开源）。
- **FITS BSCALE/BZERO 与伪无符号**：FITS Standard 4.0 §4.4.2.5（Eq. 3 + BLANK 段 + Table 11）；CFITSIO（CFITSIO 宽松许可，NASA/HEASARC）作独立读取器 Oracle。
- **XISF bounds 与 65535 换算**：XISF 1.0 Spec（PixInsight；PCL = PixInsight Class Library License **2.0.1**（2025-12-29），`LICENSE` 逐字原文；BSD-3-Clause 型 + 强制署名条款，非 OSI 认证；https://gitlab.com/pixinsight/PCL，版本 commit `5a3902196a7d7a701385a7113cbdce2976ae1a85`，文件位置 `src/pcl/XISFReader.cpp` 的 `NormalizeSamples`）。**只作声明制换算因子的取证来源，不复制**。
- **母版方差传播（当前缺口，登记 UNRESOLVED）**：本层 §9 的冻结口径是
  **不传播母版方差至 `cal`**：`cal` 只承载校准后的信号值，其不确定度由
  `snr_estimator` 独立估计（§1 非目标、§9a variance 传播）。**项目内不存在**
  要求 master calibration 参数不确定度进入 variance/covariance 的冻结条文——
  `docs/detail/UNIFIED_MODEL.md`（§1–§3）无此条文。⇒ 现状是**口径缺口而非条文冲突**：母版由有限
  帧数合并产生，其自身不确定度（`σ²/N_master`）与**与科学帧共享的相关系统项**
  （平场形状误差、暗电流标度误差）在 `cal` 面与下游 variance 中都未建模，
  下游据此得到的 SNR 在母版主导的系统项上偏乐观。
  **适用域**：该缺口在"母版帧数少 / 平场形状误差大"时影响显著；在母版帧数
  充足且平场形状良好的数据上可忽略。建模方案（低秩 covariance/provenance）
  **无项目内冻结公式**，属待定项；结论面 = 待定项登记。
  可对照 ccdproc / LSST ip_isr（二者同样不传播母版方差，属行业普遍简化）
  与 Howell 2006 Ch.4 的校准误差预算讨论。**证据面不足，结论面 = 待定项登记。**

参考代码库（含许可证）正本 = docs/engineering/SCIENTIFIC_REFERENCES.md §M。

## 15 Acceptance

- §11 Oracle 全过（解析解 max_abs==0、NumPy FP32 rtol=1e-6/atol=1e-7）；
- §7 四不变量门（常量场/空平场/幂等归一/确定性）全过；
- §11 **bias 参与门**与 **K 参与门**全过，且 ignore-bias 变异注入可判红；
- §11 **标度/归一化门**四条负例（单位混用 / 平场未归一 / dark bias 约定未声明 / 声明自洽）与
  两条正例（显式声明组合 / 本就合规组合=防过度拒绝）全过，
  且负例为**真实二进制端到端**判红（标度/归一化门判据 `--self-test`）；
- §11 **暗场线性门**（≥3 曝光档 + 斜率正 + 残差 ≤ 噪声量级）与 **曝光容差门**（`|K·Δb| ≤ ε·σ_frame`）
  在真实母版上可判：T4 组判绿（残差 0.034 ADU）、T2 组判红（斜率 −0.03824 ADU/s）、T3 组判「不可判定」；
  判据的负例（`Δb ≡ 0`）残留逐位为 0，非退化性由 §6a 表给出；
- 单位经术语权威校验项（GLOSSARY_PASS，载体见 门禁注册面（G08-10 重建））且本文件无被禁 alias；
- §9a 专属问题逐项有锚点回答，无 TBD/二选一（`eng/tools/science_contract_lint.py` PASS）；
- 解析不变量可转 SYN-001：常量场→SYN-001 constant/ramp 用例；NaN/饱和→SYN-001 invalid 边界用例（映射登记于 SYN-001 任务）。
