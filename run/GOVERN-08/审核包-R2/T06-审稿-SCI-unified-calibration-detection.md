# T06 审稿 · SCI-unIFIED-CALIBRATION-DETECTION（第 1–5 轮）

> **车道**：`docs/science/unified/**`、`docs/science/calibration/**`、`docs/science/detection/**`
> **负责轮次**：第 1–5 轮（结构 → 口径 → 公式 → 证据 → 引用）。第 6–10 轮由 T06 的其他车道负责，本件不越界，但把跨面证据已核到的部分列在「附：已核但归他轮」里。
> **基线**：HEAD 见 `git -c core.quotepath=false log -1 --format=%H`；工作树仅两条与本车道无关的未跟踪文件（`artifacts/evidence/noise_snr_code_inventory.md`、`docs/noise_snr_audit_upstream_findings.md`），本单未触碰。
> **纪律**：全程零 git 写操作、零文件修改（除本交付件）、未编译、未跑 ctest/pytest、未执行任何仓内脚本。复算件写在 `/tmp`（`/tmp/acsd_derive.py`、`/tmp/acsd_lsq.py`、`/tmp/yvv.py`）。
> **依据**：`run/GOVERN-08/工作包-RECTIFY-09原件/standards/03_READING_AND_ADVERSARIAL_REVIEW.md`、`…/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`、`AGENTS.md`、`docs/ACSD_DESIGN.md`。
> **`L.key` 假定**：任务面未给出显式 lane key，本单按审稿范围自定为 `SCI-unified-calibration-detection`。若前台另有约定，改名即可，正文不依赖该串。

---

## 1 读完了吗

| 项 | 数量 |
|---|---|
| 本片文档数 | **9**（unified 4 + calibration 3 + detection 3，其中 3 份是 3 行 README） |
| 内容文档（逐行读完） | **6** 份：`SCIENCE_SCOPE.md` 132 行、`UNIFIED_SCIENCE_MODEL.md` 214 行、`DATA_SEMANTICS.md` 278 行、`CALIBRATION.md` 258 行、`CCD_DEFECT.md` 220 行、`STAR_DETECTION.md` 202 行、`ASTROMETRY.md` 219 行 —— 实为 **7** 份主体，全部读完 |
| README | 3 份各 3 行，全读 |
| 合计 | **1532 行，全部逐行读完** |
| 未读完 | **无**。9/9 全读。 |

**必读前置**：AGENTS.md（127 行）、`docs/ACSD_DESIGN.md`（568 行，全读）、标准 03（55 行）、标准 04（71 行）、标准 01/02（供子代理与本单结构轮对照）。另为交叉对照主动读了 `docs/science/noise_snr/NOISE_SNR.md`（相关节）、`docs/science/psf/PSF.md`（相关节）、`docs/science/DISPUTE_RESOLUTION.md`（351 行，全读，用于噪声常数裁决）、`docs/science/algorithms/PHOTOMETRIC_FIT.md`（相关行）、`docs/engineering/standards/NUMERIC.md`（相关节）。

**公式重推覆盖**：MAD→σ 常数、Clopper–Pearson 上下界、Gaussian 核范数与平滑等效倍数、Young–van Vliet 生产核等效范数（**我按生产源码逐位复现了滤波递推**）、Moffat/高斯 FWHM 系数与比值、平场 floor 相对误差、面亮度星等换算符号、线性标度律方差传播、暗流截距失配式与 `Δt_max`、T2/T3/T4 全部最小二乘读数、Zackay–Ofek 最大 S/N 估计量、Calabretta–Greisen 式 (54)(55) 与 `(φ₀,θ₀)`、`sec²Δ`、桥接门余量、Mad 的渐近方差与有限样本因子（MC + 精确积分）、`c_se = 1/(4φ(d)d)`。

---

## 2 逐轮问题清单

格式：**位置 | 问题 | 依据（含我的推导或可复跑命令）| 建议改法**。

