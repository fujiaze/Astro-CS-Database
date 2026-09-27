# 检查-S4-scienceC（docs/science 丙组）对抗静态排查报告

> **范围**：`docs/science/REJECTION.md`（399 行）、`SCIENCE_SCOPE.md`（120 行）、`STAR_DETECTION.md`（182 行）、
> `UNCERTAINTY_AND_COVARIANCE.md`（177 行）、`UNIFIED_SCIENCE_MODEL.md`（143 行）五份责任文件。
> **方法**：只读静态检查。五文件全文通读；全部 file:line 锚逐一开仓核对（HEAD=52adba89，git status 干净 ⇒ 行漂移均系相对已提交树，非未提交改动）；
> 与分歧台账 D-01…D-11、L1（docs/ASTROCS_DESIGN.md）、docs/science/algorithms/*、docs/plugins、docs/contracts、lib|eng 源码、run/ 证据五方交叉比对；
> 题录经 Crossref API/arXiv 抽验。**未运行任何构建、测试或检查器**（纪律），未做任何 git 写操作，唯一写入 = 本报告。
> 上轮《检查-修复验证》PASS 项（k_gauss 表值、UNCERTAINTY :12 N_p²、GATES 六锚、14 条题录核验等）不重复报告。
> **计数：红 4 / 黄 8 / 绿 1**（另有域外附带登记 5 条，不计入）。

---

## 一、问题记录

### 红级

---

**S4-H01**｜严重级：**红**｜所属面：**①②**（专项提示直接命中：D-08 影响行新旧两说）
文件：行：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:48`（及 `:36`）

- **问题描述**：D-08 两因子改写已落位（:49-62），但其正上方 :48 首句仍保留旧归因——把 k_corr 的全部偏离归因于「Drizzle 输出协方差导致的 N_eff < N_retained」；:36 的旧单因子定义 `k_corr = N_retained/N_eff ≥ 1` 也原样存活。同一文档内对同一符号给出两种因果口径（旧：纯 drizzle 相关性；新：k_gauss(N_retained)×k_geo，主因子是有限 N 估计器偏差）。
- **证据（对照来源，含反方核验）**：
  - 现文对照（逐字）：:48「k_corr 表征 Drizzle 输出协方差导致的 N_eff<N_retained。」；:36「…且 k_corr = N_retained/N_eff ≥ 1，见 SCI-UPM §5/§6」；对照 :51-52「**k_corr = k_gauss(N_retained) × k_geo(几何)**（k_gauss = 有限 N 估计器偏置、**主导因子**；k_geo = drizzle 输出像素相关的纯几何因子…）」。
  - 分歧台账 D-08 终裁：k_corr 两因子＝k_shape×k_geo ＋几何查表，1.4 = 代码缺省/域外回退（低估 32%/2 倍）；上轮修复红-4/红-7 PASS 的是 **:49-62 的改写与表值**（1.637/1.316/1.144/1.083/1.046/≈1.00 与 P3 正本一致，五项义务齐），**不覆盖 :48 首句**。
  - 反方核验①：即使把 :48 读作 N_eff 的定义式，其因果断言也不成立——k_gauss(5)=1.637>1 在**无任何 drizzle 相关**时同样使 N_eff<N_retained，故「协方差」不是唯一/主导成因；反方核验②：D-08 首条即强调 k_corr 不在 F&H 1.26 口径内，旧句恰是把 k_corr 当纯相关性量的表述；反方核验③：本条非「漏改写」（改写在），是**旧说残留未删**——即检查点要求核对的「新旧两说」形态。
- **建议改法**：涉及科学归因与冻结定义，按纪律**只登记上呈**：由总编/负责人裁决 :48/:36 的表述如何与 D-08 收敛；本检查不给改写。

---

**S4-H02**｜严重级：**红**｜所属面：**④**（附带②：§13 自称「行号实测」为假）
文件：行：`docs/science/REJECTION.md` **30 行约 45 个锚值**系统性漂移（:43/:45/:46/:51/:53/:100/:114/:127/:131/:134/:153/:154/:157/:158/:159/:165/:167/:205/:209/:214/:223/:224/:253/:266/:267/:268/:270/:271/:273/:274）。

- **问题描述**：本文件是排异域唯一权威页（:252「权威文件: 本文件」），其内嵌实现锚与当前 rejection.cpp/rejection.h 实际行位系统性不符；位移呈规律性（规划区 +6…+17 行、impl/kernel 区统一 +20 行），即源码两次插入后未重锚。:134 更以「（**行号实测**，逐符号锚见 PHASE2_REJECTION §3）」自证，而其三条区间中两条不符。
- **证据（对照来源，含反方核验）**——实测列均为 HEAD 实开仓核对：

| 文档行 | 引用（现文） | 实测（HEAD） |
|---|---|---|
| :43 | `REJECTION.md:16`（自指） | n 行在 :20 |
| :45 | `rejection.cpp:2064-2073` | UNDERDETERMINED 闸 = **:2084-2093**（2064-2073 = INVALID_CONFIGURATION 末段） |
| :46 | `rejection.cpp:1218-1225` | underdetermined 默认 = **:1226-1242**（1218-1225 = profile 非法 set_err+p{} ） |
| :51 | `rejection.cpp:1148-1158 / :1254-1268` | n≤3→none = **:1158-1160**；对照档 percentile = **:1285**（1254-1268 = typed 默认值块） |
| :53、:100 | `rejection.cpp:1275-1283` | 对照档 if 链 = **:1285-1287**（区间仅含注释尾） |
| :114 | impl 族 9 锚（1465-1467/1470-1512/1515-1574/1577-1612/1615-1709/1712-1782/1809-1830/1878-1910/1915-1965） | 实际定义 **:1485/:1490/:1535/:1597/:1635/:1732/:1829/:1898/:1935**——全族 **+20**（同 PHASE2_REJECTION §3；其中 semantic_id :1082-1098 正确） |
| :127、:154 | `rejection.cpp:1860-1874` | 全拒 n≤4 容错 = **:2198-2210**（1860-1874 = sigma-clip 迭代体） |
| :131 | `rejection.cpp:1501-1592 trail 分支` | large_scale = **:2341-2434**；1501-1592 = winsorized 段；仓内无「trail 分支」于此（与 :160 自引 :2342-2433 矛盾） |
| :134 | `1182-1288 / 1809-1830 / 1996-2201`（并称「行号实测」） | 实为 **:1199-1308 / :1829-1850 / :2016-2221**——三条全偏，「实测」声明不成立 |
| :153 | `rejection.h:86` | P2_STATUS_UNDERDETERMINED = **h:112**（:86 = WBPP sha1 注释） |
| :154 | `rejection.cpp:1860-1874` | 同 :127，实为 :2198-2210 |
| :157 | `rejection.h:84` | P2_STATUS_ALL_REJECTED = **h:110**（:84 = profile 注释首行） |
| :158 | `rejection.cpp:2018` | MIN_SAMPLES 判定 = **:2038**（:2018 = 函数签名行） |
| :159 | `rejection.h:108-109`；`rejection.cpp:2036-2062/:2000-2013` | INVALID_CONFIGURATION/METHOD = **h:113-114**（108-109 = OK/MIN_SAMPLES）；INVALID_CONFIGURATION = **cpp:2056-2082**（2000-2013 = extreme_prior_valid 尾） |
| :165 | `rejection.cpp:1809-1830` | percentile 判据 = **:1838-1847** |
| :167 | `rejection.cpp:2038-2045` | MEDIAN_CENTER 强制 = **:2056-2059**（2038-2045 = MIN_SAMPLES/INVALID_INPUT 头） |
| :205 | `rejection.cpp:1148-1158` | astrocs_n_map_method = **:1154-1174** |
| :209、:214 | `rejection.cpp:2185-2195` | n≤4 条件 = **:2205-2210**（2185-2195 = 计数循环） |
| :214 | `rejection.cpp:1818-1830` | percentile 退化行为 = **:1834-1849**（1818-1830 = RCR 尾） |
| :223 | `rejection.cpp:1226-1228` | normalization 默认 = **:1243-1244** |
| :224 | `rejection.cpp:2025-2034` | INVALID_INPUT = **:2041-2054**（2025-2034 = INVALID_METHOD） |
| :253 | `rejection.cpp 1182-1288 / 1996-2201` | 同 :134（1-12、2342-2433 及 h 三段正确） |
| :266 | `rejection.cpp:1665-1668` | linear_fit 平均绝对残差 = **:1685-1688**（1665-1668 = 残差数组初始化） |
| :267 | `rejection.cpp:1236` | ESD alpha/max_outliers = **:1253**（:1236 = `if (pixel_profile) {`） |
| :268 | `rejection.cpp:1237/:1817-1818` | **:1254** / **:1834-1847**（1817-1818 = RCR med_scratch） |
| :270 | `rejection.cpp:1240-1241` | minmax = **:1257-1258**（1240-1241 = underdetermined 赋值） |
| :271 | `rejection.cpp:1785-1806` | reject_rcr_impl = **:1805-1826**（区间为 ESD 尾+RCR 头） |
| :273 | `rejection.cpp:1915-1965` | reject_extreme_prior_impl = **:1935-1985** |
| :274 | `rejection.cpp:1226-1251`（typed 默认值） | 实为 **:1246-1263**——ESD/percentile/minmax/large_scale 四组均在 :1251 之后 |

  - 反方核验①：非「全错」——下列锚经实测**正确**，证明确系漏更而非胡引：`:275`（astrocs_n_map 1153-1176）、`:160/:164/:253`（large_scale 2342-2433）、`:224`（ESD 单 sqrt 1730-1733）、`:235`（rejection_oracle_compare.py:193-215）、`:247`（rejection.h:45-62）、`:114` 的 semantic_id :1082-1098、`:30-32`（hiss_writer.cpp:335-365）、冻结头 :1-12、§9a:275 实测注。
  - 反方核验②：内容层无误——§5 档位/阈值/M3 依据、§8a 等效阈值推导、§17 数值均与代码/证据一致（另见 Y01 的数值归属问题），故本条仅锚面。
  - 反方核验③：同族漂移同时存在于 `docs/science/algorithms/REJECTION_ALGORITHMS.md` 与 `PHASE2_REJECTION.md §3`（域外附带登记 O1/O2）⇒ 非本文件独有，但本文件自证「实测」且为唯一权威页，判红。
- **建议改法**：按上表实测列逐锚改指，:134 的「行号实测」声明重测后方可保留（锚点订正非科学变更，可执行）。

---

**S4-H03**｜严重级：**红**｜所属面：**④③**
文件：行：`docs/science/STAR_DETECTION.md:21`、`docs/science/REJECTION.md:68`、`docs/science/SCIENCE_SCOPE.md:56`（机器侧：`eng/tests/config/test_cfg001_contracts.py:39-49/:218-230`、`eng/packaging/config/defaults.json`）

- **问题描述**：配置合同门 `test_every_source_ref_resolves_and_key_anchors_hold` 以「KEY_ANCHORS（文件:行:token）+ defaults.json source_ref」双登记钉死本片三文件的权威行；按 `cfg_common.grep_line`（单行子串）语义**静态复算，11 条锚中 8 条不成立**（含本片 3 条），其中就包含 `detection.threshold_sigma / rejection.sigma.lower_sigma / precision.default` 三个设计点名项。门所在套件由 `UT-CONFIG`（`eng/ci/checks.json:2763-2792`，**waivable: false**）在 linux-main 跑；`eng/tests/test_index.csv:8` 对该套件登记 FAIL（实测日期 2026-09-23，3 例），此后 09-25/09-27 的文档提交未重测。
- **证据（对照来源，含反方核验）**：
  - 断言语义：`test_cfg001_contracts.py:218-230` → `C.grep_line(path, line, token)`（`eng/tests/config/cfg_common.py:99-103`，单行子串）。测试文件最后提交 ad5a4fb1（09-23），其后 STAR_DETECTION/REJECTION/SCIENCE_SCOPE 分别被 c4af4136/139a2bc4（09-25）、6523b2d8（09-27）等改动 ⇒ 行漂移单向发生于文档侧。
  - 静态复算（同一断言语义，**未执行测试**）：

| 锚 | token | 现文（实印） | 判定 |
|---|---|---|---|
| STAR_DETECTION.md:21 | "5.0" | `σ_smooth = 2.0 px … sdet_api.cpp:1772-1774` | **FAIL**（"5.0" 现位于 :65） |
| REJECTION.md:68 | "4.0/3.0/8" | `与生产掩码 5744/5744 逐点一致…` | **FAIL**（"4.0/3.0/8" 现位于 :116） |
| SCIENCE_SCOPE.md:56 | "FP64" | `故**污染幅度未被本次独立复算定量确认**…` | **FAIL**（FP64 现位于 :91-92） |
| PSF.md:94 | "4" | `⇒ r_win/α = 2.629、f_out = 2.02e-3 …` | FAIL（域外） |
| NOISE_MODEL.md:86 | "rmax" | `### 5a 掩膜半径的物理导出（MASK-002）` | FAIL（域外） |
| NOISE_MODEL.md:23 | "1e-12" | `| x | 像素值（校准后 ADU/e⁻ 空背景） | 输入 |` | FAIL（域外） |
| PHOTOMETRY.md:30 | "3.0 mag" | `| S | MAD(r)/0.6744897501960817 初值尺度 (dex) …` | FAIL（域外） |
| PHASE3_HIPS_TO_FITS.md:41 | "512" | `## 4 输入有效域` | FAIL（域外） |
| PSF.md:9 / PHASE2_UPM.md:24 / DRIZZLE.md:35 | "Moffat4"/"1.4"/"pixfrac" | — | PASS（3 条成立） |

  - 反方核验①：defaults.json 的 source_ref 三行与测试期望完全一致（path/line 双对齐）⇒ 不是登记面内部漂移，而是**登记面整体落后于文档**；反方核验②：token 均仍存在于同文档（见现位行）⇒ 是行漂移不是内容丢失；反方核验③：此为已登记已知红（test_index FAIL），但 09-23 时仅 precision.default 一例，现按现树静态复算为 8/11——失败面已扩大且无人重测。
- **建议改法**：上呈 eng 侧重锚 KEY_ANCHORS 与 defaults.json source_ref（token 现位行见上表），并实跑 `UT-CONFIG` 确认（本检查纪律不跑测试、不改测试/配置）。

---

**S4-H04**｜严重级：**红**｜所属面：**①③④**
文件：行：`docs/science/SCIENCE_SCOPE.md:47-56`（判据与降级合同）、`:73-74`（失效条件重复）；`docs/science/UNCERTAINTY_AND_COVARIANCE.md:46`（同一降级合同的镜像表述，同样无接线）

- **问题描述**：γ 判据被声明为「**可执行、非退化**」的失效判据，并规定「γ 显著偏离 1 ⇒ 噪声场**必须显式降级**（退回已规定的全局常量场并登记 degraded_reason），消费口径 = 显式降级的常量场」。三方面同时出问题：①全仓无任何 γ 计算/γ 触发的降级接线；②`degraded_reason` 词表无噪声域词条、从未登记；③该「结构污染→常量场」触发面与 NOISE_MODEL 的常量场触发白名单**互斥**，而生产实际走的是第三条路（R 判据逐 patch 剔除）。
- **证据（对照来源，含反方核验）**：
  - 接线核验（反方核验①）：`grep -r "dlog(patch" / "γ" lib|eng` 仅命中两份 science 文档；实现仅存在一次性探针 `run/SCI-FIX-SEMANTICS-01/probe/m42_structure_contamination.py`（γ_struct/γ_true 计算，属 run/ 脚本，非生产、非门禁）。noise_model.cpp 无 log-slope 计算。`degraded_reason` 词表 = `docs/contracts/LOG_AND_ERROR_CONTRACT.md:124-125` 五词（photscale_incomplete/photscale_absent/upstream_artifact_absent/optional_keyword_unparsed/cache_miss_recompute），明文「新增词先在本文档登记再使用」——噪声域词不存在；module_adapters.cpp 中全部 degraded_reason 属 detection/photscale/spatial 域。
  - 冲突核验（反方核验②）：`docs/science/NOISE_MODEL.md:294`「**常量场触发条件不变量**：全局常量场**只由**几何退化（控制点近共线）或合格控制点不足触发」、`:68`「几何退化/合格控制点不足 ⇒ …退化为全局常量场（**不得**用作平面缺陷的修复）」、`:266` 同义。SCIENCE_SCOPE 新增的「γ≠1 ⇒ 常量场」是第三触发面，两份权威文档对同一处置互斥。
  - 生产实况（反方核验③）：NOISE_MODEL §5d（:258-264，commit 38f17be5）的结构污染处置 = 逐 patch `R = σ_MAD/σ_white-equiv` 自校准剔除进入平面拟合的控制点——**不是**全局降级；且 m42_structure_contamination.json 存在（γ=1.983 读数有据），故本条不质疑读数本身，只登记「判据/降级合同无接线 + 触发面冲突 + 消费口径与生产实况不一致」。
  - 镜像位置（加强证据）：`UNCERTAINTY_AND_COVARIANCE.md:46` 逐字复述同一合同——「前提是背景在 patch 尺度局部平稳（SCIENCE_SCOPE §假设的 γ ≈ 1 判据）。前提被违反时 … ⇒ 该 patch 的噪声场必须显式降级并登记 `degraded_reason`；消费口径 = 降级后的噪声场」——两份文件共同依赖的降级链在 lib/eng 均无落点（同上反方核验①）。
- **建议改法**：涉及科学判据与冻结失效域，按纪律**只登记上呈**：由总编/负责人裁定 γ 判据的层级（运行时合同 vs 文档级判据）及其与 NOISE_MODEL 触发面/词表的统一方案；本检查不给改法。

### 黄级

---

**S4-Y01**｜严重级：**黄**｜所属面：**②**
文件：行：`docs/science/REJECTION.md:277-280`（§17 表）

- **问题描述**：表中两条 `↳` 子行（3956|60、1655|160）排在「多异常点」行（451|50）之下，实为「显著点」行（5611|220）的 n 分解——与父行数值矛盾。
- **证据**：算术自证：3956+1655=5611、60+160=220（= 显著点行），而 451 的分解对不上任一子行；`run/REJECT-DOCFIX-01/evidence/m3_m5_metrics.json` populations 给出两组独立分解（sig_ge16=1655 / prod_missed_sig220；multi_anomaly_ge16=**161|40**）；跨文档旁证：`docs/science/algorithms/PHASE2_REJECTION.md:809` 将同组 60/3956、160/1655 明确归入显著点读数「其中 n≥16…」。反方核验：全部数值本身正确、与证据一致 ⇒ 仅归属/缩进错误，不涉公式与容差。
- **建议改法**：两 `↳` 行移入显著点行，或加表注点明母行（行文修复，数值不动）。

---

**S4-Y02**｜严重级：**黄**｜所属面：**②**
文件：行：`docs/science/REJECTION.md:144`（及 :125、:153 表行）

- **问题描述**：「UNDERDETERMINED 单调性：**n ≤2 恒** UNDERDETERMINED，不做剔除」写成绝对不变量，但同文件 :47-48 明确给出反例配置：生产档且 `request=extreme_value_clip_prior_sigma` ⇒ underdetermined_n=1（调用方显式传值亦可 ≤1）⇒ n=2 进核、可被剔。
- **证据**：文档内互证（§4 vs §7/§5/§8）；代码：`rejection.cpp:1236-1238`（pixel+EXTREME 默认 1）、`:2087`（闸 = `n<=underdetermined_n(1) || n<minimum_n`）、`:996`（method_minimum_n EXTREME=2）；代码注释 :1228 自述「使 n=2 能进 kernel，n=1 由 minimum_n=2 拦下」。反方核验：生产默认路径（AUTO/常规显式，undet=3/2）下 n≤2 确实恒 UNDERDETERMINED ⇒ 不误生产，但「恒」在本文件自己列明的显式档下不成立。
- **建议改法**：属独立不变量措辞与例外范围的两说——按 §10 变更流程（:362）由维护者择一收敛（限定范围或点名例外）；本检查不代拟。

---

**S4-Y03**｜严重级：**黄**｜所属面：**④③**
文件：行：`docs/science/STAR_DETECTION.md:21/:66/:68/:79/:117/:140/:146`

- **问题描述**：7 处实现锚相对 HEAD 漂移；其中阈值锚与上轮已按黄-3f 订正的 `GATES_AND_TOLERANCES.md:40-41` 形成同事实两行号。
- **证据**：

| 文档行 | 引用 | 实测 |
|---|---|---|
| :21 | `sdet_api.cpp:1772-1774`（σ_smooth 常数） | **:1761/:1763**（GATES:41 已订正为 :1761 ⇒ ③两说） |
| :66（及 :64 阈值条） | `sdet_api.cpp:1783` | 阈值行 **:1779**（GATES:40 已订正为 :1779 ⇒ ③两说） |
| :68 | `sdet_api.cpp:1856-1857`（smooth > threshold） | **:1845-1846** |
| :79 | `sdet_api.cpp:1834-1837`（dynrange/norm） | **:1818-1824**（1834-1837 = dbg 帧计数器） |
| :117 | `sdet_detect_impl … sdet_api.cpp:1599-2353` | **:1738-2483** |
| :140 | `sdet_api.cpp:1783`（bgnoise） | **:1772** |
| :146 | `wrapper_phase1/star_detector.cpp:30-70` | estimate_background = **:75 起**（:105 为 1.4826·MAD 行）；:30-70 为整段 RETIRED 注释块，不含实现 |

  - 反方核验：内容层全部为真（阈值式 median+5.0·bgnoise、单位 ADU、35.45σ_smooth/7.09× 推导、双 σ 估计器 1.5012 比值等均复算通过）；上轮只订了 GATES 六锚，STAR_DETECTION 不在修复面 ⇒ 非重复报告。
- **建议改法**：逐锚改指上表实测行（同 ① 对照行同步，消除与 GATES 的两说）。

---

**S4-Y04**｜严重级：**黄**｜所属面：**④**
文件：行：`docs/science/SCIENCE_SCOPE.md:81`、`:93`

- **问题描述**：两处证据锚指向过期行位。
- **证据**：`:81` 引 `noise_model.cpp:618`（RANSAC 经验噪声链）——实际 `out_model->source = 0; // empirical blank-sky` 在 **:1165**（:618 处为同函数早段）；`:93` 引 `module_adapters.cpp:6355-6377`（分块 dtype 由 precision 决定）——实际 dtype 选择在 **:7766-7793**（6355-6377 为 NOISE-MODEL-CANON-002 注释区）。反方核验：`:96` 的 `synthetic_gate.cpp:2849`（测试 Phase2Upm.SparseEqualsDense）**正确**；两条声明的语义本身为真 ⇒ 仅行号漂移。
- **建议改法**：改指 :1165 与 :7766-7793。

---

**S4-Y05**｜严重级：**黄**｜所属面：**④**
文件：行：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:13`

- **问题描述**：噪声律与声明锚过期。
- **证据**：`noise_model.cpp:919` — snr_noise_scale_law 实现（含「无条件 var×=α²」）实际在 **:1463-1467**；`snr_estimator.h:256` — 声明实际在 **:277-279**（:256 为 variance_floor 注释）。反方核验：本文件其余锚全部正确（:5 drizzle_engine.h:56、:9 hiss_writer.cpp:335-365、:10 drizzle_engine.cpp:1645-1647、:55 astro_sphere_sink.h:33/:45、:56 p3_rsmp_covariance.cpp:22-45、:57 p3_rsmp_propagation.cpp:101,108、:60 p3_v6_export.h:129-132、:67 sampler.cpp:83/:874-875），非全错；同对锚亦存于域外 `NOISE_ESTIMATION.md:148/:200`（附带 O3）。
- **建议改法**：改指 :1463-1467/:277-279（域外同漂一并上呈）。

---

**S4-Y06**｜严重级：**黄**｜所属面：**④**
文件：行：`docs/science/STAR_DETECTION.md:64-66`（:141 未补注）

- **问题描述**：文档以 `detection.threshold_sigma` 为全局检测阈值的配置键身份，但未披露该键**已登记为死键**（生产路径不消费）。
- **证据**：`eng/ci/prod_wiring_baseline.json:8` 冻结死键记录 `"dead_config_key:detection.threshold_sigma"`；消费侧全仓 grep：`sdet_api.cpp:1779` 硬编码 `T(5.0)`、`phase1_product.cpp:448` 硬编码 `set.selection.detection_threshold_sigma = 5.0`，lib/ 无任何从 defaults.json 读取该键的路径；doc 侧无「未接线」字样。反方核验：键值 5.0 与文档/冻结门一致、无单位换算（上轮绿-10 已核）⇒ 不是「值错」，是**披露缺口**（属④专项「配置死键」形态）。附带：`config_registry.md:308` 的 `sdet_api.cpp:1782-1792` 锚亦已漂移。
- **建议改法**：在 :64 条目补注「该键已登记但当前生产路径未消费（CHK-PROD-WIRING 冻结发现）」——披露性补注，不动阈值语义。

---

**S4-Y07**｜严重级：**黄**｜所属面：**②**
文件：行：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:12`

- **问题描述**：同一行内嵌套两个 `<!-- … -->`（R-1 外层注中嵌 Y-3d 内层注）。按 HTML 注释解析，外层注在**内层的 `-->` 处提前闭合**，其后尾文将以**可见正文**渲染——漏出的恰是被 R-1 判废的旧公式。
- **证据**：L12 含 2 个 `<!--`；渲染后可见尾文 = `）。旧对照：var_p = Σ v_j w_jp² / D_p²；分母 D_p = Σ a_jp -->）。`——即旧 D_p² 口径（R-1 已裁「以 D_p² 计会把方差高估 1/pixfrac⁴」）以正文形式出现。反方核验：raw markdown 中主句语义仍正确（订正主句在注外），故非公式错误；上轮黄-2 对 PHASE2_UPM_IMPL 同类嵌套注判 PASS（建议整理）⇒ 同族新实例，且此处漏出内容含被废公式，取黄不取红。
- **建议改法**：拆为两个并列注，或将内层旧对照并入外层注（整理类，不改公式）。

---

**S4-Y08**｜严重级：**黄**｜所属面：**④**
文件：行：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:159-160`

- **问题描述**：Cramér (1946) 给出节号+页码的**逐字引文**（§28.5「The quantiles」pp.367-369），未按台账/简报要求加未核标注；同注的「未独立验证」名单只有 Kendall&Stuart 与 Hoaglin。
- **证据**：`分歧台账.md:226`：P2 路线3 ① =「Cramér §28.5 逐字引文」属外部材料缺失型开放项；`五单元成稿简报.md:89`：「Serfling 1980 / Cramér §28.5：书目级（**小节号 UNRESOLVED——标注后引用**）」、`:93` 仍开放清单含「Cramér/Serfling/Cadwell 题录细节」。反方核验：引文公式本身独立可证（正态样本中位数渐近方差 πσ²/2n ⇒ σ√(π/(2n))，解析复算成立）；上轮 14 条题录核验不含 Cramér ⇒ 非重复报告。
- **建议改法**：按简报:89 加「小节号/页码未独立核验」标注（题录标注级，不动公式）。

### 绿级

---

**S4-G01**｜严重级：**绿**｜所属面：**④**
文件：行：`docs/science/SCIENCE_SCOPE.md:34`（及 :94 同值）

- **问题描述**：α 区间实测证据指向 `run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`，该 `out/` 产品树已按 run/ 回收策略清出，指针当前不可打开。
- **证据**：`find run -name p1_phot.json` 无 RELEASE-05 路径（run/RELEASE-05 现存 evidence/ 等，vis/out 已回收）；同族可持久证据均在（`run/REJECT-DOCFIX-01/evidence/*`、`run/SCI-FIX-SEMANTICS-01/evidence/*`）。反方核验：α ∈ [1.1387e-17, 6.0083e-17] 在 :34/:94 两处自洽，且 out/ 本为可再生、按策略不入库 ⇒ 不判黄。
- **建议改法**：可选：后续修订时把易失效指针改指可持久证据或探针脚本；不改不影响判定。

---

## 二、域外附带登记（确凿、同根因；不在本片五文件，不计入计数）

1. **O1（④）**：`docs/science/algorithms/REJECTION_ALGORITHMS.md` 同族锚漂移——:31（h:94-99/:102-111）、:33-35（1218-1225 / 1860-1874 / 1148-1158）、:59（2064-2073）、:62/:64（2018 / 2025-2034 / 2036-2062）、:70（1182-1288）、:84 impl 族（+20）。
2. **O2（③④）**：`docs/science/algorithms/PHASE2_REJECTION.md` §3 自称「（实测）」但锚同族 +6…+20 漂移；**:90** astrocs_n_map_method 语义仍写「else linear_fit」（M3 前旧口径），与同文件 :211/:569 及 REJECTION.md:65 矛盾；:66-68 自述行数 2950/595 实为 2956/604。REJECTION.md:134 直接指向该节 ⇒ 影响我方权威页的可用锚源。
3. **O3（④）**：`docs/science/NOISE_ESTIMATION.md:148/:200` 同引 `noise_model.cpp:919-926`（实为 :1463-1467）。
4. **O4（④）**：test_cfg001 其余 5 条锚（PSF.md:94、NOISE_MODEL.md:86/:23、PHOTOMETRY.md:30、PHASE3_HIPS_TO_FITS.md:41）同判 FAIL（见 H03 表）。
5. **O5（④）**：`eng/packaging/config/config_registry.md:308` 的 `sdet_api.cpp:1782-1792` 已漂移（实为 :1818-1824，阈值 :1779）；`GATES_AND_TOLERANCES.md:41` 订正注内嵌「bgnoise 计算 :1770」实为 :1772。

---

## 三、已查无问题面

### ① 科学性面（常数/公式/单位/适用域 vs 分歧台账终裁；非退化判据）

**已查内容与方法**：五文件逐行通读；对每个数值常数/公式做独立复算或与 D-01…D-11 终裁、L1、P3 正本、run/ 证据比对。
- **D 系残留扫描**：五文件 grep `1.144/0.104369/8.5%/1.044/k_shape/1.26/rel_step_max/5.07/36.31%/15%` —— 仅命中 UNCERTAINTY :52/:53 订正注内的「旧对照」留存（合规形态），正文无 D-01/D-05/D-07/D-09/D-11 的作废值；k_gauss 表 1.637/1.316/1.144/1.083/1.046/≈1.00 与 P3 正本逐值一致（上轮红-7 PASS，本轮复核未回潮）。
- **STAR_DETECTION 复算**：`T = median + 5.0·bgnoise`；归一常数 `‖k‖₂ = 1/(2σ√π) ⇒ 5.0/‖k‖₂ = 5.0·2σ√π = 35.45σ_smooth`（2.0 px 下 = 7.09×输入阈）逐位复算通过；召回表 CP 界 0.05^(1/n)（n=100→0.9705、20→0.8609、10→0.7411、5→0.5493、2→0.2236）复算一致；99% 阈（7.09±0.45 等）与 κ 经验拟合复算 = 文档值（κ99(4.29)=7.79±1.34、κ99(2.65)=7.27±0.45、κ50=6.21±0.50，残差 25.1%/9.2%）；`SNR_det = A_fit/sqrt(Σ_box(σ_bg²·(1+SNR_bg²/N_pix)) + σ_read²)` 与 ALG §5 一致；双 σ 估计器 13.8148/20.7384 → 比值 1.5012 复算成立。
- **REJECTION 复算**：§8a 等效阈值 z_low/z_high = 0.2/0.1·|median|/s（48.8–51.4 ⇒ 9.8–10.3 / 4.9–5.1 逐位对）；`|median|/s≈33 ⇒ z_high≈3.3` 惰性带、band≈10 过拒条件复算一致；M3 实测（12.200%→0.067%、3.92%→1.44%、27/1175→0/1175、11.36%/4.95%、E[k]=0.538、P(k=10)=0.0220、3.47%/10.07%、5.167%/2.5851%、142/6000、79.13%/29.39%、74.4%/15.4%/75.5%、373426 像素）逐一与 `run/REJECT-DOCFIX-01/evidence/{m3_m5_metrics.json, clean_reason_probe.json}` 与 REPORT.md 对上；Siril 等价式代数验证（`v < median(1−plow) ⇔ v−median < −plow·|median|`，median>0）。
- **SCIENCE_SCOPE 复算**：`SNR_flat²(S) = S/var`、`SNR_info² = a²PᵀC⁻¹P` 与 NOISE_MODEL §3/UNIFIED §4 一致；γ 判据所引读数有 probe 证据（接线问题另立 H04）；FP32 下溢/上溢推演正确（1e-12×α²≈1.3e-46 < FP32 最小次正规 1.4e-45 ⇒ 精确 0；1/1.3e-46≈7.7e45 > 3.4e38 ⇒ +inf；发布对 (0,0) 合法，(0,+inf) 不可表示）与 NOISE_MODEL §7 成对不变量一致。
- **UNCERTAINTY**：k_gauss 表、k_geo 查表、1.4 回退语义、`k_corr>1 显式拒 rc=2 / <1 拒 rc=1|rc=7`、`norm = (sumVarNum·k²)/D_p² = sumVarNum/N_p²`（k=D_p/N_p=pixfrac²，复算 1/pixfrac⁴=2.44@0.8）、`var_p = Σ c_j²v_j / (1−λ)²` 与 DRIZZLE 正本一致（均上轮 PASS 项，本轮未见回潮）。
- **UNIFIED**：§4 GLS 推导（Q/W、Var=1/W、SNR²=F_ref²W）与 §5 GLS、PSF_SIGNAL_WEIGHT 的 `W_info = a²PᵀC⁻¹P ≡ Σ a_j²/σ_j²` 解析一致；文献（Tonry/Ivezić/Aitken/Horne/Naylor/Z&O/F&H）题录成立。
- **结论**：除 S4-H01（k_corr 旧归因两说）、S4-H04（γ 判据接线与触发面冲突）、S4-Y08（Cramér 标注）外，**本面无其他问题**。

### ② 行文逻辑面（断链/内部矛盾/订正注新旧两说/UNRESOLVED 混入）

**已查内容与方法**：五文件全文通读 + 定向 grep。
- `UNRESOLVED/PENDING/TODO/待裁/待定/未决/存疑` 在五文件正文中 **0 命中**（无把未决事项写成结论的形态）；台账开放项（1.152 补登、外部材料 6 项、被估量 y 语义）均未被五文件当作已裁结论引用。
- 订正注记：仅 UNCERTAINTY 含 `<!-- 订正: -->`（7 开 7 闭配平），逐一阅读——:12（嵌套泄漏，已立 S4-Y07）、:13/:16/:22/:49-50/:52/:53 各注「旧对照」均收于注内、主句唯一；其余四文件无订正注 ⇒ 无其他新旧两说。
- REJECTION 内部链 §4 路由/闸 → §5 档位与 M3 依据 → §7 不变量 → §8/§8a 极端与等效阈值 → §9a → §10 不可接受变化 → §13 追溯 → §16 对照归属 → §17 实测：数值引用互洽（§8a 引 §5 表、§16 引 §8a/§17、§17 引证据 JSON 全部对上）；「采纳 WBPP 档界 6/15」与 M3 后全 N≥6 同档的表面张力已由 :368 显式自洽说明（「M3 后 6/15 不再划分本表档位」）⇒ 不构成问题。
- SCIENCE_SCOPE/UNIFIED：目标分层→假设→失效条件→不保证/仅声明 的叙述无前后矛盾；UNIFIED §4.1「单一口径无可选项」与 §3 表「source SNR 不直接作权重」经 `F_ref²` 换算条款自洽（先例：上轮跨文档红-1/绿-6 已判 PASS，本轮未见回潮）。
- 除 S4-H01（:48 vs :51 两说）、S4-Y01（§17 表归属）、S4-Y02（n≤2 恒 vs §4 例外）、S4-Y07 外，**本面无其他问题**。

### ③ 跨文档冲突面（同量不同口径；L1↔docs↔实验↔审计↔源码五方权威链）

**已查内容与方法**：以「同一数值/符号在两处以上出现」为清单逐一对照。
- SCIENCE_SCOPE 处理链（§2 五步）↔ L1 §2 五创新点 P1–P5 ↔ 实验五单元成稿主张（光度星等/绝对 SNR/守恒映射/稠密重建/纯加性天光）：对齐。
- UNIFIED §4.1 单一权重口径 ↔ L1 §2/§3.1「阶段一只生产信噪比」↔ FZ-MODE-RETIRED/FZ-FIELD-WEIGHTMODE/FZ-WEIGHT-SINGLE-PATH 三 fail-closed token（`lib/infrastructure/cli/v6_runtime_contract.h` 实在）↔ science_objective（`phase2_integrate.cpp:572` 写入 `point_source_flux_information`，schema :2225-2231 `fail_closed: 未声明 ⇒ REJECT`）：全链一致。
- REJECTION ↔ `docs/science/algorithms/REJECTION_ALGORITHMS.md` ↔ `docs/plugins/algorithms_phase2/12_rejection.md §9` 档位表 ↔ 源码：underdetermined 默认 3/1/2（cpp:1235-1242）、M3 改投（cpp:1163-1173）、percentile 0.2/0.1（:1254）、winsorized 4.0/3.0/8（:1247-1248）、ESD 0.05/10（:1253）、minmax 1/1/4（:1257-1258）、large_scale 关/8/2（:1260-1263）、生产 profile 默认 `astrocs_adaptive_pixel`（stage2_common.cpp:276-278）、`extreme_value_clip_prior_sigma` 枚举（h:61）与「永不参与 AUTO」（cpp:1001-1006 + 注释）逐项对齐；`REJECTION_ALGORITHMS:35-37` 的 3/1/2 与本片 :46-48 一致。
- STAR_DETECTION ↔ ALG §11.4 F1 召回/虚警表逐格相同；↔ GATES §2 门表（1786/2616 px²、0.15 px、0.35 FWHM、0.3 px、5.0σ、1e-4/帧、100/千px、3σ_CD、0.991）逐项一致；↔ DATA_SEMANTICS/PSF 的 1.230310、1.9140×、0.902 一致（歧义仅在已立 Y03 的行号）。
- UNCERTAINTY ↔ DRIZZLE（N_p² 归一、对角存储、相关核/λ、F&H 1.26 边界）↔ `p3_v6_export.cpp:285-286` correlation_kernel 落盘 ↔ NOISE_MODEL（方差三态/成对不变量/α² 换算）一致。
- 五单元成稿简报所列本片订正（简报:238「UNCERTAINTY:48-49 k_corr 两因子+1.3883 改写（D-08，已批）」）——**改写已在 :49-62 落位**，简报所述成立；残留两说另立 H01。
- 除 S4-H04（SCIENCE_SCOPE vs NOISE_MODEL 常量场触发面互斥）、S4-H03（docs↔eng 机器权威锚失配）、S4-Y03（与 GATES 行号两说）及附带 O1/O2 外，**本面无其他问题**。

### ④ 幻觉与锚面（文献抽验、锚点逐一开仓、「文档说有代码没接」）

**已查内容与方法**：
- **文献抽验（本轮新核 5 条，经 Crossref/期刊源，均真）**：Tonry et al. 2012 ApJ 750,99（DOI 10.1088/0004-637X/750/2/99）；Ivezić et al. 2019 ApJ 873,111（10.3847/1538-4357/ab042c）；Newberry 1991 PASP 103,122（10.1086/132801）；Young & van Vliet 1995 Signal Processing 44,139-151（10.1016/0165-1684(95)00020-E）；Fernique et al. 2015 A&A 578 A114（10.1051/0004-6361/201526075）。上轮已核 14 条（Rosner/Maples/Beaton&Tukey/ZOGY/F&H/Aitken/Stetson 等）不重复。Cramér 一条另立 Y08。
- **锚点开仓核对（全部逐一读取，非抽样）**：UNCERTAINTY 9 处 P3/实现锚 ✓（除 :13 两锚 → Y05）；REJECTION 全部实现/枚举/测试锚（→ H02 表 + 其中正确项清单）；STAR_DETECTION（→ Y03）；SCIENCE_SCOPE（→ Y04，synthetic_gate:2849 ✓）；UNIFIED 无行号外锚（节指针 §2/§3.1/L1 实在，FZ/science_objective token 实在）。正确锚均附实测验证（见 H02/Y03/Y04/Y05 反方核验列）。
- **「文档说有、代码没接」专项**：FZ 三 token ✓ 接线；science_objective ✓ 写入+schema 强制；correlation_kernel ✓（p3_v6_export.cpp:285-286、v6_calibration_covariance.cpp）；`extreme_value_clip_prior_sigma` ✓ 真实枚举且永不进 AUTO；`underdetermined_n` 显式传值通道 ✓（cpp:1240-1242）；生产 profile 默认 ✓（stage2_common.cpp:276-278）；`detection.threshold_sigma` ✗ 死键未披露 → **Y06**；γ 判据/降级合同 ✗ 无接线 → **H04**；KEY_ANCHORS 权威行 ✗ 失配 → **H03**。
- **run/ 证据可得性**：REJECT-DOCFIX-01 与 SCI-FIX-SEMANTICS-01 的 evidence/报告/脚本全部在库可开仓（本轮据其完成数值核对）；仅 RELEASE-05 vis/out 产品树按回收策略清出 → 绿 G01。
- 除已立 H02/H03/H04、Y03–Y06、Y08、G01 及附带 O1–O5 外，**本面无其他问题**。

---

*纪律执行声明：本报告为本次排查唯一写入文件；全程零 git 写（无 commit/push/stash/checkout）、零构建、零测试执行（H03 的门判定采用同断言语义静态复算并如实标注）；涉及科学公式/默认容差/冻结定义的红级问题（H01、H04，及 H02 中「实测」声明、Y02 不变量措辞）只登记上呈，未给出越权改法。*