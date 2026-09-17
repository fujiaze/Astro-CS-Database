# Calibration Science (SCI-CAL)

> ID: SCI-CAL-001  状态: FROZEN (T100 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-CAL-001..  模块: calibration

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
- **标度/域声明（UNIT-001 冻结，2026-09-17）**：本层（calibration C ABI）是**单位盲**的逐像素算术
  层——入参按 §5 输入合同已经同标度；**把磁盘文件解释成该合同是调用方（io_read/编排）的义务**。
  两类母版文件的解释规则：
  1. **FITS 整数帧**：物理值 = `BSCALE·样本 + BZERO`（FITS 标准 §4.2.1/§4.3；16 位相机的
     `BZERO=32768` 伪无符号约定同此）。本仓 `aio` 读路径对非 fpack FITS 恰应用一次，得 [0,65535] ADU。
  2. **XISF 浮点帧**：`Image` 元素的 `sampleFormat="Float32"` 配 `bounds="lower:upper"`
     声明的是**可表示域（黑点:白点，渲染域）**，XISF 1.0 §11.5 规定浮点实型图像 **必须** 带
     `bounds`（无默认域）；它**不携带物理单位**，也不给出「该 [0,1] 对应多少 ADU」的换算因子。
     PixInsight 实践是把原生 16 位整数样本除以 `2¹⁶−1=65535` 归一（PCL 参考实现
     `UInt16PixelTraits::MaxSampleValue()=65535`，`NormalizeSamples` 把 [lower,upper] 映到
     [0, MaxSampleValue]），故此换算因子取 65535——但**必须由调用方显式声明**，不得由文件后缀
     或目录名推断（`contracts/data/phase_product_exchange_matrix.json` R-NO-NAME-BINDING）。
- **禁止静默混标度**：母版与亮场标度不一致（典型：XISF [0,1] 归一化母版 + ADU 亮场）时，
  按原样相减/相除会得到标度错的产物（实测 T2：master_flat median 0.206381 ⇒ 整帧被 ×4.845；
  master_bias 中位 0.015288 ⇒ 本底只减 0.0153 而应减 1001.87 ADU）。消费边界必须
  **声明 + 校验 + fail-closed**（§5/§6/§8、DATA-P1-CAL §9.1）。

## 3a 坐标 frame

校准为逐像素独立算术，**无坐标变换、无 WCS 处理**：输入帧与输出 `cal` 为同一 frame identity（`frame_id` 定义见 `docs/contracts/DATA_SEMANTICS.md#5`，payload 变化才变）；像素坐标语义沿用词典 `pixel_coordinate`（内部 0-based）。

## 4 输入有效域

- 维度: `w>0, h>0, n_frames>=1`；数组指针非空（空指针返回 `AC_ERR_PARAM`）。
- 母版分组: 按曝光时长/滤镜已由 orchestrator 层分组，calibration 层仅接收已分组母版；`t_light` 在母版覆盖范围内。
- 数值: 输入含 NaN 时该像素的 median/MAD 计算跳过 NaN（`generate_master` 工作缓冲仅纳非 NaN），输出不传播 NaN 为伪有效值（见 §9）。
- 平场: `flat` 可为 NULL（跳过除法）；非 NULL 时 `normalize_flat` 将 median<=0 的平场保持原样不归一（避免除零）。

## 5 连续定义

```text
母版约定（输入合同；与 IRAF ccdproc / LSST ip_isr / astropy ccdproc 一致，见 §14）:
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

- `K` 是**暗电流的曝光线性缩放因子**（§6「dark 与曝光线性」），不是增益、不是本底缩放；
  **两个分支都必须施加 K**（旧实现只在 `dark_opt=1` 施加、标准分支强制 `k=1.0`，
  属缺陷，登记 ALG-CAL-001 DISP-CAL-012）。
- `dark_opt=1` 与 `dark_opt=0` 的唯一差别是**入参 master_dark 的约定**：
  `dark_opt=1` 接受含 bias 的暗场母版（`dark_total = bias + dark`），先分离再缩放；
  `dark_opt=0`（默认）接受已减 bias 的暗电流母版。二者在 `K=1` 时**代数恒等**
  （`(raw−bias)−(dark_total−bias) ≡ raw−dark_total`）；FP32 下是否逐位相等取决于
  数据是否整数可表，**不作为判据**（判据见 §11）。
- `bias` 未提供 ⇒ bias 项为 0（本底不去除）；`dark` 未提供 ⇒ dark 项为 0；
  `flat` 未提供 ⇒ 跳过除法。三者独立可为 NULL。
- 与 `lib/algorithms/calibration/src/calibrator.cpp:113-145,157-189` 及
  `lib/algorithms/calibration/src/master_generator.cpp:243-255` 一致。

## 6 假设

- `bias` 与曝光无关；`dark` 与曝光线性（**两分支**均经 `K=t_light/t_dark` 线性缩放）；
- `flat` 光谱形状与 Light 滤镜匹配；
- **单位一致（UNIT-001 起为机器门）**：`raw/bias/dark/flat` 与 `cal` 同标度、同增益（ADU；§3）。
  母版与亮场标度不一致（如 XISF [0,1] 归一化浮点母版配 ADU 亮场）属**输入合同违背**：
  本层仍不作自动换算（C ABI 单位盲），但**消费边界（io_read/编排）必须显式声明标度并校验，
  违反即 fail-closed**（结构化诊断点名文件 + 观测中位数 + 应声明项；见 §8 表与 DATA-P1-CAL §9.1）。
  声明面：`master_units`（各帧类单位 token）/`master_scale`（到 ADU 的线性换算因子）/
  `master_flat_normalize`（平场是否按 median 归一，枚举 `none`|`median`）；
- **平场已归一（UNIT-001 起为机器门）**：`flat` 约定为 `median≈1.0`（ALG-CAL-002 产物）。
  消费边界按 `master_flat_median_range`（冻结默认 `[0.5, 2.0]`，见 `config/defaults.json`
  `calibration.master_flat_median_range`）判定：中位数落在区间内即视为已归一；区间外**必须**
  显式声明 `master_flat_normalize="median"`（等价于 §5 `flat_norm` 的 `flat/median(flat)`，
  对已归一平场幂等，§7）才允许消费；既不归一又不落区间的整帧**拒绝**（禁止整帧被 `1/median` 静默缩放）；
- 坏点稀疏且与天体源不混淆（连通域大小过滤可分离）。

## 7 独立不变量

- **常量场不变量**：常数输入 `raw=C, dark=D, flat=1.0` 时 `cal = C−D`（`dark_opt=0`）在全帧恒定，无空间调制。
- **空平场不变量**：`flat=NULL` 时退化为减法校准，不引入除法伪影。
- **幂等归一不变量**：对同一 `flat` 连续两次 `normalize_flat` 结果一致（median 已为 1.0，二次归一不变）。
- **确定性不变量**：相同 `raw/bias/dark/flat/K` 输入顺序改变不改变 `cal`（逐像素独立算术，无跨像素归约）。
- **bias 参与不变量**：`dark==NULL` 时，提供 `bias` 与不提供 `bias` 的 `cal` 逐像素差恒为
  `bias / max(flat, 0.1)`（`flat==NULL` 时为 `bias`）；`bias≠0` 时该差必须非零。
- **K 参与不变量**：`dark≠NULL` 时，`cal` 对 `K` 的依赖为 `−K·dark′/max(flat,0.1)`
  （`dark′=dark` 标准式、`dark′=dark−bias` 兼容式）；`K=1` 不是默认值而是
  `t_light==t_dark` 的特例，缺 EXPTIME 时不得静默取 1。
- **约定等价不变量**：`K=1` 且 `dark_total` 含 bias 时
  `(raw−bias)−(dark_total−bias) ≡ raw−dark_total`（代数恒等；FP32 逐位性不作判据）。
- **median 鲁棒性**：`generate_master` 在单帧 `n_frames=1` 时直接拷贝，不做 sigma-clip（ `master_generator.cpp: single frame copy`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `w<=0`/`h<=0`/空指针 | 返回 `AC_ERR_PARAM`，不写 `out` | `ac_generate_master_bias` 参数校验 |
| `median(flat)<=0` | `normalize_flat` 不归一，保持原样 | `calibrator.cpp:91 if(!(med>0)) return` |
| `flat_norm` 过小 | `max(...,0.1)` floor 避免极大放大 | `calibrator.cpp:85-100,129,139,173,185` |
| `MAD=0` (无离散度) | sigma-clip 提前终止，不再剔除 | `master_generator.cpp: sigma<=0 break` |
| `t_light/t_dark` 极端 | `K` 仍按比值应用，溢出由 FP32 饱和语义界定，不静默 clamp | `calibrate: k=k_init` 直通 |
| `bias` 未提供而 `dark` 在位（标准式） | 本底不去除：`cal = (raw − K·dark)/flat`；运行必须在预检/manifest 显式登记（`optimize`/error 行） | §5；ALG-CAL-003 F3.2 |
| `dark` 未提供而 `bias` 在位 | `cal = (raw − bias)/flat`，`K` 不进入算术 | §5 |
| 母版与亮场标度不一致（含 XISF [0,1] 归一化母版配 ADU 亮场，且未声明标度） | **UNIT-001 起 fail-closed**：消费边界 DATA 拒绝（rc=2），诊断点名文件 + 观测中位数 + 缺失声明项；禁止按原样消费 | §3/§6；ALG DISP-CAL-013；`p1_op_calibrate` 前置校验 |
| 母版标度与亮场不一致但**已显式声明** `master_units=normalized` + `master_scale`（如 65535） | 按声明换算到 ADU 后消费；换算因子与声明写入 manifest（可审计） | §3；ALG DISP-CAL-013 |
| 平场母版未归一（`median` 落在 `master_flat_median_range` 外）且未声明 `master_flat_normalize` | **UNIT-001 起 fail-closed**：DATA 拒绝（rc=2），诊断点名文件 + 实测 median + 区间 | §6；ALG DISP-CAL-013 |
| 平场母版未归一但已声明 `master_flat_normalize="median"` | 按 §5 `flat_norm` 归一（幂等）后消费；归一动作写入 manifest | §5/§7 |
| 坏点全帧 | `cc_correct_median` 仅修复 `bad_mask=1` 像素，其余不变 | `cosmetic_corrector.cpp` |

## 9 精度策略

- 母版算术与校准核心为 FP32（`float`）；FP64 双精度 ABI `*_f64` 在像素级算术路径使用 `double` 不降级（`calibrator.cpp:157-189 calibrate_d`），其余统计/mask 路径经 `float` 中转（注释已明示）。
- `flat` 除法前 `max(...,0.1)` 保证除数下界 0.1，避免 `1e-7` 量级噪声放大。
- 中位数用 `std::nth_element` O(n)；MAD 转 sigma 系数为唯一全精度写法 `1.482602218505602`（= 1/Φ⁻¹(3/4)，高斯假设；单精度存储时写作 `1.482602218505602f`，即舍入到 `1.4826022f`，相对差 +1.36e-08）。
- 不传播母版方差至 `cal` 的方差项（ivar 由 `snr_estimator` 独立估计，见 NOISE_MODEL）。

## 9a 专属问题回答（SCI-001 指定问题逐项）

- **bias/dark/flat/pedestal**：bias=零曝光本底母版、dark=**已减 bias** 的暗电流母版（`dark_opt=1` 兼容形态 `dark_total` 含 bias，见 §2）、flat=像素响应场；本合同**不加 pedestal**
  （`signal` 不自动加 pedestal，见 GLOSSARY/DATA_SEMANTICS §4）。**bias 必须显式进入算术**
  （§5 标准式）：母版 dark 已减 bias 是**默认约定**，此时再忽略 master_bias 会残留整帧本底
   pedestal（实测 T2：bias ≈ 1001.9 ADU，见 ALG-CAL-001 DISP-CAL-012 证据）。
- **曝光/gain**：曝光仅以 `K=t_light/t_dark` 比值进入暗场缩放，**不是增益校正**；gain 不在本层建模（非目标 §1），信号保持原 ADU 标度（GLOSSARY `adu`）。
- **read noise**：校准层不建模、不传播；噪声建模归 `snr_estimator`（`docs/science/NOISE_MODEL.md`）。
- **负值**：`raw−dark` 可为负，保留不 clamp（DATA_SEMANTICS §4 负值保留）。
- **saturation**：FP32 饱和语义界定，不静默 clamp（§8 `t_light/t_dark` 行）。
- **mask**：坏点掩膜为 `bad_mask`，极性 **1=坏点**（GLOSSARY `bad_mask`，实测 `lib/algorithms/calibration/src/cosmetic_corrector.cpp#158`）。
- **variance 传播**：本层不传播母版方差至 `cal`（§9）；ivar 由 `snr_estimator` 独立估计，产品位见 DATA_SEMANTICS §4a。

## 10 不可接受变化

- 改变 `flat_norm` 的 `median=1.0` 或 `floor 0.1` 语义而无 SCI 冻结变更；
- 将 `K` 改为优化搜索值而非 `t_light/t_dark` 比值；
- 在 `cal` 层引入非线性/增益校正或背景扣除；
- 使 `normalize_flat` 在 `median<=0` 时仍归一导致除零/Inf 传播。

## 11 验证 Oracle

- **解析解**：常数场 `raw/dark/flat` 组合验证公式精确性（`max_abs==0`）。
- **Python 参考**：NumPy 对同一 `raw/bias/dark/flat/K` 的双分支公式逐像素比对，FP32 `rtol=1e-6, atol=1e-7`。
- **不变性门**：常量场、空平场、幂等归一、确定性四门（见 `TST-CAL-INV-*`）。
- **bias 参与门（新增，BIAS-001）**：同一输入下「提供 master_bias」与「不提供 master_bias」
  的 `cal` **必须不同**（逐像素判据；`dark==NULL` 时差值恒为 `bias/max(flat,0.1)`，
  §7）。凡产物逐位相同即判红——这是「标定输入被静默忽略」的机器可判据。
- **K 参与门（新增，BIAS-001）**：`dark≠NULL` 时，改 `K` 必须改变 `cal`；
  缺 EXPTIME 时必须 fail-closed 而非静默取 `K=1`。
- **失败注入**：空指针/零维度/NaN 输入显式错误码 `AC_ERR_PARAM`（见 `TST-CAL-FAIL-*`）；
  **ignore-bias 变异注入**：把实现里的 bias 项去掉后，上面两道门必须判红（可执行负例入口）。
- **标度/归一化门（新增，UNIT-001；可执行正负例入口 `tools/quality/check_master_unit_guard.py --self-test`）**：
  以真实数据的独立统计为判据（不依赖文件名/目录名）：
  (a) **单位门**：亮场观测中位数 > 1 ADU 而某 bias/dark 母版观测中位数 ≤ 1.0 且未声明
      `master_units=normalized` + `master_scale` ⇒ 判红（拒绝）；
  (b) **平场归一化门**：`median(flat)` ∉ `master_flat_median_range`（默认 [0.5,2.0]）且未声明
      `master_flat_normalize="median"` ⇒ 判红（拒绝）；
  (c) **dark bias 约定门**：提供 `master_dark` 而未显式给出 `dark_optimization`（bool）⇒ 判红（拒绝）；
  (d) **dark bias 约定门**见 (c)：提供 `master_dark` 时该 bool 是**科学输入**（K≠1 时两式
      产物不同），不得有隐含默认；
  (e) **声明自洽门（U4）**：声明 `normalized` 却未给到 ADU 的换算因子、bias/dark 换算因子不一致、
      声明非 ADU 的亮场未给换算 ⇒ 判红（拒绝）——即「**声明本身错也不得猜**」；
  (f) **正例对照（必须全绿，防过度拒绝）**：① 同一组归一化母版 + 显式声明（`master_units`/`master_scale`/
      `master_flat_normalize="median"`/`dark_optimization`）⇒ 通过，且校准产物中位数落在 §5 公式的
      ADU 预测值附近（一致性判据的**可调容差数值**按 CFG-001 只登记在 `config/defaults.json` 的
      `calibration.dark_light_exposure_tolerance` 项，本文件只引用不复制数值；口径定义见 SCI-RES-01/R-003）：
      T2 NGC1727（曝光 600 s）逐像素 oracle `median[(raw−bias−K·(dark−bias))/flat_norm]` =
      **436.2 ADU**，现行错误实现 7048.6 ADU ⇒ **16.2×**；T4（曝光 180 s）361.2 vs 3650.4 ADU ⇒ 10.1×）；
      ② 母版本就 ADU（观测中位数 > 1）+ 平场本就归一（`median(flat)` ∈ 带内）+ 约定已声明
      ⇒ **无需任何标度声明**即通过（回归锚：门不是「一律拒绝」）。
  四条负例（a/b/c/e）必须能同时判红，两条正例（f①②）必须同时判绿（红→绿对照见
  `run/PROJECT-GOVERNANCE-01/UNIT-001/`）。

## 12 关联 ALG ID

- `ALG-CAL-001` MasterBias/Dark 生成（sigma-clip+合并）
- `ALG-CAL-002` MasterFlat 生成（减 Bias→逐帧归一→sigma-clip+mean→再归一）
- `ALG-CAL-003` 单帧校准 `calibrate/calibrate_d`（双分支除法+floor）
- `ALG-CAL-004` 坏点检测/修复 `cc_detect_hot/cold + cc_correct_median`

## 13 追溯与测试

- 权威文件: `docs/science/CALIBRATION.md` (SCI-CAL-001)
- 实现: `lib/algorithms/calibration/src/calibrator.cpp` (`normalize_flat, calibrate, calibrate_d`), `lib/algorithms/calibration/src/master_generator.cpp` (`generate_master`), `lib/algorithms/calibration/src/cosmetic_corrector.cpp`
- 公开 API: `lib/algorithms/calibration/include/astro_calibration.h` (`ac_generate_master_bias/dark/flat, ac_calibrate_frame, ac_correct_frame` 及其 `_f64` 变体)
- 测试: `TST-CAL-001` 常量场、`TST-CAL-INV-001` 幂等归一、`TST-CAL-FAIL-001` 参数校验（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用均已核对原文定位）

1. Newberry, M. V. 1991, PASP, 103, 122, "Signal-to-Noise Considerations for Sky-Subtracted CCD Data"（DOI 10.1086/132801）。定位：全文 S/N 模型含 bias/dark/read-noise 分量。**本合同不引用其具体公式号**（原文公式映射未在本次核验范围内逐式确认）；§5 连续定义为 Project-defined derivation，文献仅作概念上下文，不得以其覆盖本合同。
2. Janesick, J. R. 2001, *Scientific Charge-Coupled Devices*, SPIE Press Monograph PM83（ISBN 0-8194-3698-4），Ch.2 photon transfer（gain/read-noise 测量上下文；Ch.2 定位经 Janesick et al. 2004 EM-CCD 论文二次引用核对）。
3. HST ACS Data Handbook §4.4 "Flat-Field Reference Files"（<https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-4-flat-field-reference-files>；URL 即节定位）：flat-field=像素响应校正、P-flat 结构与低频修正分离的实践上下文。
4. **ccdproc (astropy affiliated package), "Reduction toolbox"**（<https://ccdproc.readthedocs.io/en/latest/reduction_toolbox.html>，逐字核验 2026-09-17）：原文 "Assume in this section that you have created a master bias image called master_bias and a master dark image called master_dark **that has been bias-subtracted** so that it can be scaled by exposure time if necessary."；顺序为 trimming/overscan → `subtract_bias` → `subtract_dark(..., scale=True)` → `flat_correct`；`subtract_dark` API 文档（<https://ccdproc.readthedocs.io/en/latest/api/ccdproc.subtract_dark.html>）参数 `dark_exposure`/`data_exposure`/`scale`。**据此冻结：master_dark 是"已减 bias 的暗电流母版"，bias 必须显式减除，dark 按曝光比缩放，flat 最后除。**
5. **LSST Science Pipelines `lsst.ip.isr`**（ISR = instrument signature removal；<https://pipelines.lsst.io/modules/lsst.ip.isr/index.html>）：模块自述 "corrections for overscans, crosstalk, **bias and dark frames**"；源码 `python/lsst/ip/isr/isrFunctions.py`（<https://raw.githubusercontent.com/lsst/ip_isr/main/python/lsst/ip/isr/isrFunctions.py>，逐字核验 2026-09-17）：`biasCorrection` 执行 `maskedImage -= biasMaskedImage`，`darkCorrection(maskedImage, darkMaskedImage, expScale, darkScale, ...)` 的 Notes 逐字为 "The dark correction is applied by calculating: maskedImage -= dark * expScaling / darkScaling"（即 `−dark·t_light/t_dark`），`flatCorrection` 执行除法且 flat 标度取自数据（`scalingType` MEAN/MEDIAN/USER，不假设已归一）；`isrTask.py` 的 `IsrTask.run` 处理顺序自述为 doBias → doCrosstalk → doBrighterFatter → **doDark** → doFringe → doStrayLight → **doFlat**。**据此冻结：bias → dark(×K) → flat 的顺序与 K 的物理含义（曝光比）。**
6. **XISF Version 1.0 Specification**（PixInsight/Pleiades Astrophoto；<http://pixinsight.com/xisf/xisf-1.0.xsd> 为随规范发布的 XML Schema，本次以镜像全文逐字核验 2026-09-17）：§Image 元素 `bounds="lower:upper"` —— "This attribute shall be specified for all Image elements serializing floating point real pixel data. … The bounds attribute defines the representable range of a real or integer image … `lower` … the black point … `upper` … the white point"；"There is no default representable range for real images whose pixel samples are encoded as floating point scalars, so in these cases the representable range must be declared explicitly"；整数图像默认可表示域 `[0, 2ⁿ−1]`。`FITSKeyword` 元素只是 FITS 兼容元数据层（"provides a compatibility layer with image data stored as legacy FITS files"），**不承担像素域/单位语义**。**据此冻结：XISF 浮点 `bounds="0:1"` 是渲染可表示域，不是 ADU；ADU 换算因子必须由调用方声明。**
7. **PCL（PixInsight Class Library）参考实现 `src/pcl/XISFReader.cpp`**（<https://gitlab.com/pixinsight/PCL/-/raw/master/src/pcl/XISFReader.cpp>，逐字核验 2026-09-17）：`NormalizeSamples`/`NORMALIZE_FLOAT_IMAGE` 把文件可表示域线性映射到目标类型域——浮点目标 `*i=(*i−lower)/range`（→[0,1]），整数目标 `*i=(*i−lower)·MaxSampleValue/range`（→[0,2ⁿ−1]）；`UInt16` 的 `MaxSampleValue()=65535`。同一物理数据在 Float32 [0,1] 与 UInt16 [0,65535] 两种表示间的换算因子即 **65535**。**据此冻结：XISF Float32 `bounds="0:1"` 母版换算到 16 位 ADU 的声明因子取 65535（声明制，非推断制）。**
8. **FITS Standard 3.0 §4.2.1/§4.3**（`BSCALE`/`BZERO`：物理值 = `BSCALE·样本 + BZERO`；16 位相机 `BZERO=32768` 伪无符号约定）。本仓真实亮场（T2/T4）实测 `BITPIX=16, BZERO=32768`，读入域 [0,65535] ADU——这就是亮场一侧的"ADU 域"定义。
9. IRAF `ccdproc`/`zerocombine` 家族（NOAO/IRAF：zero → dark → flat 的经典归约顺序，dark 帧先在 zero 校正后合并）。**本次核验状态**：`iraf.net` 帮助页对本节点返回 403（Cloudflare 人机校验），**未能逐字取原文**；因此本文只按 ccdproc 文档中明示的 IRAF 等价关系（"Those transitioning from IRAF to ccdproc … BIASSEC and TRIMSEC conventions"）与经典实践引用，不作为冻结判据的唯一来源（冻结判据以第 4、5 条为准）。

## 15 Acceptance

- §11 Oracle 全过（解析解 max_abs==0、NumPy FP32 rtol=1e-6/atol=1e-7）；
- §7 四不变量门（常量场/空平场/幂等归一/确定性）全过；
- §11 **bias 参与门**与 **K 参与门**全过，且 ignore-bias 变异注入可判红（BIAS-001）；
- §11 **标度/归一化门**四条负例（单位混用 / 平场未归一 / dark bias 约定未声明 / 声明自洽）与
  两条正例（显式声明组合 / 本就合规组合=防过度拒绝）全过，
  且负例为**真实二进制端到端**判红（UNIT-001；`tools/quality/check_master_unit_guard.py --self-test`）；
- 单位经 `tools/check_glossary.py`（GLOSSARY_PASS）且本文件无被禁 alias；
- §9a 专属问题逐项有锚点回答，无 TBD/二选一（`tools/science_contract_lint.py` PASS）；
- 解析不变量可转 SYN-001：常量场→SYN-001 constant/ramp 用例；NaN/饱和→SYN-001 invalid 边界用例（映射登记于 SYN-001 任务）。