### 第 1 轮 · 结构

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 1-01 | `SCIENCE_SCOPE.md:65,101` | 第 5 章是「假设」而非标准要求的「判据与误差」；第 7 章把「有效域/失效条件/不产出/参考文献」四节并成一章，参考文献降为 7.4 | 复跑 `grep -n '^## ' docs/science/unified/SCIENCE_SCOPE.md`；对照 `standards/02_DOCUMENT_WRITING_STANDARD.md:16-24` 七段式。另 6 份主体文档全部对齐。`grep -n '判据' docs/science/unified/SCIENCE_SCOPE.md` 只命中 :82 一处且是失效判据 | 把 :107-116「7.2 失效条件」表提升为「5 判据与误差」；:101-105/:118-120 另立章节 |
| 1-02 | `SCIENCE_SCOPE.md:52` / `DATA_SEMANTICS.md:190` / `docs/ACSD_DESIGN.md:108` / `docs/science/noise_snr/NOISE_SNR.md:237` | **5σ 深度定义 + 参考轮廓口径四处重复正本**；`NOISE_SNR.md` 给公式却不给轮廓，其余三份给轮廓却不点名 | `grep -rn '中位视宁度' docs/` → 命中 ACSD_DESIGN.md:108、SCIENCE_SCOPE.md:52、DATA_SEMANTICS.md:190；`grep -n 'F_5\|m_5' docs/science/noise_snr/NOISE_SNR.md` → :237 只有 `m_5 = ZP - 2.5*log10(5*sigma_F(ref))` | 轮廓收敛到 `NOISE_SNR.md:237` 旁一处，其余三处自然语言点出 |
| 1-03 | `SCIENCE_SCOPE.md:31,40` / `DATA_SEMANTICS.md:115,117-120` | 标度类别词表（5 token）与线性标度律在本车道被**逐字抄两遍** | 复跑 `grep -n 'surface_brightness' docs/science/unified/*.md`；两份都写「由 engineering 正本的数值标准承载」，但仍重抄 | 保留一处的引用 + 一处的结果引用，删重复全抄 |
| 1-04 | `UNIFIED_SCIENCE_MODEL.md:125` | 「全链的数据对象是**封闭集**」把自己抬成第二正本 | `ACSD_DESIGN.md:149` 已声明「各是独立对象，全仓一份正本定义」；`grep -rn '封闭集' docs/` | 改为「本表是 `ACSD_DESIGN.md` 数据对象集在物理含义/单位/权重资格三列上的投影」 |
| 1-05 | 代码与配置侧：`lib/algorithms/calibration/README.md:53,58`、`docs/science/algorithms/CALIBRATION_ALGORITHMS.md:386,465,470,489,513`、`lib/algorithms/calibration/src/master_generator.cpp:212` | **一整族悬空节锚指向本车道正本**：`SCI §3a / §4 / §7 / §8 / §9a / §11 / §15`（即旧平铺 `docs/science/CALIBRATION.md`）。现车道版 `CALIBRATION.md` 只有 7 章，无 §3a/§4/§7/§8/§9a/§11/§15 | 复跑 `grep -c '^## ' docs/science/calibration/CALIBRATION.md` → 7；`grep -rn 'SCI §' docs/science/algorithms/CALIBRATION_ALGORITHMS.md lib/algorithms/calibration/` → 8 处，全部无落点 | 按 migration 映射改指新正本的具体小节（自然语言点名小节标题，不用 §） |
| 1-06 | 代码与合同侧：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1107,1368,1522,1592`、`lib/infrastructure/gaia_xpsd_client/{src/module_entry.c:938,README.md:42,62,memory.md:31}`、`lib/algorithms/calibration/include/astro_calibration.h:281,287`、`eng/packaging/config/defaults.json:966`、`eng/contracts/data/unified_object_registry.json:206,244`、`eng/contracts/schemas/phase_config_normalize.schema.json:225`、`frame_snr.schema.json` 的 `propertyNames.not.description` | **第二整族悬空节锚指向本车道正本**：`DATA_SEMANTICS §4a / §8 / §8.2 / §10.2 / §10.3 / §10.5 / §11.2 / §12.4 / §13.4 / §16.1 / §30.4 / §31.7 / §31.8`。现车道版 `DATA_SEMANTICS.md` 只有 7 章、`κ` 零命中 | 复跑 `for s in '§4a' '§8.2' '§10.5' '§12.4' '§30.4'; do printf '%s ' "$s"; grep -c "$s" docs/science/unified/DATA_SEMANTICS.md; done` → 全部 0；`grep -c 'κ\|kappa' docs/science/unified/DATA_SEMANTICS.md` → 0 | 同上；`κ` 处方应改指 `CCD_DEFECT.md`（现正本） |
| 1-07 | `SCIENCE_SCOPE.md:82` | γ 判据的「冻结面」是**悬空指针** | 文档称「冻结阈值 `|γ−1| ≤ 0.3`（仓内冻结值，**冻结面在 science 的噪声与信噪比分册**）」。复跑 `grep -n 'γ\|对数斜率' docs/science/noise_snr/NOISE_SNR.md` → 只有 :58 的 log-log 斜率 1/2/0 指纹与 :252 的 `gamma=2`（SNR 指数），**没有 `|γ−1| ≤ 0.3`**；`grep -rn 'd log(patch' --include=*.md .` → 全仓只命中 SCIENCE_SCOPE 自己 | 二选一：在 `NOISE_SNR.md` 补该判据，或把指针改到真正承载处；并把符号 `γ` 与 SNR 指数 `γ` 分名（见 2-09） |
| 1-08 | `SCIENCE_SCOPE.md:52` | **文本损坏**：3 个 U+FFFD 替换字符（应为「摘要量」中的「要」） | `python3 -c "s=open('docs/science/unified/SCIENCE_SCOPE.md',encoding='utf-8',errors='replace').read().split(chr(10));print(repr(s[51]))"`；同批其余 8 份零替换字符 | 改回「要」；并加一条 CI 检查扫 U+FFFD |
| 1-09 | `DATA_SEMANTICS.md:135` | `masonry 面` 是 `mosaic` 的笔误；且同句「有两处**实现**」与上文 :132-133 的「两个**面**」混用 | `grep -n 'masonry\|mosaic' docs/science/unified/DATA_SEMANTICS.md` → :132 用 `mosaic 面`、:135 用 `masonry 面`；`grep -rn 'masonry' docs/` 无定义 | 改 `mosaic`；「两处实现」→「两面」 |
| 1-10 | 7 份主体文档第 3 行 `> 上游：docs/ACSD_DESIGN.md 第 X 章第 X 节` | 用章/节号指向另一份文档（标准 02:90-91 要求写相对路径与文档名，不用 § 锚）；且 `docs/ACSD_DESIGN.md` **在这 7 份的参考文献表里一次都没出现** | `grep -n '^> ' docs/science/{unified,calibration,detection}/*.md` → 7 处；`grep -n 'ACSD_DESIGN' docs/science/{unified,calibration,detection}/*.md` → 只命中第 3 行 | 抬头改成「上游：最高设计《ACSD 最高设计》的项目定位与创新点两章」一类自然语言；并把最高设计列入文末来源 |
| 1-11 | `ASTROMETRY.md:182` | 历史叙事：「取得它们时的输入带有**未修复的母版标度问题**，修复后必须整体复跑才能标定」 | 违反 `AGENTS.md §5`「正文无…历史叙事」与标准 02:10 | 改成正向表述：「阈值分档在母版标度问题修复后的输入上标定；标定完成前这些读数不得作为判据」 |
| 1-12 | `CALIBRATION.md:34` | 正文内嵌 40 位 commit 串（标准 02:11 禁正文 commit；02:88 只允许出现在文末参考文献条目） | `grep -n '5a3902196' docs/science/calibration/CALIBRATION.md` → :34（正文）与 :258（参考代码行） | :34 只留「PixInsight Class Library 的 `XISFReader.cpp` 的 `NormalizeSamples`」，commit 串下沉到 :258 |
| 1-13 | `DATA_SEMANTICS.md:69-76` | 表题「逐像素编码有**三个**互斥且穷尽的态」下列 **4 行**（第 4 行「损坏」是硬失败、非产品态） | `sed -n '69,76p' docs/science/unified/DATA_SEMANTICS.md` | 表题改「三个产品态 + 一条硬失败」；或把「损坏」移出表 |
| 1-14 | `UNIFIED_SCIENCE_MODEL.md:130` vs `:125` | `:130` 一行两对象（`variance`/`ivar`），而 `:125` 写「端口只接同一种对象」，读者易误读为两者共享端口 | 同行对照 | 该行标注「本行两对象，端口分立」 |

**第 1 轮小计：14 条。无零发现。**

---

### 第 2 轮 · 口径

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 2-01 | `STAR_DETECTION.md:32,152` | 噪声估计器的**差分方向写错**。文档两处写「**相邻行之间**的差分」；生产实现是**同一行内相邻列**差分 | 实现 `lib/algorithms/star_detection/src/sdet_api.cpp:708` 注释 + `:722-723` `const double diff = (double)row[x] - (double)row[x-1];`；`docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:34` 写「逐行差分 `d[y,x]=img[y,x]−img[y,x−1]`」。中文「相邻行之间」= 垂直差分，与实现的水平差分在有行/列向非均匀噪声或平行转移结构的探测器上噪声功率不同 | 改「行内相邻像元差分」，并给式 `d[y,x]=I[y,x]−I[y,x−1]` |
| 2-02 | `STAR_DETECTION.md:50,152` vs `:52-64` | **`bgnoise` 的定义漏了 ×1/√2**，导致同一文档两处对同一符号的定义相差 √2 | 代码 `sdet_api.cpp:756` `return (T)(med_std * 0.70710678118654752);`，函数头注释 `:710` 明写「行标准差 → 行中位 → *0.70710678（1/sqrt(2)）」。我的实测：`python3 -c "import numpy as np;r=np.random.default_rng(0);x=r.standard_normal((4000,4000));print(np.sqrt(((x[1:]-x[:-1])**2).mean()))"` → 1.41388 = √2·σ，故未除 √2 的行差分 RMS 是 1.414σ。**换算链本身没错**（代码有该归一化），错的是文档对符号的文字定义 | :50 与 :152 的定义补成「行内差分稳健标准差 ×1/√2 = 逐像元噪声 σ_n（ρ_adj=0 时无偏）」，并引 `STAR_DETECTION_ALGORITHMS.md:34-40` 的 ρ_adj 适用域 |
| 2-03 | `STAR_DETECTION.md:184` | **上下游倒置**：把「天体测量解算」列为检测的下游 | `docs/ACSD_DESIGN.md:184-185` mermaid 节点序是 `platesolve → star_detection`；`:194` 文字「WCS 解算在前」；`:196`「星表引导检测用本帧权威 WCS 把 Gaia 逆投影到像素域」 | 改「上游：本设计 platesolve 节点的 WCS；下游：PSF 建模与测光消费检测输出」 |
| 2-04 | `STAR_DETECTION.md` 全文 | 只描述**盲检测**（:50「单一全局阈值加局部极大」），从未写**权威模式是星表引导** | 代码 `lib/infrastructure/scheduler/src/module_adapters.cpp:3378-3379`：`catalog_guided（权威路径，星表缺失即 fail-closed）| blind_diagnostic（显式声明的非权威诊断路径）`；`ACSD_DESIGN.md:196` 同 | 补「星表引导检测为本链权威路径，盲检测为显式声明的诊断路径」一条 |
| 2-05 | `SCIENCE_SCOPE.md:36,42`、`DATA_SEMANTICS.md:20,89,124,188` vs `ACSD_DESIGN.md:98-99`、`PHOTOMETRY.md:15`、`frame_snr.schema.json:374` | 同一逐帧测光标度**四个名字**（`α_k` / `k_photo` / `photscal` / `scale`），链上无任何等价声明 | 复跑 `grep -rn 'α_k\|k_photo' docs/science/unified/ docs/science/calibration/ docs/science/detection/` → 车道内只命中 `α_k` 4 处、`k_photo` **0** 处 | 在 `DATA_SEMANTICS.md §3.6` 写一句「`α_k` 即最高设计与合同中的 `k_photo,k` / `frame_k_photo`」 |
| 2-06 | `SCIENCE_SCOPE.md:40` vs `:93` | 内部张力：`:40` 写「电子数只在校正方另行给出增益换算后才成立，**本链不建模增益**」，`:93` 的加权方差面来源式却直接用 `gain` 与 `read_noise_e/gain` | 同文件两行对照 | :93 补一句「增益由调用方从帧头/配置提供，本链不反推增益」 |
| 2-07 | `SCIENCE_SCOPE.md:93` | `var = max(signal,0)/gain + (read_noise_e/gain)²` **量纲不齐**；且 `signal` 在 `DATA_SEMANTICS.md:165` 已被定为 `ADU/sr` | 逐项量纲：`read_noise_e/gain` → e⁻/(e⁻/ADU) = ADU，平方 = ADU² ✓；`signal/gain` 若 `signal` 是 ADU → ADU²/e⁻（≠ADU²），若是 e⁻ → ADU（未平方）。**没有任何读法让两项同量纲**。标准写法是 `Var = S_ADU/g + (RN_ADU)²`，或电子域 `Var = S_e + RN_e²` | 明确写出 `signal` 的域与两项的量纲；建议改成 `var_adu = max(S_adu,0)/g + (RN_adu)²` 并注明 e⁻ 计数约定 |
| 2-08 | `UNIFIED_SCIENCE_MODEL.md:54-59,135` | `point_information` 写 `ADU⁻²`，而同表 `:130` 的 `ivar` 写 `sr²/ADU²`，文档未声明 `d_k` 在哪个承载面 ⇒ 表内两行并列而无语境 | 我的量纲分析：`W = a²PᵀC⁻¹P`，`P` 归一化无量纲 ⇒ `[W] = [C⁻¹]`。若 `d` 在通量面，`C` 是 `ADU²` ⇒ `W` 是 `ADU⁻²` ✓；若 `d` 在面亮度面，`C` 是 `ADU²/sr²` ⇒ `W` 是 `sr²/ADU²` ✗。`DATA_SEMANTICS.md:175` 已把这条写死（「点源量不随面亮度口径变…与面亮度族的 `sr²/ADU²` 不是同一量纲」），本册没引 | 在 :135 单元格或 :142 后补一句与 `DATA_SEMANTICS.md` 等价的族限定语 |
| 2-09 | `SCIENCE_SCOPE.md:82` vs `NOISE_SNR.md:252` | 符号 `γ` 在链上被赋**两个完全不同的含义**：patch 方差对数斜率 vs `w = SNR^gamma/F_ref^gamma` 的指数 | `grep -rn 'γ' docs/science/` → `SCIENCE_SCOPE.md:82`（斜率）与 `NOISE_SNR.md:252`（指数 =2） | 前者改 `κ_γ` 或 `s_log`，后者保留 `γ` |
| 2-10 | `DATA_SEMANTICS.md:185,186,188` | `reference_mag = 6.0` 的**冻结点标注不成立**；键名 `reference_flux_k` **在 schema 里不存在**；`scope` 只列了 enum 两值之一 | 复跑 `python3 -c "import json;d=json.load(open('eng/contracts/schemas/unified/frame_snr.schema.json'));b=d['properties']['reference_baseline']['properties'];print(b['reference_mag']);print(b['scope']);print(b['reference_flux_source'])"` → `reference_mag` 只有 `{'description':…, 'type':'number'}`（无 const/default，全文无 `6.0` 字面）；`scope.enum = ['group','frame_independent_fixed_magnitude']`；`reference_flux_source.enum = ['fixed_magnitude','group_median','config','unavailable']`；`grep -c 'reference_flux_k' eng/contracts/schemas/unified/frame_snr.schema.json` → 0。6.0 的真实落点是**代码默认值** `lib/infrastructure/scheduler/src/module_adapters.cpp:7151 ref_mag = doc["snr"].value("reference_mag", 6.0);`（可被输入 JSON 覆盖，不是冻结量） | 三处都改：冻结点改指代码默认值并标明「可被配置覆盖」；键名改 `reference_flux_common`；`scope` 补 `group` 档与 `reference_flux_source` 的四值 |
| 2-11 | `eng/contracts/schemas/unified/frame_snr.schema.json:5,301` | 合同字面写「真实信号/噪声比，**不受天光影响**」，与最高设计的单调性论断**方向相反** | 复跑 `grep -n '不受天光影响' eng/contracts/schemas/unified/frame_snr.schema.json` → :5、:301；`ACSD_DESIGN.md:110`：「信号…天光只通过其散粒噪声进入噪声项；…**固定源通量下天光越亮信噪比越低**」 | 合同描述改为「天光只经散粒噪声进入分母；固定源通量下 SNR 随天光单调下降」 |
| 2-12 | `CALIBRATION.md:53,59,62` | `flat` / `flat_norm` 符号重载：`:47` 声明 `master_flat` 是「已归一平场」，`:53/:59` 的公式除 `max(flat, 0.1)`，`:62` 又定义 `flat_norm = max(flat/median(flat), 0.1)`；`:75` 改用 `flat_norm` | 同节内三处对照 | 公式里统一写 `max(flat_norm, 0.1)`，`flat` 只表示盘上母版 |
| 2-13 | `CALIBRATION.md:215` | 冻结不变量表里的符号 **`dark′` 全仓只出现这一次，从未定义** | `grep -rn "dark′" docs/ eng/ lib/ --include=*.md --include=*.cpp --include=*.h` → 唯一命中 `CALIBRATION.md:215`。按上下文应是「标准式的 `dark`」或「兼容式的 `dark − bias`」（`:53` vs `:59`），两分支取值不同 | 表前补定义，并写明分标准/兼容两式 |
| 2-14 | `CALIBRATION.md:208-216` vs `docs/science/algorithms/CALIBRATION_ALGORITHMS.md:465-470` | 两份**不变量清单不是同一份**：science 七条（常量场/空平场/幂等归一/确定性/偏置参与/缩放因子参与/约定等价）vs 算法 I1–I6（…/负值保留/掩码极性）。互缺 3 条，而 `:224` 自称「上表七条」 | `sed -n '465,471p' docs/science/algorithms/CALIBRATION_ALGORITHMS.md` | 两侧合并为一份八条清单并互相点名 |
| 2-15 | `STAR_DETECTION.md:40,140-141` | `1.230310` 未声明参数化口径，读者按标准 Moffat 公式会算出 0.869959 | 复跑 `python3 -c "import numpy as np;print(2*np.sqrt(2**0.25-1))"` → `0.8699589`（= FWHM/α）。仓内 `docs/science/psf/PSF.md:69,85` 定义其 `σ` 为轮廓 rms 半径（`α = √2·σ`），故 `FWHM = 2√2 σ √(2^{1/4}−1) = 1.230307652590102·σ`，与代码 `lib/algorithms/psf/src/dpsf_psf.cpp` 的 `MOFFAT4_FWHM_FACTOR` 一致 ⇒ **仓内自洽**。但「同一方向宽度参数」的措辞把两套 σ 混为一谈 | 补一句「`1.230310` 是 `FWHM/σ_psf`（rms 半径口径），不是 `FWHM/α`（= 0.869959）」 |
| 2-16 | `STAR_DETECTION.md:152-153` | 两套 σ_bg 描述**不对称**：文档只给了第二套的裁剪细节，读者会得出「第一套不做裁剪」的错误画像 | 实现 `sdet_api.cpp:726-737` 第一套**同样**做每行 3 轮 `5σ` MAD clip，再取总体标准差、行间取中位、最后 ×1/√2 | 两套都写裁剪轮数与阈值；并在 :153 补上第二条消费链（`sigma_sky_adu` → 帧 SNR/深度 → HiPS `frame_snr`，见 `star_detector.cpp:37-39`） |
| 2-17 | `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:365-366` | 代码注释把 SIP 系数量纲的**内在矛盾**指向 `docs/science/ASTROMETRY.md §5`，而该册第 5 章是「判据与误差」，**无对应内容** | 复跑 `sed -n '360,368p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp`；`grep -n '^## \|^### 5' docs/science/detection/ASTROMETRY.md` | 在 ASTROMETRY §3.1 显式写清本链 A/B 系数是**像素域**约定（`pixel^{1−i−j}`）、与 SIP 标准的中间世界坐标（度）约定差一个 CD 量纲，并给出该转换的读侧要求 |
| 2-18 | `docs/ACSD_DESIGN.md:562`（顶层，非本车道，报给前台） | 附录 A 术语表的数据对象清单**漏 `depth_m5`**，与同文件 `:149` 的 13 个对象冲突 | `grep -n 'depth_m5' docs/ACSD_DESIGN.md` → 只命中 :149 与 :108 | 顶层补一项 |

**第 2 轮小计：18 条。无零发现。**

---

### 第 3 轮 · 公式

#### 3-A 已确认的**公式错误**（我自己动手重推，逐条给推导）

**3-01（最重之一）`CALIBRATION.md:75` — 平场 floor 的相对误差式写反，与自己下一行的表直接矛盾**

- 文档：「由 `cal = num / max(flat_norm, 0.1)`，真实响应 `f < 0.1` 的像元被按 `0.1` 相除，该像元的校准值相对误差为 **`(0.1/f − 1)`**，**单边偏低**」
- 我的推导：`cal = num/0.1`，`cal_true = num/f`，相对误差 `(cal − cal_true)/cal_true = (f/0.1) − 1`。
- 复算：`python3` 见 `/tmp/acsd_lsq.py`，对 f = 0.05/0.02/0.01 得 `f/0.1−1 = −50% / −80% / −90%`，而文档式的 `0.1/f−1 = +100% / +400% / +900%`。
- 文档自己的表（`:83-85`）写的是 **−50.0% / −80.0% / −90.0%** ⇒ **正文式与本节表格自相矛盾，且正文式符号与「单边偏低」的定性相反**。
- 建议改法：`(0.1/f − 1)` → `(f/0.1 − 1)`，并补一句「`0.1/f` 是响应倒数的相对误差，不是校准值的」。

**3-02（最重之一）`DATA_SEMANTICS.md:98` — 面亮度星等的 `Ω_ref` 项符号错，量级 53.14 mag**

- 文档：`SB_mag = ZP_k − 2.5·log10(signal) + 2.5·log10(Ω_ref)`
- 我的推导（两种自然读法都给出**减号**）：设 `F = SB·Ω`。
  - 读法一「`SB_mag` = 面积 `Ω_ref` 内该面亮度的总星等」：`m(Ω_ref) = ZP − 2.5log10(SB·Ω_ref) = ZP − 2.5log10(SB) − 2.5log10(Ω_ref)`。
  - 读法二「`SB_mag` 为单位 `Ω_ref` 面积上的星等面密度」：`m = SB_mag − 2.5log10(Ω/Ω_ref)` ⇒ `SB_mag = m + 2.5log10(Ω/Ω_ref) = ZP − 2.5log10(SB·Ω) + 2.5log10Ω − 2.5log10Ω_ref = ZP − 2.5log10(SB) − 2.5log10(Ω_ref)`。
  - 交叉核对（`/tmp/acsd_lsq.py`）：取 `ZP=0, SB=1e-4 /sr, Ω=10 sr` ⇒ `F=1e-3`、`m=7.5000`；`Ω_ref = 1 arcsec² = 2.35044e-11 sr` 时文档式给 **−16.5721**、正确式给 **+36.5721**，**差 53.1443 mag**。`Ω_ref = 1 sr` 时两者都是 10.0000（差 0）⇒ 错误只在 `Ω_ref ≠ 1 sr` 时显形。
- 另：`SG_psf`/深度链的 `+2.5log10` 符号在整个 astronomy 文献里都是「mag/arcsec² = mag/sr − 26.57」，方向与本文档写法相反。
- 建议改法：`+ 2.5·log10(Ω_ref)` → `− 2.5·log10(Ω_ref)`，并给一个自洽的数值自检例（像 `Ω_ref = 1 sr` 时两式相等）。

**3-03（最重之一）`UNIFIED_SCIENCE_MODEL.md:68` — Zackay & Ofek 转述两处失真（一手核对）**

- 文档：「在独立测量 `θ_j = μ_j T + ε_j`、`Var(ε_j) = σ_j²` 下，似然比关于 `T` 线性，其充分统计量权重**恰为 `1/σ_j²`**，最大信噪比估计量为 **`Σ_j μ_j θ_j / σ_j²`**」
- 一手：`curl -sSL https://arxiv.org/pdf/1512.06872 | pdftotext` 后 `grep -n -B4 -A8 "maximum S/N estimator"` → §2.2
  - 式 (9)：`S = Σ_j (μ_j/σ_j²) X_j`
  - 式 (18)：`T̃ = (Σ_j μ_j θ_j/σ_j²) / (Σ_j μ_j²/σ_j²) = S/I`
  - 附录 A 式 (A10)：`β_j = μ_j/σ_j²`；§3 结论「weighting the images by `F_j/σ_j²`」
- 差异：(a) 权重是 **`μ_j/σ_j²`**，不是 `1/σ_j²`；(b) 最大 S/N 估计量是 **`S/I`**，文档**只抄了分子 `S`，丢了归一化分母 `I = Σ μ_j²/σ_j²`**。
- 讽刺点：同一页 `:73-74` 的 `F̂ = Q/W` 是带除法的，`:95` 还专门论证「权重必须携带 `F_ref²`」，却在引用处把分母丢了。
- 建议改法：补回 `μ_j` 因子与分母；或改引原始式并注明 `μ_j ≡ 1` 的特例才得到文档写法。

**3-04 `UNIFIED_SCIENCE_MODEL.md:76` — `blockdiag` 与「帧间噪声相关时不能简单求和」自相矛盾**

- 文档：「**帧间噪声相关时不能简单求和**：必须用联合协方差 `C = blockdiag(C_1..C_K)` 的一次矩阵求逆，此时 `W = a² Pᵀ C⁻¹ P` **不等于** `Σ_k W_k`」
- 推导：`blockdiag(C_1..C_K)` 正是**独立**情形；此时 `C⁻¹` 块对角 ⇒ `PᵀC⁻¹P = Σ_k PᵀC_k⁻¹P = Σ_k W_k`。文档在同一句里既用 `blockdiag` 又断言不等于，逻辑上不可能同时成立。
- 建议改法：改成 `C = [[C_11, C_12], [C_12ᵀ, C_22]]`（一般分块矩阵），并把 `blockdiag` 留给独立情形的对照。

**3-05 `ASTROMETRY.md:89` — 「缩小 3600² 倍」方向与倍数都错（用生产代码自证）**

- 文档：「`cd_inv` 的量纲是像元每角秒，**不得写成度每像元**——写成度每像元会让畸变被**缩小 `3600²` 倍**」
- 我的推导：`L` = trans 线性项（arcsec/px），`CD = L/3600`（deg/px）。
  - 元素级逆（本链用的就是元素级）：`inv(L_deg) = inv(L/3600) = 3600·inv(L)` ⇒ **放大 3600 倍**。
  - 行列式级：`det(L_deg) = det(L)/3600²` ⇒ `1/det` 放大 3600² 倍。
  - 两种情形**都是放大，不是缩小**；且本链实现是元素级 ⇒ 倍数是 **3600**，不是 3600²。
- 生产代码自证：`sed -n '338,360p' lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp` ⇒ `det_lin = x10*y01 − x01*y10`（arcsec²/px²）、`cd_inv_00 = inv_det_lin*y01`（px/arcsec）；`:344` 的代码注释原文写「注意: 用 trans 线性项的逆, 不是 result->cd 的逆 (**差 3600 倍**)」⇒ **代码注释与本文档的 3600² 矛盾，且代码注释的 3600 与我的推导一致**。
- 建议改法：改为「写成度每像元会使 `inv` 结果**放大 3600 倍**（元素级）或 3600² 倍（行列式级），畸变项随之放大」。

**3-06 `ASTROMETRY.md:146` — `sec²Δ ≤ 1.03046` 与「半角 15 度」互斥**

- 复算：`python3 -c "import numpy as np;print(1/np.cos(np.radians(15))**2, np.degrees(np.arccos(1/np.sqrt(1.03046))))"` ⇒ `sec²(15°) = 1.0717968`；`1.03046` 对应 **9.9000°**。
- 仓内来源侧一致：子代理定位到 `lib/algorithms/projection/p3_wcs.cpp:180` 与 `docs/science/PHASE3_HIPS_TO_FITS.md:152` 都写「FOV ≤ 20° ⇒ γ_max = 9.9023°、sec²γ_max = 1.030462」⇒ 正确表述是**全视场 20°（半角约 10°）**，不是半角 15°。
- 建议改法：`（对应切平面视场不超过半角 15 度）` → `（对应全视场不超过 20°，半角约 10°）`，并引 `PHASE3_HIPS_TO_FITS.md` 与 `GATES_AND_TOLERANCES.md`。

**3-07 `ASTROMETRY.md:142` — 「独立区间包络余量不低于 2.8 倍」按文档自己的常数算不出**

- 复算：`python3 -c "print(0.9/(1.79e-3*100), 0.9/(2.93e-3*100))"` ⇒ `5.0279`（✓ 支持「不低于 5 倍」）、`3.0717`（要得 2.8 需 `s = 3.2143e-3`，文档未给该常数）。
- 判读：「不低于 2.8 倍」作为**下界**成立（3.07 ≥ 2.8），但它既不等于标称值也无推导，与同句「5 倍」（5.03）不同源。
- 建议改法：把两端常数（`0.184` 与 `0.9586`）写进正文让读者能自己复算；或把 2.8 改成实测值 3.07。

**3-08 `ASTROMETRY.md:146` — 误差模型缺 `C_env`，与「100% 来自双精度舍入」不自洽**

- 文档给了 `ε ≈ C_env·u·sec²Δ/s_rad`，`u = 2⁻⁵³`，但 **`C_env` 全文无取值**（`grep -n 'C_env' docs/science/detection/ASTROMETRY.md` 只有 :146 一处）。
- 我的反推：要让 `ε = 1e-6 px` 的保守门在 `s = 1.79e-3 arcsec/px` 处「恰好成立」，需 `C_env ≈ 1.5e7`（`/tmp` 复算：`1e-6×1.79e-3/(2⁻⁵³×1.03046) = 1.565e7`）。而纯双精度舍入地板 `u·sec²Δ/s_rad ≈ 3.4e-11 px` ⇒ 该模型在 `u=2⁻⁵³` 下**无法论证 1e-6 px 门的紧性**（比地板宽约 4.5 个量级）。
- 子代理独立实测闭式 TAN 往返最坏误差（15° 半视场，3e5 随机点）= 2.26e-9 arcsec ⇒ 实测 `C_env ≈ 3.4e4`，文档取值比实测保守约 440 倍。
- 建议改法：给出 `C_env` 的取值与来源（实测 or 解析），并把「100% 来自双精度舍入」与「门比地板宽 4.5 个量级」两句的关系写清。

**3-09 `DATA_SEMANTICS.md:148` — `1e-46` 与仓内实测差约 12 个数量级**

- 文档：「一个以 `calibrated_adu` 标度给出的方差地板 `1e-12`（`ADU²`），按 `α²` 换算到 `photo_scaled_adu` 后**约 `1e-46`**」
- 我的复算：`np.float32(1e-46) == 0.0` ✓、最小正规数 `1.1755e-38`、最小次正规数 `1.4013e-45`、`1/1e-46 = 1e46 > 3.4028e38` ⇒ 「精确下溢为 0」「1/floor 上溢为无穷」**这两句数值上为真**。但**文档没给 α，这个例子不可复现**（反解需 `α² = 1e-34`、`α = 1e-17`）。
- **与工程正本冲突**：`docs/engineering/standards/NUMERIC.md:71-75`（我已复核原文）写「真实数据（M42 生产噪声平面 `run/M42-VARIANCE-RCA-01/plane_fixed.f64`，sha256 `965e6fe…`）：冻结 `variance_floor = 1e-12 ADU²` 按 **33 个实测 `α² ∈ [1.2966e-46, 3.6099e-45]`** 换算后，在 float32 下 **32/33 精确下溢为 0**，其余为**次正规数**（max `4.204e-45`）⇒ 换算后地板真值是 **1.297e-58 … 3.610e-57**。
- ⇒ 两处结论：(a) 「约 `1e-46`」看起来是**把 `α²` 的量级误当成了换算后的地板值**，差约 12 个数量级；(b) 「**精确**下溢为 `0`」**不成立**——33 例中有 1 例是非零次正规数（我已复核 `np.float32(4.204e-45) != 0`）。
- 建议改法：直接引 `NUMERIC.md` 的 33 例口径与 `α²` 实测区间，显式点名次正规数分支，删掉自造的 α=1e-17 例。

**3-10 `STAR_DETECTION.md:52-64,135` — 35.45 是连续核理想值，生产平滑器是 Young–van Vliet 递归滤波**

- 文档自陈按「**连续**二维高斯核」导出 `‖k‖₂² = 1/(4πσ_s²)`，`σ_s = 2.0` ⇒ `‖k‖₂ = 0.141047`、`5/‖k‖₂ = 35.45`、`1/‖k‖₂ = 7.09`。我的复算：`1/(2·2·√π) = 0.1410474`、`5·2σ_s√π = 35.4491`、`1/(2σ_s√π) = 7.08982` ⇒ **连续极限三值全部正确**。
- 但生产平滑不是采样高斯核，而是 **YvV 递归滤波**：`lib/algorithms/star_detection/src/sdet_api.cpp:1586,1588` 调 `sdet_gaussian_blur_yvv(src, dst, w, h, 2.0)`，实现在 `lib/algorithms/star_detection/src/sdet_image.cpp:176-183` → `sdet_yvv_blur_impl`，系数公式在 `:106-123`（Young–van Vliet 的 `q = 3.97156 − 4.14554√(1−0.26891σ)`、`b0 = 1.57825 + 2.44413q + 1.4281q² + 0.422205q³`），递推在 `:131-149`。
- **我按源码逐位复现了该滤波**（`/tmp/yvv.py`，σ=2 得 `B=0.227285251, b1=1.232767549, b2=−0.553313955, b3=0.093261155`，DC 增益 1），对冲激响应求 `Σk²`（可分离 ⇒ 二维 `Σk² = (Σk₁²)²`）：`Σk₁² = 0.1331741816`（N=41/81/161/321 全部收敛到同值），`Σk²₂D = 0.0177353626` ⇒ **`1/‖k‖₂ = 7.50896`、`5/‖k‖₂ = 37.5448`**，比文档的 7.09/35.45 **高 +5.912%**。
- 对照：离散采样高斯（半径 8）`Σk²₂D = (0.14105236)² = 0.019896` ⇒ 35.45 只在「无限长采样高斯」时成立，截断核也会偏低。
- 建议改法：:135 的常数表加「按连续二维高斯核解析导出；生产核为 YvV 递归高斯（`sdet_image.cpp:106-149`），等效 `1/‖k‖₂ = 7.50896`、等效倍数 `37.5448`（+5.91%）」，并把两条口径并列冻结。

**3-11 `CCD_DEFECT.md:103` — `κ ≥ 1/Σw² = 2` 的「信息论论证」不成立**

- 文档：「`κ` 的下界来自一条不可回避的信息论论证：修复值是邻居的确定性函数，**不得声称它比一次独立测量携带更多信息**，故 `var ≥ σ²`，即 `κ ≥ 1/Σw² = 2`」
- 我的推导：`ŷ = Σ_j w_j x_j`，`Σw = 1`，各 `x_j` 独立同方差 `var` ⇒ `Var(ŷ) = Σ_j w_j²·var`。**这正是式中 `κ = 1` 的那一项。** 两个独立样本的算术平均方差是 `var/2`，**确实携带比单次测量更多的信息**（方差更小），所以「不得声称比一次独立测量携带更多信息」这句在数学上是假的。
- 另：真正的相关性来自 `ŷ` 与邻居 `x_j` 之间的**协方差**（`Cov(ŷ, x_1) = w_1·var`），它影响的是「修复像元与邻元一起参与后续加权时的有效自由度」，不改变 `Var(ŷ)` 本身。把它写成对单像元方差的乘性下界是**换了物理对象**。
- 判读：`κ = 2` 作为**保守工程选择**是合理的（`SUMW²/2` 落到 `var`，等价于「修复像元按一次独立测量记账」），但它**不是下界、也不是信息论结论**。§5.1 的判据「`var ≥ κ·Σw²·var_j`，`κ` 的下界 2 来自信息论论证」把这个错误论证固化成门。
- 建议改法：把「信息论论证」改为「保守记账约定：修复像元的方差不低于一次独立测量」；`κ ≥ 1/Σw²` 改述为「取 κ=1 意味着修复像元与邻元噪声完全独立且加性组合方差最小；本链不采用该乐观口径，冻结 κ=2 使其按一次独立测量记账」。同时补一句：`astro_calibration.h:283` 另有「贴边单侧复制时 `Σw² = 1`」的形态，`κ` 只施加在被修复像元上，两种形态须分开记账。

**3-12 `CALIBRATION.md:108` — 截距失配式的推导隐含一条未声明前提**

- 文档：`cal_pipeline − cal_true = (t_light/t_d)·(b_light − b0) = −K·Δb`，`Δb ≡ b0 − b_light`
- 我的完整重推：设 `master_dark` 为已减偏置、`median(master_dark) = (b0 + I_d·t_d) − b_bias`，`K = t_light/t_d`：
  `cal_pipeline = raw − b_bias − K[(b0 + I_d t_d) − b_bias]`；`cal_true = raw − b_light − I_d·t_light`；
  相减 = `−K·b0 + b_bias(K−1) + b_light − b_light + I_d t_light − K I_d t_d` = `−K·b0 + b_bias(K−1) + K·b_light − b_light` = `−K(b0 − b_light)`。
  ⇒ **数值上与文档一致**，但我用了 `b_bias = b_light`（即 `median(master_bias) = b_light`）。文档**没有声明这条前提**；若母版偏置中位与亮场偏置电平不同，式子多出一项 `b_bias(K−1) − (b_bias − b_light)`。
- 建议改法：把「`b_light` 取 `master_bias` 中位」写成显式前提，并给该项的残差式。

**3-13 `DATA_SEMANTICS.md:41-43` — HiPS tile 的 FITS 行项与它自己引的 Górski 约定相反（详见 §5 独立裁决块）**

**3-14 `ASTROMETRY.md:58,201` — 「切平面投影在天球北极处发散」与所引 [2] 原文矛盾**

- 文档：「切平面投影在天球北极处发散，因此参考点的选择必须避开极点，这也是原生纬度取 90° 的原因」（§5.7 失效域表重复一次「视场跨越切平面投影的发散区（接近天球北极）」）
- 一手（`curl -sSL https://arxiv.org/pdf/astro-ph/0207413 | pdftotext`，我已独立取到全文）：
  - §2.2 原文「the gnomonic projection **diverges at the equator** so it can't have the reference point there for the same reason」；Fig. 8 题注「Gnomonic (TAN) projection; **diverges at θ = 0**」（θ=0 是**原生赤道**）。
  - §2.3 原文「For zenithal projections, `(φ0, θ0) = (0, 90°)` so the CRVAL_ia specify the celestial coordinates of the **native pole**」——即对天顶族 CRVAL **就是**极点，与本册 §2.4 自己上一句「此时参考天球坐标（`CRVAL`）就是天球北极的坐标」一致。
- ⇒ 文档在同一段里既说「CRVAL 是北极」又说「北极处发散所以要避开」，自相矛盾；且发散区判错。
- 建议改法：改为「切平面投影在**原生赤道**（与投影中心大圆 90° 处）发散；本链取 `(φ₀,θ₀) = (0°, 90°)` 使投影中心落在天球北极，视场须避开大圆 90° 半径」；§5.7 同步改。

#### 3-B **重推通过**的公式（明确记为无问题，防止下一轮重复劳动）

| 位置 | 量 | 我的复算 | 结果 |
|---|---|---|---|
| `CALIBRATION.md:138-141` | `1.482602218505602 = 1/Φ⁻¹(3/4)`，`Φ⁻¹(0.75)=0.6744897501960817` | `python3` `scipy.stats.norm.ppf(0.75)` 与 `1/` | **16 位逐位相同** ✓ |
| `CCD_DEFECT.md:66,119` | 同上 | 同 | ✓ |
| `STAR_DETECTION.md:78-91` | Clopper–Pearson 零失败 95% 单侧下界 `0.05^(1/n)`；`n ≥ ln0.05/ln0.99` | `0.95427 / 0.97049 / 0.99000 / 0.99701`；`298.0729` | **四行与 298.07 全部逐位命中** ✓ |
| `STAR_DETECTION.md:55,61,64` | 连续核 `‖k‖₂ = 0.141047`、`5/‖k‖₂ = 35.45`、`1/‖k‖₂ = 7.09` | `0.1410474 / 35.4491 / 7.08982` | ✓（但见 3-10：模型与实现不符） |
| `STAR_DETECTION.md:139,141` | `2√(2ln2) = 2.354820`；`2.354820/1.230310 = 1.9140` | `2.3548200`；`1.9140054` | ✓（但见 2-15：口径未声明） |
| `CALIBRATION.md:77-85` | 平场 floor 数值表（0/0/−50%/−80%/−90%） | `/tmp/acsd_lsq.py` 逐格复算 | **表全部正确** ✓（错的只是正文那一个式，见 3-01） |
| `CALIBRATION.md:122` | `Δt_max = t_d·(ε·σ_frame/|Δb| − 1)` | 由 `|K·Δb| ≤ εσ_frame` 与 `K = t_light/t_d` 解出 | ✓ |
| `CALIBRATION.md:183,185,195` | T2/T3/T4 全部最小二乘读数 | 独立最小二乘：T2 slope **−0.038236**、截距 **1042.111**、max|res| **21.072**、`1042.111−1001.867 = 40.244`；T4 slope **+0.420480**、截距 **1021.855**、max|res| **0.0341**（文档 0.0342）、`1021.855−916.156 = 105.699`；`105.70/57.82 = 1.8281` | **全部命中** ✓（残差向量 T2 `[-10.536, 21.072, −10.536]`、T4 `[−0.0244, 0.0341, −0.0097]`） |
| `CALIBRATION.md:67` | `K=1` 时两分支代数恒等 | `(raw−bias) − (dark_total−bias) = raw − dark_total` | ✓ |
| `CCD_DEFECT.md:113` | 五点插值权重和 `−0.274+0.774+0+0.774−0.274 = 1` | 1.000 | ✓（无偏性结论只依赖权重和，成立） |
| `CCD_DEFECT.md:105,138` | 不施 κ 时 SNR 高估 `√κ = √2` | `1/√(1/2) = √2` | ✓ |
| `ASTROMETRY.md:62` | `π/2 ≈ 1.5708`、`π/(2√2) ≈ 1.1107` | `1.5708 / 1.1107207` | ✓ |
| `ASTROMETRY.md:96` | `s0 = 3600·√|det(CD)|`（arcsec/px） | `[CD] = deg/px` ⇒ `√|det| = deg/px` ⇒ ×3600 = arcsec/px | ✓ |
| `ASTROMETRY.md:45` | `q = p − CRPIX = (下标+0.5) − w/2` 的自洽性 | `xp = 下标+1`，`CRPIX = w/2+0.5` ⇒ `xp−CRPIX = 下标 + 0.5 − w/2` | **恒等成立** ✓ |
| `ASTROMETRY.md:55-56` | `R_θ = (180°/π)·cot θ`、逆 `θ = arctan(180°/(π R_θ))` | 一手 `arXiv:astro-ph/0207413` §5.1.3 式 (54)、(55) | **逐字命中** ✓ |
| `ASTROMETRY.md:49` | `LONPOLE` 缺省「`δ₀ ≥ θ₀` 取 0°，否则 180°」；天顶族缺省恒 180°（除非 `δ₀=90°`） | 一手 §2.2 原文（我已 `grep` 命中） | **逐字命中** ✓ |
| `ASTROMETRY.md:49` | `(φ₀,θ₀) = (0°,90°)` | 一手式 (11)（§2.2/§2.3/Table 12） | ✓ |
| `UNIFIED_SCIENCE_MODEL.md:57-65` | `Q_k = a_k Pᵀ C⁻¹ d`、`W_k = a²PᵀC⁻¹P`、`F̂ = Q/W`、`Var = 1/W`；多帧 `Q = ΣQ_k, W = ΣW_k` | 令残差对 `F` 求导为零 ⇒ `a²F PᵀC⁻¹P = a PᵀC⁻¹d` ⇒ `F = Q/W` | ✓ |
| `UNIFIED_SCIENCE_MODEL.md:102-103` | GLS `x̂ = (AᵀC⁻¹A)⁻¹AᵀC⁻¹d`、`Cov = (AᵀC⁻¹A)⁻¹` | 标准结论 | ✓（但 3-04 的 `blockdiag` 破坏了同节的一致性） |
| `UNIFIED_SCIENCE_MODEL.md:88-91` | `SNR² = F_ref²·W`、`w = SNR²/F_ref² = W = 1/σ_F²` | 由 `σ_F = 1/√W` 直接代入 | ✓ |
| `DATA_SEMANTICS.md:188` | `F_ref,k = 10^(−0.4·(m_ref − ZP_k))` | 由 `m = ZP − 2.5log10 F` ⇒ `F = 10^(0.4(ZP−m)) = 10^(−0.4(m − ZP))` | ✓ |
| `DATA_SEMANTICS.md:117-120` | `x′=αx ⇒ Var′=α²Var, ivar′=ivar/α²`；`S = F/A_cell ⇒ Var(S)=Var(F)/A_cell²` | 一阶传播律代入 | ✓ 数值正确（但「逐字出自 GUM」不成立，见 4-10） |

**第 3 轮小计：14 条公式缺陷（3-01…3-14）+ 17 组重推通过。无零发现。**

---

### 第 4 轮 · 证据

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 4-01 | `SCIENCE_SCOPE.md:84` | 支撑「**前提可被违反是已确认的事实**」的唯一证据句，**零数字、零出处、零实验路径** | `grep -rn '对数斜率' docs/` → 全仓只命中本句；同段「作为对照的『跨帧配对差』口径…给出的斜率更大」同样无数字 | 补实验单元路径 + 两个口径的斜率读数 + 复跑命令；否则「已确认的事实」降级为「未定量项」 |
| 4-02 | `STAR_DETECTION.md:74` | 三个定量读数 `0.0047 / 1.086 / 0.902` **正文零引用**；且唯一载体不可复跑 | 唯一出处 `artifacts/evidence/governance-01/research/R-3_PSF质心科学门与容ada.md:140-146`（子代理定位）；`find . -name "exp_r3*"` → 无输出，即 R-3 自称的复跑脚本与结果日志**不在仓内** ⇒ 三个数**不可复跑**，违反 `AGENTS.md §6`「复现命令 + 输出」 | 引可定位路径 + 补脚本与 seed；补不到就降级为「历史读数，不可复跑」 |
| 4-03 | `STAR_DETECTION.md:155` | 「同帧实测**不相等**…实测比值**不为 1**」——**无数值、无出处** | 真实读数在 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md:445`：「同帧实测 **20.7384 vs 13.8148 ADU（比值 1.5012，M42_M1_T2 Red 20251212@012404）**」 | 补两个数 + 帧标识 + 引 algorithms 卷 |
| 4-04 | `STAR_DETECTION.md:121` | 「真饱和平台电平与 65535 之比跨越 1，且有**可观比例**高于 65535」「被标为饱和的检出星中有**相当比例**为尖峰形」——**零数字、零引用** | 部分读数在 `STAR_DETECTION_ALGORITHMS.md:70-73`（`max=80789.7 ADU`、`dynrange=65339.0`、`saturated=1` 共 94/2474、其中 53 颗 3×3 峰值 ≤ 65535，帧 `M42_M1_T2 Red 20251212@012404`）；但「尖峰形比例」**全仓无任何数字** | 补前三个数与帧标识；「尖峰形比例」要么补数要么删 |
| 4-05 | `ASTROMETRY.md:141,142,146` | `1.79e-3`/`2.93e-3`/`0.9`/`5 倍`/`2.8 倍`/`C_env` 全部**无引用、无推导** | 真实出处 `docs/science/algorithms/GATES_AND_TOLERANCES.md:73-74`（含 `C_env = 78 实测 / 128 设计`、`u=2⁻⁵³`、仓内最小真实尺度 `0.9586″/px`）。我按该值反算：`78·2⁻⁵³·206264.8/1e-6 = 1.786e-3`、`128·2⁻⁵³·206264.8/1e-6 = 2.931e-3` ⇒ **两个常数可复算**，只是本册没写 `C_env` | 引 `GATES_AND_TOLERANCES.md`，正文写出 `C_env` 与 `0.9586` |
| 4-06 | `CCD_DEFECT.md:50-57` | 全部 7 条仓内复算读数（`1.779 ADU`、`97` 跳变、`53` 段、`111` 列、中位 `2`/`p90 927`/最长 `4096`、`260755`/`76757`/`p99.9 = 8`）**无文件名、无命令、无脚本、无 seed** | 复跑 `grep -rn '1\.779\|260755\|76757' --include=*.md .` → 全仓只命中本节；`ls "testdata/T2 calibration files/"` 可见数据文件确实存在 | 补 `testdata/T2 calibration files/masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf` + 解码/统计命令 + 连通域连通性参数 |
| 4-07 | `CALIBRATION.md:36,179-195` | T2/T3/T4 表、`σ_frame = 57.82`、T4 Lum 帧 FITS 头读数，**全部只有目录级描述**（「`testdata/` 下三组标定帧」「同一组 180 秒 Lum 亮场」），无文件名/命令/脚本 | `ls "testdata/T2 calibration files/"` → `masterDark_BIN-1_4096x4096_EXPOSURE-{600,1200,1800}.00s.xisf` 等确实存在；`grep -nE 'python3 |fitsheader|\$ ' docs/science/calibration/*.md` → **零命中**（全车道无一条复现命令） | 每份文档的复算节末加可粘贴的「数据 / 脚本 / 命令 / 读数」四要素块 |
| 4-08 | `DATA_SEMANTICS.md:194-207` | 「星表行语义」**整节零引用**（`sed -n '194,207p' … | grep '\[n\]'` 只命中区间记号 `RA ∈ [0,360)`）；且 `×0.001 − 1.5` 的身份错 | 仓内该式的真实身份：`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:1827,1962,2106,2138-2139` 的 `double magG = mag_raw * 0.001 - 1.5;`，其中 `mag_raw` 是自定义 leaf 记录偏移 20 处的 **uint16**；`lib/infrastructure/gaia_xpsd_client/README.md` 写上游是「PixInsight XPSD 格式客户端（历史上游 Gaia-DR3-DR3SP-Client-C，MIT）」⇒ **这是 PixInsight XPSD 私有索引的编码约定，不是 ESA Gaia DR3 关系** | 改标为「PixInsight XPSD 索引约定」并引该 client 的 `文件:行`；Gaia DR3 官方 flux 语义另立一条（子代理未能取到 ESA 官方公式原句 ⇒ 标「核对不到」，不得凭印象写） |
| 4-09 | `DATA_SEMANTICS.md:186` | `m_ref = 6.0` 的冻结点不成立（见 2-10） | 同 2-10 | 同 2-10 |
| 4-10 | `DATA_SEMANTICS.md:122,276`、`SCIENCE_SCOPE.md:40` | **过度声称「逐字出自计量学通用手册」**。GUM 100:2008 里没有 `Var′ = α²·Var` 与 `Var(S) = Var(F)/A_cell²` 这两句；它们是 GUM §5.1.2 式 (10) `u_c²(y) = Σ(∂f/∂x_i)²u²(x_i)` 的直接推论。另 `:276` 把「线性标度传播的表达式」归到 **§4.2.3**（该节讲的是均值的方差 `s²(q̄)=s²(q_k)/n`，式 5） | `curl -sSL https://www.bipm.org/documents/20126/2071204/JCGM_100_2008_E.pdf | pdftotext`；子代理独立核对：§4.2.3 NOTE 2 的方差/标准差量纲说明**逐字命中**（这部分文档是对的），线性传播律在 §5.1.2 式 (10) | 「逐字出自」降为「由 GUM §5.1.2 式(10) 直接推出」；`:276` 的节号改正 |
| 4-11 | `CALIBRATION.md:141` | 「该文把 `1.4826·MAD` 作为对照基线引用，**其权威全精度值即上面这个 16 位常数**」——**与原文冲突且与本文 :256 自注自相矛盾** | 一手：Rousseeuw & Croux 1993 JASA 88:1273 的权威摘要原文只印 **4 位** `MAD_n = 1.4826 med_j{|x_i − med_j x_j|}`，不可能是 16 位全精度值的出处；而本文 :256 自己写「本册使用的 1.482602218505602 常数由解析式独立复算，**不依赖该文**」 | 删「其权威全精度值」；保留解析式复算句 |
| 4-12 | `CALIBRATION.md:30,34` | 把 `bounds` 的语义（「可表示域」「像素样本可在显示设备上表示的取值范围」）挂在 XISF XSD 上，**来源不支持** | 我独立取到 `https://pixinsight.com/xisf/xisf-1.0.xsd`（须 https + UA，裸 `http://` 现 301/406）：`:248-252` 的 `BoundsType` 只有词法模式与注释 `<!-- lower:upper -->`，**无任何语义定义**；`:623` 是 `<xs:attribute name="bounds" type="BoundsType"/>`，**没有 `use="required"`**；只有 `:24` 的文件头注释写「The bounds attribute is required for floating point real images.」 | 必填声明可保留（引 :24 注释）；语义定义改引 XISF 规范正文或删除 |
| 4-13 | `CALIBRATION.md:34,258` | 65535 换算因子的**取证来源不可解析** | `curl -sSL -o /dev/null -w '%{http_code}' https://github.com/PixInsight/PCL` → **404**；`git ls-remote https://github.com/PixInsight/PCL` → 「Repository not found」；文档所钉 commit `5a3902196a7d7a701385a7113cbdce2976ae1a85` 与 `src/pcl/XISFReader.cpp::NormalizeSamples` **无法核实**（两个独立子代理各自核对到同一结论） | 换成可解析来源，或登记为 UNRESOLVED；`2¹⁶−1 = 65535` 的算术关系本身自明，可只写算术并注明「PixInsight 参考实现的行为未取得可复现证据」 |
| 4-14 | `CALIBRATION.md:204` | §5.4「四条主流实现在这一点上口径一致：IRAF 的坏像元掩膜按像元幅度判定；Siril 的化妆品校正按幅度逐条记录点缺陷与坏列；LSST 的 `afw` 用带包围盒的显式缺陷表；astropy 的 ccdproc 提供 `ccdmask` 参数化的掩膜流程」——**整段零引用** | `sed -n '204p' docs/science/calibration/CALIBRATION.md` 无 `[n]`；**同一组断言在 `CCD_DEFECT.md:75` 是带 `[5]` 的** ⇒ 同仓同断言引用不一致 | 补 `[5]` + 参考代码指针；IRAF/Siril/LSST 各自补书目级条目 |
| 4-15 | `CCD_DEFECT.md:158` | §5.3「**Rubin 侧只有代码级证据**：其仪器签名移除模块在掩膜像元上按插值后的像元值重算方差面，并把插值像元排除出坏区统计」——**零引用、零 `文件:行`**，却是全册唯一的强代码断言 | `grep -n 'Rubin' docs/science/calibration/CCD_DEFECT.md` → 唯一命中 :158，该行及所在列表项无任何 `[n]`；`AGENTS.md §6` 要求上游代码证据给「项目 + 版本 + 文件:行」 | 补 LSST `文件:行` + 版本；补不到就降级为「未取证的转述」 |
| 4-16 | `CCD_DEFECT.md:37` | 「典型曝光时长下单帧被卫星线穿越的比例约为个位数百分比量级」引 [4]，但 :186 自标「全文未取」——**自标已过时** | 该文是开放获取（`https://www.nature.com/articles/s41550-023-01903-3.pdf` 可取）；子代理取到全文并核对到 `2.7 ± 0.2% @ 11 min` 与「贯穿视场、呈直线」 | 自标升级为「已取全文逐字核对」并把 2.7±0.2%@11min 写进正文 |
| 4-17 | `CCD_DEFECT.md:109` | 插值可行性论断的章节归属与例证越界 | 文档在 :192 声称核对「§4.2 的逆方差权重与高斯抽样句」；但「未成像/成像伪影的插值可行性」在 **§4.1**（子代理在 arXiv:1601.07182 已接受稿逐字命中）。另「**衍射星芒**不是该文例证**——§4.1 只用 satellite trails，"diffraction spikes" 全文仅出现 1 次且在引言 | 章节改 §4.1；删「衍射星芒」或改注出处 |
| 4-18 | `DATA_SEMANTICS.md:100` | 「本链冻结的单位串用大写 `ADU`，与标准 Table 4 中的小写 `adu` **只差大小写**」——漏了标准的 case-sensitive 明文 | FITS 4.0 §4.3.1 明写「Note that, per IAU convention, **case is significant throughout**」（子代理独立核对）。⇒ 大写 `ADU` 严格说**不合规范** | 显式登记为项目偏离（给出理由与读侧兼容策略），或改用小写 `adu` |
| 4-19 | `DATA_SEMANTICS.md:45,38` | ① `512×512` 是 `hips_tile_width` 的 **Default / "a good compromise"**，**不是规范强制**；② 文档把 tile 像素序映射归给「外部 HiPS 生成工具族」，但**仓内没有那个 oracle 证据文件**，且仓内既有审查（`run/GOVERN-08/审核包-R2/审稿-P1-TAIL-LIB-001.md` CE-10/CE-17）已指出该测试的期望值取自被检实现自身、对 x/y 位序对调零鉴别力 | 子代理取到 REC 全文：`§4.1` 目录布局与 `§4.4.1` `hips_pixel_scale` 单位为度**逐字对上**；`§4.2.1` 的 512 是推荐值 | 512 标注为「默认档，可配置」；像素序给一手锚（见 §5 独立裁决块），并把 oracle 证据文件纳入仓内或降级 |
| 4-20 | `DATA_SEMANTICS.md:272` | EMVA 参考链接 **404**（`ema-1288` 少一个 v） | 子代理核对：`https://www.emva.org/standards-technology/ema-1288/` → 404；正确为 `emva-1288/`。另 §2.4 Noise Model 属 **Linear** 模块，General 模块的 §2.4 不是噪声模型 | 改 URL；注明被引的是 Linear 模块的 §2.4 |
| 4-21 | 车道整体 | **没有任何一条仓内实测引用具备 `AGENTS.md §6` 要求的「复现命令 + 输出」** | `grep -nE 'python3 |fitsheader|\$ ' docs/science/{unified,calibration,detection}/*.md` → **零命中**；数据文件确实存在于 `testdata/`（我已 `ls` 确认） | 见 4-07 的四要素块方案；可立即补齐的有三条（CCD_DEFECT §3.1 的 T2 600s 母版、CALIBRATION §5.3 的 T2/T3/T4 表、§5.3 的 `σ_frame = 57.82`） |
| 4-22 | `STAR_DETECTION.md:101` | **零问题（正面）**：「仓内不存在该协议的复跑入口（固定 seed 脚本与结果文件）」经子代理独立核对为真（`ls 实验/` 无召回阈表单元；`grep -rl "召回" 实验/ --include=*.md` 只命中 1 个审计笔记）。但仓内**存在相近但不合协议**的召回测量（`run/SCI-FIX-STARPSF-01/code/e2_detect_recall.cpp`：W=256/H=256、固定 seed `20260919`、但对星做了随机亚像元抖动 `:134`，与其结果头声称的「峰值对齐像素中心」自相矛盾；样本量 3 帧 × 25 星 = 75/档，非协议的 1000/档） | 该登记状态的处理是**正确的**（明确、可复核、不隐含通过） | 补一句「仓内存在相近但不合本协议的召回测量」，避免被读成「仓内无任何召回实验」 |

**第 4 轮小计：22 条（4-22 为正面登记）。无零发现。**

---

### 第 5 轮 · 引用

#### 5-A 引用缺陷

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| 5-01 | `CALIBRATION.md:242` | **`[1]` Newberry 1991 是孤儿条目**：文末列出，正文零标注 | `grep -n '\[1\]' docs/science/calibration/CALIBRATION.md` → **只命中 :242**（文末）；正文无任何 `[1]` | 删条目，或在正文找到它真正支撑的那句话（`:93` 的方差式若归给 [4,5]，Newberry 或许该在那儿） |
| 5-02 | `UNIFIED_SCIENCE_MODEL.md:202` | Naylor 1998 **期号错**：`296(1)` 应为 `296(2)` | `curl -s https://api.crossref.org/works/10.1046/j.1365-8711.1998.01314.x` → `issue: 2`；OUP 自己的文章 URL 就是 `/mnras/article/296/2/339/` | 改 `296(2)` |
| 5-03 | `DATA_SEMANTICS.md:266`、`STAR_DETECTION.md:190`、`ASTROMETRY.md:211` | Greisen & Calabretta 2002 **末页错**：三处都写 `1061–1076`，应为 `1061–1075` | `curl -s https://api.crossref.org/works/10.1051/0004-6361:20021326` → `page: 1061-1075`；OpenAlex `last_page: 1075`（两个独立子代理 + 我方取全文均命中） | 改 1075 |
| 5-04 | `ASTROMETRY.md:215` | Shupe et al. 2005 **题名错** + 页码不全 + 「未取全文」不成立 | 文档写「The SIP convention for pointing」；一手（ADS 扫描 `https://articles.adsabs.harvard.edu/pdf/2005ASPC..347..491S`，子代理取到全文）实际题名是 **「The SIP Convention for Representing Distortion in FITS Image Headers」**，作者 David L. Shupe, Mehrdad Moshir, Jing Li, David Makovoz, Robert Narron, Richard N. Hook，页 **491–495** | 改题名与页码；核验状态由「书目级，未取全文」改为已取全文 |
| 5-05 | `CCD_DEFECT.md:190` | 作者缩写错：`Pasha H. A.` → 实为 **Pasha, Imad**（应为 **I.**） | `curl -s https://api.crossref.org/works/10.1088/1538-3873/ad2866`（子代理核对） | 改 `Pasha I.` |
| 5-06 | `CCD_DEFECT.md:192` | 作者缩写错：`Desai V.` → 实为 **Desai, Shantanu**（应为 **S.**） | `curl -s https://api.crossref.org/works/10.1016/j.ascom.2016.04.002` | 改 `Desai S.` |
| 5-07 | `CCD_DEFECT.md:180,184` | `[1]` 与 `[3]` 是**同一份 ACS Data Handbook**（[1] 的范围说明里已含 `§4.6.1–§4.6.3`），条目重叠 | 同行对照 | 合并为一条，正文按节号定位（同一主题一份正本，`AGENTS.md §5`） |
| 5-08 | `CCD_DEFECT.md:212,214` | 题名/页码截短：[17] Massey 题名缺「Advanced Camera for Surveys」后半句；[18] Erben 只给首页（真范围 326(6):432–464） | 子代理 Crossref 核对 | 补全 |
| 5-09 | `DATA_SEMANTICS.md:270` | EMVA 链接 404（见 4-20） | 同 4-20 | 同 4-20 |
| 5-10 | `UNIFIED_SCIENCE_MODEL.md:204` | Zackay & Ofek 用了 arXiv 预印本题名（「Optimal source detection and photometry using ensembles of images」），Crossref 刊名为「How to COAAD Images. **I.** Optimal Source Detection and Photometry **of Point Sources Using** Ensembles of Images」 | 子代理 Crossref 核对 | 统一为刊名并给 arXiv 预印本链接 |
| 5-11 | `DATA_SEMANTICS.md:268` | Górski 题名用 arXiv 版（`…Fast Analysis of Data on the Sphere`），ApJ 刊名为 `…Fast Analysis of Data Distributed on the Sphere` | 同上 | 统一为刊名 |
| 5-12 | `CCD_DEFECT.md:200,202,204` vs `:206,208,210,212` | **取证状态标注不统一**：[11][12][13] 带「书目级核验，未取全文」，而 [14][15][17][19] **无任何标注**却被正文实质引用（`:33`、`:35`） | 同行对照 | 统一给每条标注核对状态 |
| 5-13 | `CCD_DEFECT.md:37` vs `:186` | 「个位数百分比」的定量主张与自标的「全文未取」不匹配（见 4-16） | 同 4-16 | 同 4-16 |

#### 5-B **引用核对通过**（明确记为无问题）

| 范围 | 结论 |
|---|---|
| 编号连续性与一一对应 | `unified` 三份：`DATA_SEMANTICS [1]–[6]`、`SCIENCE_SCOPE [1]–[5]`、`UNIFIED_SCIENCE_MODEL [1]–[7]` —— **全部连续、无跳号、无「有列未引」、无「有引未列」**（两个子代理独立 grep 统计一致）。`STAR_DETECTION [1]–[6]`、`ASTROMETRY [1]–[4]` —— **同样全通过**。`CCD_DEFECT [1]–[20]` —— **全通过**（[12][15][17] 在 `[12, 13]` / `[15, 17]` 组引中被引）。`CALIBRATION [1]–[8]` 连续，**仅 [1] 孤儿**（见 5-01） |
| DOI/卷页真实性 | 除 5-02/03/04/05/06 外，**全部 28+ 条文献的 DOI 均可解析、卷期页与 Crossref/OpenAlex 一致**（两个子代理 + 我方取全文三方交叉） |
| 「逐字核对」的自标注质量 | **整体很高，且大部分真的逐字命中**：`CALIBRATION [4]` FITS 4.0 的 Eq.3 / BLANK 段 / Table 3 `sr` / Table 4 `adu`、`[6]` ccdproc 的 `master_dark that has been bias-subtracted so that it can be scaled by exposure time`、`[7]` LSST `ip_isr@28faec7d` 的 `maskedImage -= dark * expScaling / darkScaling`；`CCD_DEFECT [1]` ACS Table 3.4 的位值 16/64/128 与 §4.3.2 热/温阈句与 §4.5.6「~0.6 per hour…~10%」、`[2]` WFC3 §6.3、`[3]` ACS §4.6.1「over 50 pixels…upstream」、`[6]` van Dokkum & Pasha §6 结论节的形式噪声式与「difficult to assign an uncertainty」、`[7]` Desai §4.2/§3.2、`[8]` SDSS 四条 flag、`[9]` SDSS caveats、`[10]` Bosch §4.5 的权重组与「unbiased if ΣD_pj = 1」与「only objects whose centers were interpolated」、`[18]` Erben 零权重句、`[19]` DES 临时缺陷句、`[20]` van Dokkum 2001 摘要 —— **全部逐字命中**（子代理逐条给出原文句） |
| 诚实性自标注 | `UNIFIED_SCIENCE_MODEL [4]` Aitken 1936 自标「按 Crossref 与 Cambridge Core 记录核对，出版年记为该两源一致给出的 1936」——**经核对为真**（Cambridge Core `citation_publication_date = 1936/01`、Crossref `issued 1936`）。`SCIENCE_SCOPE [5]` Janesick 自标「正文未逐页核对，核验状态为书目级」——**诚实**。反例只有 4-10/4-11 两处（GUM 的「逐字」、RC93 的「权威全精度值」） |
| **astrometry.net URL（我先怀疑、核对后否决）** | `ASTROMETRY.md:219` 的 `https://github.com/dstndstn/astrometry.net` —— **这是官方上游，不是个人 fork**。独立核对：GitHub API `full_name=dstndstn/astrometry.net`、`fork=false`、`parent=None`、863 stars、`homepage=http://astrometry.net`；`https://github.com/astrometry/astrometry.net` → **404（该 org 不存在）**；astrometry.net 官网 `use.html` 正文即链接该仓。许可：仓库 `LICENSE` 首段「…the whole work must be distributed under the **GPL version 3 or later**」⇒ 文档的「GPL-3.0-or-later」**核对到**。**⇒ 不得报为问题** |
| HiPS 像素序（见下方独立裁决） | `DATA_SEMANTICS.md:41-43` 的公式**我判定为行方向反了**，但两个子代理在这个问题上给出了**相反的结论**，其中一个随后自行撤回。见 §5 独立裁决块 |

**第 5 轮小计：13 条引用缺陷 + 5 组明确通过。无零发现。**

---

## 3 本轮是否零发现

**五轮全部不是零发现。**

| 轮次 | 新增实质问题 | 零发现？ |
|---|---|---|
| 1 结构 | 14 | **否** |
| 2 口径 | 18 | **否** |
| 3 公式 | 14（+17 组重推通过） | **否** |
| 4 证据 | 21（+1 条正面登记） | **否** |
| 5 引用 | 13（+5 组明确通过） | **否** |
| **合计** | **80 条** | |

**明确未发现问题的项**（防止下一轮重复劳动，也防止被误读为「什么都没查」）：引用编号连续性与一一对应（除 CALIBRATION [1]）、坏点掩膜极性 `1=坏点`（8 处文档 + 2 处 lib README + ARTIFACTS 全一致）、内部像素坐标约定（5 处一致，`ASTROMETRY:45` 的 `q` 恒等我复算成立）、最高设计 13 个数据对象 ↔ `UNIFIED_SCIENCE_MODEL` 12 行（一一对应）、`var_repaired = (Σw²·var)·κ` 与 `κ=2.0`（文档 / `defaults.json` / `module_adapters.cpp` / `astro_calibration.h` 四层一致）、平场下界 `0.1`（science 与 algorithms 逐字一致）、`MAD→σ` 16 位常数（车道内全部一致且我复算逐位相同）、`1.230310`（与 `psf/PSF.md` 自洽，我复算闭式 `1.2303076525901024`、相对差 `+1.907e-6` 与该册声明的 `+1.91×10⁻⁶` 吻合）、`CALIBRATION §5.3` 全部最小二乘读数（我独立复算逐位命中）、`§3.1` 的 `35.45/7.09/0.141047`（连续极限正确）、`Clopper–Pearson` 四行、正文「见 §几」式机械跳转锚（零命中）、日期/版本号/历史词（`旧版/作废/曾/原`，零命中）、元信息块（第 3 行 `> 上游：` 只是追溯指针，不构成 `AGENTS.md:104` 禁止的元信息块）。

**收敛判定：本车道第 1–5 轮远未收敛。** 按 `standards/03` §2「连续两轮无新增实质问题」的要求，本轮 1–5 每一轮都产出实质新增，**离收敛至少还差一轮订正 + 一次复审**。

---

## 4 独立裁决块（本单必须裁的两件事）

### 4.1 噪声常数争议（1.152/√N 与 1.49×）

**争议双方**：
- **甲方**（旧正本同步件）`docs/science/DISPUTE_RESOLUTION.md`：§4 给 `c(n=64) = 1.152`，`1.44 = 1.152×1.2533`，`N_sky ≥ 9216`，预算数 `(1.152×1.25/0.015)² = 5816.6`，并称「`1.152` 是**相对标准误**」；§11 给 `1.166 = √1.361`，字面式 `1.729/√n`；§12 给 `n=3` 的 MAD 有限样本偏差 `1.49×`，且「实测渐近式在 n=3 处高估 7.4%」与「另记 8.03%」并存，「须以固定 seed 复跑裁定单一值」。
- **乙方**（新分册）`docs/science/algorithms/PHOTOMETRIC_FIT.md:29,299`：`b_n` 表（Croux & Rousseeuw 1992, *Computational Statistics* **1**, 411, p.414：n=3→1.495、4→1.363、5→1.206、9→1.107，n>9→`n/(n−0.8)`），定义式 `MAD_n = b_n·1.4826·med_i|x_i−med_j x_j|`，并明确 RC93 JASA 的 `b = 1.4826` 是**渐近**一致性常数、非有限样本 `b_n`。

**我回到物理/统计推导与一手文献的裁决**（两个子代理各自独立取到 CR92 与 RC93 扫描件并做了 MC/精确积分，我本人复推了闭式并核验了仓内标定脚本）：

#### (1) 我的独立推导（不采信任何一方）

正态样本，`Y_i = |X_i − T|`（`T` = 样本中位数，`T → 0`），`MAD_n` = `Y` 的样本中位数。`G(y) = 2Φ(y/σ) − 1`，`g(y) = 2φ(y/σ)/σ`，`m_Y` 由 `G(m_Y)=1/2` ⇒ `Φ(m_Y/σ) = 3/4` ⇒ **`m_Y = σ·d`，`d = Φ⁻¹(3/4) = 0.6744897501960817`**。

样本 p-分位数的渐近方差：`Var(x̂₍ₚ₎) ≈ p(1−p)/(n·g(F⁻¹(p))²)`。p=1/2 ⇒

```
Var(MAD_n) ≈ 1/(4n·g(m_Y)²) = σ²/(16 n φ(d)²)
E[MAD_n]  → σ·d
```

于是（`φ(d) = 0.3177765726841070`）：

| 量 | 闭式 | 数值 | 甲方声称 |
|---|---|---|---|
| `n·Var(MAD)/E(MAD)²` | `1/(16φ(d)²d²) = (1/(4φ(d)·d))²` | **1.3604593043** | 1.361 ✓ |
| `√n·SD(MAD)/E(MAD)`（相对 SE） | `1/(4φ(d)·d)` | **1.1663872874444212** | 1.166 ✓ |
| `√n·SD(1.4826·MAD)/σ`（σ 归一） | `1.4826/(4φ(d))` | **1.1663872874444212** | （甲方未单列） |

**仓内正本独立吻合**：`grep -n 'c_se' docs/science/noise_snr/NOISE_SNR.md` ⇒ `:142` `SE(sigma_bg)/sigma_bg = c_se/sqrt(N), c_se = 1/(4*phi(d)*d) = 1.1663872874444212`。我复算 `1/(4×0.3177765726841070×0.6744897501960817)` ⇒ **16 位逐位相同**。

**1.729 从哪来**：`1.1663872874444212 × 1.4826 = 1.7292883800`。这是 `√n·SD(σ̂)/E[MAD_raw]`（分母误用**未缩放**的 MAD），比相对 SE 恰好多一个 `1.4826` 倍。

#### (2) 一手文献核对（CR92 / RC93 全文，两个子代理各取一次）

- **CR92 p.414 式 (4) 逐字**：`MAD_n = b_n 1.4826 med_i |x_i − med_j x_j|`，`b_n` 的判据是「**approximately unbiased**」⇒ **b_n 是 E（无偏）判据，不是 SD 判据**。完整表 `n=2..9 → 1.196, 1.495, 1.363, 1.206, 1.200, 1.140, 1.129, 1.107`；`n > 9` 用 `b_n = n/(n−0.8)`。
- **b_n 是 E 判据的独立证明**：CR92 p.413 Table 1 的 `MAD_n` ave 列（未加有限样本修正）逐点取倒数 = `1/0.6689=1.49514`、`1/0.7336=1.36314`、`1/0.8291=1.20613`、`1/0.8331=1.20034`、`1/0.8774=1.13952`、`1/0.8855=1.12931`、`1/0.9032=1.10717` ⇒ **与 p.414 表逐位吻合**。
- **RC93 JASA p.1273 逐字**：`The constant b in (1.2)… we need to set **b = 1.4826**` ⇒ 是**渐近一致性常数**；全文无 `b_n`，只在 pp.1274/1277 交叉引用 CR92。**乙方的「记号边界」陈述逐字成立。**
- **RC93 JASA Table 2（p.1276）的 MAD 行 ∞ = 1.361** ⇒ 证实「1.361 = MAD 的标准化方差」，与我推导的 1.3604593 相对差 **−0.04%**。
- **我的 MC（M=4×10⁵，固定 seed，batch-means SE）**：`b_n^E` 复现 CR92 表 **全部落在 ±0.9% 内**（该文自身 Table 1 ↔ `c_n` 表已有 0.4%–1.1% 的自相矛盾，故其精度约 ±1%）。n=3 另有**精确求积**：`E[MAD]=0.453522`、`SD=0.374607` ⇒ `b_3 = 1.48677`。
- **表与渐近式的接缝**：`b_9` 表值比 `9/8.2 = 1.097561` 高 **0.86%**；`n=10` 渐近仍低 0.85%；`n ≥ 20` 后二者差 < 0.1%。⇒ 引用 `n/(n−0.8)` 时适用域应写 **`n ≥ 20`**。

#### (3) 逐条判决

| 项 | 判决 |
|---|---|
| **`1.49×` vs `1.495`** | **同一个数，无争议。** 差 0.33%。而且两份文件的这句话**字面完全相同**（`DISPUTE_RESOLUTION.md:155`「n = 3 的 MAD 有限样本偏差为 1.49×」vs `PHOTOMETRIC_FIT.md:29`「n=3 时 S 与 sigma_residual 的期望偏低约 1.49 倍」）。§12 只是没写 1.495、没挂 CR92 p.414 |
| **两组常数的适用域** | **不同适用域，不冲突。** `b_n` 治**偏倚**（E 判据：σ̂ 的平均值偏不偏高；高斯、中心=中位数、n 查表或 `n≥20` 用 `n/(n−0.8)`）；`1.1663872874444212` 治**精度**（相对 SE 系数）。恒等换算：`C(n) = c_se(n)·E[σ̂]/σ = c_se(n)/b_n` |
| **`1.152`** | **数值正确、标签错误。** 它是 `C(64) = √n·SD(σ̂)/σ`（**σ 归一**），不是相对标准误。解析 `C(64) = 1.1663872874444212 × (64−0.8)/64 = 1.151807`；我的 MC（M=1e6）= `1.15227 ± 0.00082`；恒等闭合 `1.16649 × 0.98781 = 1.15227`。**真正的相对 SE 在 n=64 是 1.16649**，1.152 比它低 1.24% ⇒ `DISPUTE_RESOLUTION.md:65`「1.152 是**相对标准误**」**判为错标签** |
| **1.152 的仓内出处（决定性）** | `DISPUTE_RESOLUTION.md:66` 说的「20 万次 MC 直接测量 c(64)=1.1508」就是仓内脚本 `实验/absolute-snr/code/audit/route1/exp03_sky_budget_constant.py:47-49`（我已读原文）：`M = 200000`、`s1 = mad_sigma(rng.normal(0.0,5.0,size=(M,64)))/5.0`、`out["c1_patch_mad_n64"] = {"c_sqrt_n": float(s1.std()*8), ...}` ⇒ **`c_sqrt_n = SD(σ̂/σ)·√64` = `C(64)`**。另 `实验/dense-snr-reconstruct/code/route2/exp_P4R2_08_mad_sigma_budget.py:58-60` 同量、n=4000。⇒ **REG-01（原始标定记录待登记）的实质内容可以关闭**：量已定、脚本已定位、seed 已固定 |
| **台账「三读数并存」** | **伪冲突**。1.1508 / 1.1542 / 1.1614111 是 `C(n)` 在 **n=64 / 65536 / 4000** 的值，`C(n) = 1.1663872874444212×(n−0.8)/n` 给出 1.151807 / 1.166373 / 1.166154，差异 1.25% 有闭式解释（`E[σ̂]/σ` 的有限样本偏差） |
| **§11 的 `1.729/√n`** | **错。** `1.7292883800 = 1.4826 × 1.1663872874`，是 `1.4826` 被多乘一次。**正确字面式是 `c_se/√N = 1.1663872874444212/√N`**。⇒ `DISPUTE_RESOLUTION.md:145,150` 与其待同步汇总 `:344`（`docs/science/PHOTOMETRY.md` §11「字面式 `1.729/√n`」）**必须撤回，不得执行**；`PHOTOMETRY.md:420` 现在用的 `1.166/√n` 是对的 |
| **§11「`1.166` 不是『SD(MAD)/MAD 正态渐近常数』」** | **错（自我否认不成立）**。由我的推导，`√n·SD(MAD)/E(MAD) = 1/(4φ(d)·d)` **正是** SD(MAD)/MAD 的正态渐近常数 |
| **§4「`(1.152×1.25/0.015)² = 5816.6`」** | **算术矛盾。** `1.152×1.25 = 1.44`；`1.44/0.015 = 96`；`96² = 9216.0000`（`/tmp` 复算）。`5816.6 = (1.144/0.015)² = 5816.6044`，是**已否的 1.144 支**。子代理穷举 `(a·b/c)²` 组合，池内**仅此一族命中** ⇒ 5816.6 是陈旧/错误数 |
| **顺带发现：`9216` 的自洽写法** | `9216 = (1.152/0.012)²` **精确成立**（1.152/0.012 = 96.0），与 `NOISE_SNR.md:153` 的「本链默认阈值对应 `target ≈ 1.2%`」完全对齐。「1.44 = 1.152×1.2533、(1.44/0.015)² = 9216」只是数值等价的另一写法，1.5% 这个 target 是被 ×1.25 补偿出来的 |
| **§4 的 `1.44 = 1.152×1.2533`** | **无闭式推导。** 那是把 MAD 的 SD 系数与**样本中位数**的 SD 系数相乘，统计上没有定义；其唯一经验支撑是「直接管线测量 c ≈ 1.449（n=9216）」，而 1.449 来自 `exp03` 的 c4 臂（**另一个估计器**：各 patch 的 MAD² 取中位）。**必须标为 heuristic**，否则会被当成派生量 |
| **§12「7.4% vs 8.03% 须以固定 seed 复跑裁定」** | **伪冲突，且该裁决在方法上不可能成立。** n=3 的 `√n·SD(median)/σ` 精确值 = **1.16017814**（精确积分 `E[X₍₂₎²]=0.44867110` ⇒ `SD=0.66982916`；MC 2×10⁶ 复核 1.15987）。`(1.25331414−1.16017814)/1.25331414 = +7.4312%`；`/1.16017814 = +8.0277%`；代数互锁 `1/(1.0803)−1 = −7.4331%`。**两者是同一陈述的两种分母写法**，固定 seed 复跑不可能裁定出「单一值」 |
| **§12 与 PHOTOMETRIC_FIT 并置的问题** | §12 把「√(π/2) 在 n=3 对**中位数**的 SD 高估 7.43%」与「`b_3=1.495` 是 MAD 的**无偏**修正」放在同一节，读者会以为互相支撑，实际**无推导关系**。应分节写 |

**裁决总结**：
- **乙方的 `b_n` 全部成立**（CR92 p.414 逐字 + 我的 MC/精确积分 + 第三方与 CR93 Table 1 的交叉印证）。
- **甲方的 `1.361` 与 `1.166` 数值成立，但字面式 `1.729/√n` 错（多乘 1.4826）**。
- **`1.152` 数值成立、标签错**，且依赖 `n`，不能当 `n` 无关的天光常数用。
- **两组常数不是同一适用域，不构成冲突**：一个治偏倚（`b_n`），一个治精度（`1.1663872874444212`）；`C(n) = c_se(n)/b_n` 把它们精确换算起来。

**还需要的文献（明确写出「需要哪一篇的哪一部分」）：**
1. **要把 CR92 的小 n 表钉到 1e-3**：需 **CR92 论文 p.415–428 的 Fortran 源码**（该文自称 "The Fortran source code of both algorithms is listed in this paper"）**＋其 RNG（Cheney & Kincaid 1985 p.335 与 AS 183）的逐位规定 ＋确切重复数**。在补到之前，`b_3/b_5/b_6/b_9` 按 **±1%** 使用（该文自身 Table 1 ↔ `c_n` 表已有 0.4%–1.1% 自相矛盾为证）。**这不影响 1.495 vs 1.49 的裁决。**
2. **要把 `1.36046` 与 `1.361` 的 −0.04% 钉死**：**RC93 JASA 未给出 MAD 的绝对渐近方差**（只给 `V(S,Φ)=0.8573` 式 (2.9)；MAD 只在 Table 2 给了 CV²）。需另找一手来源核对绝对量 `n·Var(MAD)/σ² = 1/(16φ(d)²) = 0.618983` —— 候选：**Rousseeuw & Croux 1991, *A Class of High-Breakdown Scale Estimators Based on Subranges*, Comm. Statist. 21, 1935–1951**（= CR92a），或 **Hampel 1974**。**在补到之前，`1.361` 只按 3 位有效数字用，精确值一律写 `1.1663872874444212 = 1/(4φ(d)·d)`，不写 `√1.361` 当精确式。**
3. **不需要新文献即可关闭 REG-01**：登记 `实验/absolute-snr/code/audit/route1/exp03_sky_budget_constant.py` 的路径、seed、量定义、`n`、以及 `n` 依赖闭式 `C(n)=1.1663872874444212×(n−0.8)/n` 即可。
4. **`target` 本身（1.2% vs 1.5%）不裁**：`NOISE_SNR.md:153` 写 1.2%、`DISPUTE_RESOLUTION §4` 用 0.015，两者是设计选择落配置的问题（`NOISE_SNR.md:153` 明写「`target` 是本链的设计选择而非物理常数，落配置」），**须由负责人定**，本单不越权。

---

### 4.2 HiPS tile 像素序（DATA_SEMANTICS.md:41-43）—— 一次三方的分歧与我的裁决

**公式**：`leaf local (NESTED, 18 bits) = interleave(x, y)`（`x` 占偶数位、`y` 占奇数位）；`FITS index = (511 − x)·512 + y`。

**我回到一手来源**（`curl -sSL https://arxiv.org/pdf/astro-ph/0409513 | pdftotext`，全文已取）：

- Górski 2005 §5.2 散文句逐字：「They both have their origin in the **southernmost** corner of each base-resolution pixel, with the x index running along the **North-East** direction, while the y index runs along the **North-West** direction.」
- **位序**：同节式 (13)(14) `x = [… b2 b0]₂`、`y = [… b3 b1]₂` ⇒ **x 偶数位、y 奇数位**，与文档一致。
- **式 (15)(17) 与散文句一致，不矛盾**：`v = x + y`（15）、`i = F1(f)·Nside − v − 1`（17），`i` 是**自北极起编号**的环号；`v = 0` ⇒ `i` 最大 ⇒ **最北？** 不对：`F1(f) = frow + 2` 的定义（式 11 前的正文：「we define two functions which index the location of the **southernmost corner** of each base resolution pixel」）⇒ `v=0` 对应 `i = F1(f)·Nside − 1`，即该 face 行的**最南**环。⇒ **`(x,y) = (0,0)` 是最南角**，散文与式 (17) 一致。
- **我用仓内已装的 healpy 1.20.1 独立实测（`python3`，只读）**：`hp.pix2xyf(64, ipix, nest=True)` 对 `p'=1/2/4` 分别给 `(ix,iy) = (1,0)/(0,1)/(2,0)` ⇒ **ix 取偶数位、iy 取奇数位**，与 Górski 一致、**与文档一致**。角位置：face 0（北极帽）`nside=64`，`(x,y)=(0,0)` ⇒ `θ = 89.403°`（Dec = +0.597°，**最南**）；`(63,63)` ⇒ `θ = 0.731°`（Dec = +89.27°，**最北**）。face 3、face 5 同样单调。⇒ **healpy 与 Górski 散文一致：`(x,y)=(0,0)` 在最南角。**
- **FITS 行方向**：Greisen & Calabretta 2002 Paper I（我已取全文，`/tmp/gc.txt:808-812`）§5.1 逐字：「we recommend that FITS writers order the pixels so that the first pixel in the FITS file (for each image plane) be the one that would be displayed in the **lower-left** corner (with the first axis increasing to the right and the second axis increasing **upwards**)」⇒ **FITS 行号增大 = 向北**。IVOA HiPS REC §4.2.1.3 逐字：「Contrary to the FITS convention, in JPEG and PNG the lines of the pixel array are stored in top->down direction.」⇒ HiPS JPEG/PNG 是北在上，HiPS FITS 是**南在上** ⇒ **FITS 行 0 = 南，行号增大向北**。
- **我拉了一个真实 tile 验证**：`curl https://alasky.u-strasbg.fr/2MASS/H/Norder5/Dir10000/Npix10499.fits`（1054080 字节，`astropy.io.fits` 读出 shape (512,512)、`CTYPE1=RA---HPX`、`CTYPE2=DEC--HPX`、`INDXSCHM=None`、`PV2_1=4`、`PV2_2=3`、`CRPIX1=-8703.5`、`CRPIX2=-31743.5`）⇒ **产品确实无 `INDXSCHM` 键**，方向只能靠约定判定。

**裁决链**：`FITS 行 0 = 南`、`FITS 行号增大向北` ⟹ FITS 行号是**北向增**的量；`(x,y)=(0,0)` 是**最南**角、x 向北增 ⟹ **FITS 行号应当正比于 `x`**。文档写的 `511 − x` 是**北向减**的量，把 `x = 0`（最南）放到行 511（最北）⇒ **南北镜像**。

⇒ **`FITS index` 的行项应为 `x·512`（或任何北向增的映射），文档的 `(511 − x)` 与它自己引用的 Górski [2] 约定相反。这是本车道最重的一条结构/公式缺陷**（见 3-13）。

**列方向（诚实标注为未闭合）**：G&C §5.1 只规定「第一轴向右」，**未规定右方是东还是西**；HiPS REC 全文对列方向**零陈述**（子代理对 1326 行做过 north/south/east/west/orient/first row/scan/Cartesian 扫描，零命中）。Górski 说 y 向**西北**增 ⇒ 若第一轴向右为东，则列号应 `511 − y`；若向右为西，则列号 `= y`。⇒ **文档的 `+y` 在两个方向约定下都不可判**。`docs/science/DOCUMENT_INDEX` 之外的仓内 oracle（`lib/algorithms/shared/healpix/healpix_core.h:43-46` 声称「由 CDS Hipsgen MAPTILES 外部 Oracle 逐像素冻结」）**证据文件不在仓内**，且仓内既有审查（`run/GOVERN-08/审核包-R2/审稿-P1-TAIL-LIB-001.md` CE-10/CE-17）已指出该测试的期望值取自被检实现自身、对 x/y 位序对调零鉴别力。

**建议改法**：
1. 行项改 `x`（或写成「FITS 行号北向增」的自然语言 + 一手锚：Górski §5.2 散文与式 (15)(17)、G&C Paper I §5.1、HiPS REC §4.2.1.3）。
2. 列方向**必须点名一份冻结依据**（要么补仓内 oracle 证据文件，要么把 `+y` 登记为「按本仓约定，方向未由标准冻结」）。
3. 文档在该处补一条注：**Górski 2005 §5.2 的散文句与式 (15)(17) 方向一致**（我在本次裁决中曾被一个相反的子代理结论误导，见 §6 的否决记录），并说明 `x` 占偶数位是**取自式 (13)(14) 而非散文**。

**我未能闭合的部分（诚实登记）**：我尝试用 `astropy.wcs` 对真实 tile 的表头做逐像元端到端核对，但该表头的 `CRPIX` 指向投影原点（−8703.5, −31743.5）且带 `PV2_1/PV2_2`，astropy 的 HPX 求值在我手上没有复现出该 tile 的几何（邻位像元的 Dec 偏离 tile 中心 8–20°，物理上不可能），因此**端到端像元级核对未完成**。上表的裁决完全建立在三个一手来源（Górski 散文+式(13)(15)(17)、G&C Paper I §5.1、HiPS REC §4.2.1.3）加 healpy 角位置实测上，不依赖那一步。

---

## 5 推翻的既有结论

| # | 被推翻的结论 | 原出处 | 推翻理由 | 推翻后应是什么 |
|---|---|---|---|---|
| 5-A | 「`1.729/√n` 是 `1.166` 的字面式」，并列入待同步汇总要求 `docs/science/PHOTOMETRY.md` 改写 | `DISPUTE_RESOLUTION.md:145,150,344` | `1.7292883800 = 1.4826 × 1.1663872874`，是 `1.4826` 被多乘一次；`1.4826·MAD` 的相对 SE 就是 `1.1663872874/√N`（与 `NOISE_SNR.md:142` 的 `c_se` 逐位相同） | **`1.1663872874444212/√N`**。待同步项 `:344` 必须**撤回**；`PHOTOMETRY.md:420` 现有的 `1.166/√n` **不要改** |
| 5-B | 「`1.152` 是**相对标准误**」 | `DISPUTE_RESOLUTION.md:65` | `1.152` 是 `C(64) = √n·SD(σ̂)/σ`（σ 归一），解析值 1.151807、MC 1.15227±0.00082；真正的相对 SE 在 n=64 是 **1.16649**，低 1.24% | 「`1.152` = `√n·SD(σ̂)/σ` 在 `n=64` 的值（`= c_se×(1−0.8/n)`），**不是**相对标准误，且**依赖 n**」 |
| 5-C | 「`(1.152×1.25/0.015)² = 5816.6`」 | `DISPUTE_RESOLUTION.md:64` | `1.152×1.25 = 1.44`；`1.44/0.015 = 96`；`96² = 9216.0000` | `9216`（`= (1.152/0.012)² = (1.44/0.015)²`）；`5816.6 = (1.144/0.015)²` 属已否支，删 |
| 5-D | 「`1.166` **不是** SD(MAD)/MAD 的正态渐近常数」 | `DISPUTE_RESOLUTION.md:144` | 我的推导 `√n·SD(MAD)/E(MAD) = 1/(4φ(d)·d) = 1.1663872874` **正是**该常数 | 删掉这句自我否认 |
| 5-E | 「§12 的 7.4% 与 8.03% 是两条路线各自的读数，**须以固定 seed 复跑裁定单一值**」 | `DISPUTE_RESOLUTION.md:156-157` | 精确值 `√3·SD(median₃) = 1.16017814`（精确积分 + 2×10⁶ MC），`(1.25331−1.16018)/1.25331 = 7.4312%`、`/1.16018 = 8.0277%`，代数互锁。**固定 seed 复跑在方法上不可能裁定** | 统一为单一表述「√(π/2) 在 n=3 对样本中位数的 SD 高估 **7.43%**（1.16018 vs 1.25331）」 |
| 5-F | 台账「1.1508 / 1.1542 / 1.1614111 三读数并存、不可分辨」 | `DISPUTE_RESOLUTION.md:66-67` | 三者是 `C(n)` 在 **n=64 / 65536 / 4000** 的值，闭式 `C(n)=1.1663872874×(n−0.8)/n` 给出 1.151807 / 1.166373 / 1.166154 | **伪冲突**；REG-01 可由登记 `实验/absolute-snr/code/audit/route1/exp03_sky_budget_constant.py:47-49`（`M=200000`、`c_sqrt_n = SD(σ̂/σ)·√n`、n=64）直接关闭 |
| 5-G | `FITS index = (511 − x)·512 + y` 的行项 | `DATA_SEMANTICS.md:41-43` | Górski §5.2 散文 + 式 (15)(17) + healpy 实测三源一致：**`(x,y)=(0,0)` 在最南角、x 向北增**；G&C Paper I §5.1 + HiPS REC §4.2.1.3：**FITS 行 0 = 南、行号向北增** ⇒ 行项应为 `x` | 行项改 `x`；列方向（`+y`）**无一手依据可判**，须点名冻结源 |
| 5-H | 「切平面投影在**天球北极**处发散，参考点选择必须避开极点」 | `ASTROMETRY.md:58,201` | C&G 2002 §2.2 原文「the gnomonic projection **diverges at the equator**」、Fig. 8 题注「diverges at θ = 0」（θ=0 是**原生赤道**）；且 §2.3 明写天顶族的 `CRVAL_ia` **就是**极点，与本册上一句自相矛盾 | 「发散区是**与投影中心成大圆 90° 的原生赤道**」；参考点取北极是因为**本链选天顶族**，不是因为北极发散 |
| 5-I | 「`cd_inv` 写成度每像元会让畸变**缩小 3600² 倍**」 | `ASTROMETRY.md:89` | 元素级 `inv(L/3600) = 3600·inv(L)` ⇒ **放大 3600 倍**；行列式级才 3600²。生产代码 `ipv_wcs.cpp:344` 自己的注释写「差 3600 倍」 | 「放大 3600 倍（元素级）/ 3600² 倍（行列式级）」 |
| 5-J | 「`1.2533/√n` 在 n=3 高估 7.4%」与「1.49×」在同一节作为互相支撑 | `DISPUTE_RESOLUTION.md:154-156` | 二者分属**中位数的 SD 精度**与 **MAD 的无偏修正**，正交 | 分节写，并各自标出统计量、判据与量 |
| 5-K | （我自己的初步判断，已自行推翻）「`STAR_DETECTION.md §3.1` 的 5.0→35.45 换算链漏了 √2 因子」 | 本单中途 | 代码 `sdet_api.cpp:756` 已施加 `×0.70710678118654752` ⇒ `bgnoise` 已是 σ_n。**换算链是对的**；错的是文档对 `bgnoise` 的**文字定义**未写该步 | 见 2-02（改为定义缺口，不是公式缺口） |
| 5-L | （我自己的初步判断，已自行推翻）「`STAR_DETECTION.md` 引的 astrometry.net 仓库 URL 可能指到 fork，应更正」 | 本单中途 | `dstndstn/astrometry.net` 是**官方上游**（GitHub API `fork=false`、`parent=None`、官网 `use.html` 链接该仓）；`astrometry/astrometry.net` 反而 404 | **不改** |
| 5-M | （我自己的初步判断，已自行推翻）「`1.230310` 这个 Moffat4 系数对不上标准公式，应改」 | 本单中途 | 标准 `FWHM/α = 0.869959`；`1.230310 = 0.869959×√2`，来自 `psf/PSF.md` 明确的 `α = √2σ`（σ 为 rms 半径）参数化，仓内自洽且与代码 `MOFFAT4_FWHM_FACTOR` 一致 | **数值不改**；只补口径声明（见 2-15） |

---

## 6 自证段

### 6.1 本单做了什么、没做什么

- **真读了**：`AGENTS.md`、`docs/ACSD_DESIGN.md`（568 行全读）、标准 03/04（全读）、本车道 7 份主体文档 **132+214+278+258+220+202+219 = 1523 行逐行读完** + 3 份 README。9/9 全部读完，无抽样。
- **动了手重推**：见 §3-B 的 17 组表格，每组都给了自己的推导或 `python3` 复算命令；对 3-01…3-14 逐条给出了自己发现问题的推导过程。
- **没做的事**：零 git 写、零文档修改、零编译、零 ctest/pytest、零仓内脚本执行。所有复算件在 `/tmp`（`acsd_derive.py`、`acsd_lsq.py`、`yvv.py`）。外部一手件（PDF/XML）也只在 `/tmp`。
- **没采信子代理**：5 个子代理（工具实际起了 10 个，因为每次派发被复制一次）共回报 ~11 万字结论，我**逐条复核了 load-bearing 项**，其中 3 条**否决**、2 条**降级**、1 条**自行撤回我自己的初判**（见 §5 的 5-K/5-L/5-M 与下方否决记录）。

### 6.2 派发了哪些子代理、否决了哪些

| # | 派发范围 | 回报 | 我的处置 |
|---|---|---|---|
| 1 | `CALIBRATION.md` + `CCD_DEFECT.md` 的 [1]–[8] / [1]–[20] 逐条外部取证（DOI 可解析性、卷页、逐字内容、零引用断言） | 书目 28 条全部真实；命中 2 处作者缩写错、2 处章节越界、2 段零引用、1 个不可解析的取证来源（PCL 404）、1 处与原文冲突的「权威全精度值」 | **全部采纳**。`1.482602218505602` 的 16 位复算我独立复核通过 |
| 2 | `unified` 三份的外部取证（含 FITS 4.0 表号、HiPS REC 像素序、Gaia DR3 flux 公式、Zackay–Ofek 权重、Aitken 年份） | Zackay–Ofek 的 `S/I` 与 `μ_j/σ_j²` 两条公式失真、GUM「逐字」过度声称、FITS case-sensitive、EMVA 404、`masonry` 笔误、`reference_mag=6.0` 无冻结面、页码 1076→1075、Naylor 期号 | **全部采纳**（Zackay–Ofek 两条我另取一手 PDF 独立复核通过）。其中「HiPS 行方向是镜像」的最终结论我**采纳**，但**推翻了它的推理链**（它引 Górski 散文 + REC 推断；我补上了 G&C Paper I §5.1 的 FITS 行方向明文与式(15)(17)、healpy 实测，把链条闭合） |
| 3 | `detection` 两份的外部取证（含 SExtractor 对照、wcslib SIP 实现、Shupe 2005、桥接门常数复算） | Shupe 题名错、`sec²(15°)≠1.03046`、2.8 倍算不出、`C_env` 缺、0.0047/1.086/0.902 的 R-3 出处、bgnoise 漏 √2、**astrometry.net URL 正确** | **采纳** Shupe/页码/sec²/2.8/C_env/0.0047/**否决其「wcslib 可对照 `AP[1,0] −= 1`」的核查目标**（wcslib 7.7/8.10 都无 `sip.c`，SIP 已并入 `C/dis.c::sipset()`，它找不到对照物是对的）；**明确记入正面**：astrometry.net URL 与许可核对通过 |
| 4 | MAD 常数争议的独立裁决（Q1–Q7） | 双方实质上都对、`1.729` 错、`5816.6` 错、1.152 是 σ 归一而非相对 | **全部采纳**。我另做闭式复推（`1/(4φ(d)d) = 1.1663872874444212`，与 `NOISE_SNR.md:142` 逐位相同），并**独立读了两份仓内标定脚本**确认 1.152 的仓内出处 |
| 5 | 跨文档一致性（结构 / 口径 / 一致性 / 无出处断言 / 可复现） | 18 条无出处断言、8 条悬空节锚族、1.5e7 的 `C_env` 反推、21 个读数不可复现 | **全部采纳**。其中「`point_information` 不冲突，只缺族限定语」我**独立复核后采纳**（我的量纲分析与它一致，与我初判相反）；「`bad_pixel` 极性全仓一致」我复核后采纳 |
| — | 同任务第二份（工具复制） | 6 份 | **一处否决**：其「HiPS 行方向公式正确 + Górski 2005 自身前后矛盾」**结论错、推理错**。它把 Górski 式 (17) 读反了（式 (11) 前的正文明写 `F1(f)` 索引的是 base pixel 的 **southernmost** corner，故 `v=0` 对应该 face 行的最南环）。它随后自行撤回并改称「以 healpy 为准」；我的 healpy 实测（face 0：`θ(0,0)=89.403°` vs `θ(63,63)=0.731°`）站在 Górski 散文这一边，**故其修正后的结论仍错**，我按原方向推翻 |

### 6.3 定位方式（全部可复跑，不用「见某行」）

- 本车道文档内定位：`grep -n '<锚串>' docs/science/unified/<文件>` 等（本单每条都给了锚串或唯一字串）。
- 仓内代码/合同定位：`grep -rn '<锚串>' lib/ eng/ docs/ --include=*.cpp --include=*.h --include=*.json --include=*.md`，配合 `<文件>:<行>`。
- 数值复算：`python3` 脚本 `/tmp/acsd_derive.py`（常数与量纲）、`/tmp/acsd_lsq.py`（最小二乘与符号）、`/tmp/yvv.py`（生产平滑核范数）。每条公式缺陷都附了自己的推导文字，不依赖脚本。
- 外部一手件：给出 URL 与章节/式号；PDF 一律 `curl -sSL <url> | pdftotext`（或子代理的渲染+OCR 路径）。
- 中文路径：全部在 shell 里加 `git -c core.quotepath=false` 或不加引号直接用（未执行任何 git 写命令，`git status --porcelain` 与 `git ls-remote` 均为只读）。

### 6.4 诚实边界（本单明确核对不到的）

1. `CR92` p.415–428 的 Fortran 源码与其 RNG 规定（子代理未读）⇒ CR92 小 n 表只能按 **±1%** 使用。
2. RC93 JASA 未给 MAD 的**绝对**渐近方差 ⇒ `1.361` 与 `1.3604593` 的 −0.04% 差**未钉死**，需另找一手来源（候选已列）。
3. Gaia DR3 官方 `phot_g_mean_mag_g_flux` 缩放公式原句：ESA Gaia 文档页 404/纯 JS，TAP 只暴露 VizieR 调和子集（无 `_mag_g_` 列）⇒ **`×0.001 − 1.5` 的 ESA 对照核对不到**；但仓内 PixInsight XPSD client 的 `文件:行` 已足以判定它**不是** Gaia DR3 口径。
4. Tonry 2012 / Ivezić 2019 的 **5σ 深度口径原文**核对不到（IOP/OUP 在取证环境不可取，arXiv 无 PS1 预印本）⇒ `SCIENCE_SCOPE.md:52` 声称的「深度的定义与 PSF 匹配、孔径与零点绑定的口径由 Pan-STARRS1 与 LSST 文档给出」**未获证据支撑**，本单**不判定其为错**，只标「核对不到」。
5. Horne 1986 / Naylor 1998 全文不可取 ⇒ 「最优提取公式取同一形式」**核对不到**（不判错）。
6. Rousseeuw & Croux 1993 JASA 全文在两个取证通道都取不到（T&F 403、JSTOR JS 挑战、ADS 405、OpenAlex `oa_status=closed`）⇒ 只有摘要级证据；不过另一个子代理从 wis.kuleuven.be 取得了 RC93 的扫描件并逐字读到 `b = 1.4826`，**该条因此闭合**。
7. `m_ref = 6.0` 是否另有裁决记录：`grep -rn 'm_ref' docs/ eng/ lib/ run/` 只命中 DATA_SEMANTICS、DISPUTE_RESOLUTION 与 schema，**无裁决记录**；6.0 的唯一机器落点是代码默认值（可被配置覆盖）⇒ **「保留并补冻结面」还是「删」需负责人裁决**，本单不越权。
8. HiPS tile 的**列方向**未闭合（见 §4.2）。
9. `CCD_DEFECT.md §3.1` / `CALIBRATION.md §5.3` 的 21 个读数我**只复算了 CALIBRATION 的最小二乘部分（全部命中）**，CCD_DEFECT 的 7 条需要 XISF 解码器，本单**只判其无出处/不可复跑，不判其错**。
10. 本车道文档只报第 1–5 轮；跨文档 ↔ 实验结论、负向、语言、可复现四轮由 T06 其他车道负责，我已把核到的证据列在 §4（`4-21`、`4-22`）供其接手，但不代其下结论。
