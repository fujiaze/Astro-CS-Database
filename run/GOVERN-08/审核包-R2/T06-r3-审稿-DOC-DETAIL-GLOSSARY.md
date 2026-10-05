# T06 第 3 轮独立审稿 · 跨切面车道 DOC-DETAIL-GLOSSARY

**车道**：`docs/detail/**`、`docs/GLOSSARY.md`、`docs/README.md`、`docs` 顶层（`docs/ACSD_DESIGN.md`），以及**跨全部正本的一致性**。
**基线**：`18dd6c64 把重复断言收敛为唯一正本`（即「止血」那一轮）。工作树本车道范围内无未提交改动。
**只读**：全程零 git 写操作（git 仅只读，中文路径一律 `git -c core.quotepath=false`），零文档修改。**本文件是本车道唯一新增件。**
**L.key**：沿用前两轮 `DOC-DETAIL-GLOSSARY`。

> **⚠ 一处须前台知悉的越界**：子代理 C（跨切面数据对象）在交件时自行新建了
> `run/CROSSCUT-13OBJ-AUDIT/跨切面一致性-13对象×4层-审计报告.md`。
> 我派单时写的是「不要修改任何文件」，该子代理仍建了新报告。零 git 写、零既有文件修改，
> 但**这超出了我给它的写面**，请前台决定该文件去留。我未采信其未经我复核的数字（见 §7）。

---

## 1 读完了吗

| 项 | 数 |
|---|---|
| 本车道文档总数 | **49**（`docs/detail/**` 47 份 `.md` + `docs/GLOSSARY.md` + `docs/README.md`） |
| **我自己逐行读完** | **14 / 49**（`AGENTS.md` 127、`ACSD_DESIGN.md` 570、`GLOSSARY.md` 32、`docs/README.md` 12、`00_INDEX.md` 79、`common.md` 72、`UNIFIED_MODEL.md` 135、`PHASE1_DETAILED_DESIGN.md` 227、`PHASE2_DETAILED_DESIGN.md` 145、`PHASE3_DETAILED_DESIGN.md` 173、`merged_TROUBLESHOOTING.md` 85、`21_observability.md` 177、`acsd.phase2.sample.md` 224、`NOISE_SNR.md` 300–379 段）—— 全部**逐行读完**，非抽样 |
| **我逐段读完 + 动手的** | `LOG_AND_ERROR_SYSTEM.md`（未通读，见下）、`19_runtime.md`（读关键 14 行 + 全量 grep 定位）、`acsd.phase1.noise-snr.md`（读 60–194 段 + 全量 grep）、`acsd.phase2.upm-fit.md`（读 60–110 段 + 全量 grep）、`acsd.phase2.integrate.md`（读 60–70 段 + 全量 grep）、`acsd.phase3.wcs.md`（读 56–75 段） |
| **我机器全量对账 + 逐条回读的** | `docs/detail/**` 全部 47 份的**仓内路径引用**（733 处引用逐个 `test -e`）、`GLOSSARY.md` 21 条锚（逐条解析标题集）、`registry/*.md` 26 份、`infrastructure/*.md` 7 份、`anchors/*` — 这些走的是「先脚本定位、再逐条打开目标文件读内容」 |
| **未逐行读完的（逐份列出）** | `LOG_AND_ERROR_SYSTEM.md`(214)、`PRODUCT_STORAGE_FORM.md`(263)、`STAR_DETECTION_IMPL_DESIGN.md`(511)、`anchors/ANCHOR_CONTRACT.md`(152)、`infrastructure/{17_aio,18_cli,20_benchmark,22_gaia_xpsd_client,23_hips_browser}.md`、三个 `README.md`、`registry/` 余下 21 张卡。**理由**：我的四个子代理分别覆盖了路径/结构/语言轮、公式重推轮、对象口径轮、术语表轮；我自己把预算集中在本车道**正本**（`UNIFIED_MODEL` / 三份 PHASE 详细设计 / 顶点 / GLOSSARY）与**止血直接触碰的 15 个文件**上。**这些未逐行处不能算本轮已逐行覆盖。** |
| 跨片对照面（我按需读段） | `docs/science/noise_snr/NOISE_SNR.md`（§3.4 全读）、`docs/engineering/UNIFIED_OBJECTS.md`（标题集 + §1/§2）、`eng/contracts/schemas/unified/{frame_snr,point_information}.schema.json`、`eng/contracts/{resource_gate_v1.json,data/phase_product_exchange.schema.json}`、`eng/contracts/schemas/cpu_profile.schema.json`、`lib/infrastructure/pipeline/module_ports.registry.json`、`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`、`lib/infrastructure/scheduler/src/module_adapters.cpp`、`lib/infrastructure/scheduler/src/mosaic_window.cpp` |
| 必读件 | `AGENTS.md`（127 行全文）、`docs/ACSD_DESIGN.md`（570 行全文）、标准 `03_READING_AND_ADVERSARIAL_REVIEW.md`（55 行全文）、标准 `04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`（71 行全文）、`T06-r2-审稿-DOC-DETAIL-GLOSSARY.md`（176 行全文）、`T07-重复断言收敛-detail-glossary.md`（483 行全文） |

---

## 2 止血效果抽查

### 2.1 逐条判定（T07-detail 的 22 条，我逐条开目标文件核实）

| # | 主题 | 判定 | 我的证据 |
|---|---|---|---|
| D-01 | 进程退出码落点 | ✅ **有效** | `19_runtime.md:13`/`:116`、`21_observability.md:52`、`LOG_AND_ERROR_SYSTEM.md:133` 四处现均指 `docs/engineering/standards/ERROR_MODEL.md`「进程退出码」一节；同句真正的域→码表已分名回 `LOG_AND_ERROR.md`「错误对象与退出码映射」。**车道外 2 处未改**（`PUBLIC_API.md:1215` 仍指 `LOG_AND_ERROR.md`「验收」一节，该文件**无「验收」节**） |
| D-02 | `point_information` 名与 `a_k²` | ✅ **有效** | `PHASE1_DETAILED_DESIGN.md:186/:210` 现为 canonical 名 + 指回本文件 `:173` 的带因子式；全仓 `point_source_information` **0 命中**（我 grep） |
| D-03 | 稀疏层「默认产出」 | ⚠ **部分有效** | 副本确实改指了；但**顶点 `:231` 未改**（见 N-R3-04），`:138` 的「现行默认」措辞与 `:62` 的「产者未落地」在同一张卡内并存 |
| D-04 | `m_ref` 冻结措辞 | ⚠ **部分有效** | `UNIFIED_MODEL.md:76` 已正确改写；但**同一小节 `:69` 仍逐字写「冻结为固定参考星等档」**，与 `:76`「故它不是冻结常数」自相矛盾（见 N-R3-13） |
| D-06 | README `§0` 机械锚 | ✅ **有效** | `docs/README.md:3` 现为「《ACSD 最高设计》的「文档权威与索引」与「文档写法」两节」，两节名在 `ACSD_DESIGN.md:5`/`:30` 真实存在 |
| D-07 | GLOSSARY 权威范围 | ✅ **有效（但交付不完整）** | `:5`/`:8`/`docs/README.md:10` 均已降级为「叫法与单位口径的登记面」+「不收录全部核心术语」。但 13 个 canonical 对象只收了 **5 个**，恰漏 `validity`/`rejection` 等歧义风险最高者（见 N-R3-12） |
| D-08 | 20 条锚点改指 | ✅ **有效** | 我写解析器逐条打开目标文件比对标题集：**20/20 全部命中，0 悬空**；`grep -c '§' docs/GLOSSARY.md` = **0**（我复跑） |
| D-09 | 附录 A 漏 `depth_m5` | ✅ **有效** | `ACSD_DESIGN.md:564` 已含 `depth_m5`，与 `:151` 的 13 个对象一致 |
| D-11 | O3 检测阈值落点 | ✅ **有效** | `PHASE1_DETAILED_DESIGN.md:112` 现指 `docs/detail/STAR_DETECTION_IMPL_DESIGN.md`「O3 检测阈值」一节，节名真实 |
| D-12 | 面亮度单位推导落点 | ✅ **有效** | `PHASE2_DETAILED_DESIGN.md:11` 现指「面亮度单位的推导」一节，真实存在 |
| D-13 | HEALPix 索引落点 | ✅ **有效** | `common.md:32` 现指「坐标语义」一节，真实存在 |
| D-14 | export 输入语义守卫落点 | ✅ **有效** | `PHASE3_DETAILED_DESIGN.md:15` 现指「投影算法」一章；被删的「输入语义守卫」与「硬约束」两条职责**确在同一句内**（`ACSD_DESIGN.md:320`），信息未丢 |
| D-15 | G-RES-01 判据章落点 | ✅ **有效** | `19_runtime.md:15`/`:69`/`:95` 三处现均指真实节名 |
| D-16 | 排障表入口节名 | ✅ **有效** | `merged_TROUBLESHOOTING.md:24` 现指「症状主表」一节；七项/五列的分名说明在 `:12–:15` |
| D-17 | 创新点小标题 | ✅ **有效** | 两张卡抬头已改指顶点正名 |
| **D-18** | **registry 体例描述** | ❌ **半改，且自证失真** | `00_INDEX.md:33` 现写「会话模块 `acsd.phase{1,2,3}.session` 与 `acsd.phase2.resample` 不在生产端口注册表的 `module_id` 集合内」。**实测（python3 解析 `module_ports.registry.json`）：注册表 20 个 `module_id`、卡 25 张、有卡不在注册表 = 5 张 = `[acsd.phase1.hips-writer, acsd.phase1.session, acsd.phase1.star-detection, acsd.phase2.resample, acsd.phase2.session]`。**⇒ 该句点名的 4 张里，`acsd.phase3.session` **根本没有卡**（也不在注册表），而真实差集里的 `acsd.phase1.hips-writer` 与 `acsd.phase1.star-detection` **一个字都没点**。T07 §4-f 自证「改后点名这 5 张」**不成立**（见 N-R3-06） |
| D-19 | star-detection 归属 | ✅ **有效** | `acsd.phase1.star-detection.md:13` 已改为三层分名（卡片名/文档路径/端口 `module_id`） |
| D-20 | 面亮度星等 `Ω_ref` 符号 | ✅ **有效（我独立复算）** | `PHASE1_DETAILED_DESIGN.md:161` 现为减号。见 §3 的推导 |
| D-21 | 球面残差律阈值 | ✅ **有效（我独立复算）** | `acsd.phase1.drizzle.md:122–123` 已改为由表自推的 7″/217″/688″。见 §3 |
| **D-22** | **两档权重式「对账」** | ❌ **方向错，且新造矛盾** | 对账的**代数**对（我重推过），但把 `scope` 枚举的两个值**误读成两个归一化分母档**，等于给 science 正本逐字禁止的写法发通行证。见 N-R3-03 |

### 2.2 止血那轮是否又制造了新矛盾：**是，1 处硬矛盾 + 1 处自证造假**

**① `21_observability.md:109` —— T07 声称改了，实际一个字没改。**

T07 §3-5「改动前后逐字」写：

```
- **exit 10（RESOURCE）在资源门判定域内的充分条件**：判定域内 ①②③ 任一违约且处于 enforce 面
+ **exit 10（RESOURCE）在资源门判定域内的充分条件**：判定域内 ② 任一违约且处于 enforce 面
```

我跑 `git -c core.quotepath=false diff c0bec7f7 18dd6c64 -- docs/detail/infrastructure/21_observability.md`：

```
2	2	docs/detail/infrastructure/21_observability.md
```

**全文只改了 2 行**（`:52` 的 D-01 改指、`:97` 的方向词）。`:109` 逐字未动，现仍写「判定域内 **①②③ 任一违约**且处于 enforce 面」。而同一文件 `:89` 写 ① 是 `record_and_justify（合同未给该判据 enforcement 键）`、`:91` 写 ③ 同、`:97` 写「七项中**只有 ② 在合同里带硬失败执行面**…**任何资源判据都不改变程序退出码**」。

⇒ **同节内部 `:109` 与 `:89/:91/:97` 直接矛盾**（R2 的 N10 至今未闭合），**且 T07 的「改动前后逐字」表里出现了一次从未发生的改动**。这直接回答前台的问题：**止血的自证表不可逐条采信**。

**② D-22 的对账把一个科学禁令改写成了「两档合法写法」。** 见 N-R3-03。

### 2.3 删副本处的信息落点（抽 4 条逐条给行号）

| 删副本 | 唯一正本落点（我打开核实） | 信息是否在位 |
|---|---|---|
| D-02：`PHASE1_DETAILED_DESIGN.md:186` 删 `W_psf = PᵀC⁻¹P`（缺 `a_k²`） | 同文件 `:173` `W_psf,k(x,y) = a_k(x,y)^2 P_kᵀ C_k⁻¹ P_k = 1 / Var(F_hat_k)`，`:179` 另给白噪声闭式 | ✅ 在位 |
| D-03：两处副本删「生产侧产者状态」 | `acsd.phase1.noise-snr.md:62`（输出面表，逐字「交付状态 = 合同面已冻结、生产侧尚无产者」）与 `:384` | ✅ 在位 |
| D-14：删「FITS 产品」章引用 | `ACSD_DESIGN.md:316 ### 6.3 投影算法`，其第三条（`:320`）逐字含「导出只接受面亮度语义输入…其余语义拒绝；输出模式…显式声明，可视化模式标注不可测量」 | ✅ 在位（两条职责同在一条） |
| D-16：排障表「七项」改「五列 + 四项表外」 | `merged_TROUBLESHOOTING.md:12–15` 逐项保留七项；表 `:38` 仍 5 列、数据行 `:40–:59` **20 行**（我逐行数，R2 的「21 行」把分隔行算进去了，T07 的 O-2 正确） | ✅ 在位 |

**但「信息在位」≠「无矛盾」**：D-03 的正本（`:62`）与**顶点 `:231`** 直接冲突，见 N-R3-04。

---

## 3 前两轮订正的正确性回查

### 3.1 我自己重推过的量（不采信任何前两轮结论作为唯一依据）

**(a) 面亮度星等的 `Ω_ref` 符号（D-20 / R2-M02）—— 对。**

一级正本自带自检：`m(Ω) = ZP − 2.5·log10(F)`，`F = signal·Ω`。

```
直接展开：m(Ω_ref) = ZP − 2.5·log10(signal·Ω_ref)
                    = ZP − 2.5·log10(signal) − 2.5·log10(Ω_ref)   ⇒ 减号
```

用一级正本参数独立复算（`python3`，`ZP=0`、`signal=1e-4/sr`、`Ω_ref = 1 arcsec² = 2.3504430539097885e-11 sr`）：

```
−2.5·log10(Ω_ref) = 26.5721
直接定义式 m(Ω_ref)  = 36.5721
减号式               = 36.5721  ✓
加号式               = −16.5721 ✗   两式差 53.1443 mag
```

**(b) 球面残差律 `δ(θ)` 四档（D-21 / R2-M04）—— 对。**

由该卡自带的 `δ = (1 − pixfrac²)·θ²·[0.25/(1+r_c²) − 0.625·ξ_c²/(1+r_c²)²] + O(θ⁴)`，取表列 `pixfrac = 0.8`、`r_c = ξ_c = 0`：

```
括号项 = 0.25 ；(1 − 0.64)·0.25 = 0.09  ⇒ δ = 0.09·θ² （θ 以弧度计）
反解：δ=1e-10 ⇒ θ=6.9″ ；δ=1e-7 ⇒ θ=217.4″ ；δ=1e-6 ⇒ θ=687.5″
回代文档表四档（1 arcsec = 4.84813681e-6 rad）：
  θ=2″  文档 8.5e-12 / 我 8.4616e-12
  θ=6.3″ 文档 8.4e-11 / 我 8.3960e-11
  θ=60″ 文档 7.6e-9  / 我 7.6154e-9
  θ=300″ 文档 1.9e-7 / 我 1.9039e-7
⇒ 表对、旧摘要句错。T07 的三个边界 7″/217″/688″ 与我的 6.9/217.4/687.5 一致。
```

**根因句在 science 侧 `docs/science/algorithms/DRIZZLE_GEOMETRY.md` 的现状我核到了：仍是同一句错话**（T07 只登记未改）。这是跨车道项，detail 单改会与正本不一致。

**(c) `c_se = 1/(4φ(d)d)`（R2-S-04）—— 对。**

```
python3: 1/(4·φ(Φ⁻¹(0.75))·Φ⁻¹(0.75)) = 1.1663872874444212   （与文档逐位相同）
平方 = 1.3604593043119548 ，文档标 1.361，相对差 −3.973e-4（−0.04%），标注诚实
```

**(d) `k = D_p/N_p = pixfrac²`（R2-S-05）—— 对。** 由 `A_drop,j = pixfrac²·A_pixel,j`、`w_jp = a_jp/A_drop,j` ⇒ `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²` ⇒ `k = pixfrac²`。

**(e) `ZP_k` 的符号方向 + `F_ref,k = F0/k_photo,k`（R2-S-02）—— 对。**

```
m = ZP − 2.5·log10 F ⇒ F = 10^(−0.4(m − ZP))
F_syn = k_photo·F_adu 代入 ⇒ ZP_k = ZP_syn − 2.5·log10(k_photo)   （负号方向正确）
F_ref,k = 10^(−0.4(m_ref − ZP_k)) = 10^(−0.4(m_ref − ZP_syn))·k_photo^(−1) = F0/k_photo,k
⇒ a_k := F_ref,k/F0 = 1/k_photo,k
```

**(f) 两档权重式的代数（D-22）—— 代数对，语义错。** 见 N-R3-03。

### 3.2 逐条「对 / 错 / 不完整」

| 条目 | 判定 | 我的推导或一手来源 |
|---|---|---|
| R2-S-02 / D-22 的 `F_ref` 口径与 `ZP_k` 符号 | **对** | 见 (e)，我独立推 |
| R2-S-04 / `√1.361` | **对** | 见 (c) |
| R2-S-05 / `k = pixfrac²` | **对** | 见 (d) |
| R2-M02 / `point_information` + `a_k²` | **对**（detail 侧已闭环；跨对象命名未收） | `PHASE1_DETAILED_DESIGN.md:186/:210` 已改；但同一物理量在 detail 仍有 **6 个名字**：`W_k`(`PHASE2:83,:89`)、`W_info,k`(`PHASE2:141`)、`W_psf,k`(`PHASE1:173`)、`W`(`PHASE2:95`)、`POINT_INFORMATION/W`(`phase3.writer`)、`w_info_*`(`noise-snr.md:352-354`) |
| R2-M03 / 排障表行数 | **对**（T07 的 O-2 更正正确） | 我逐行数：数据行 `:40–:59` = **20 行**，表头 `:38` 5 列 |
| R2-M04 / `δ` 摘要阈值 | **对**（detail 侧） | 见 (b)；**science 侧未同批改** |
| R2-M11 / 「FITS 产品」章 | **对** | 见 §2.3 |
| R2-N12 / `m_ref` 冻结 | **对但不完整** | `UNIFIED_MODEL.md:76` 已改；`:69` 仍留「冻结为固定参考星等档」（N-R3-13） |
| R2-X-01 / X-05 / X-07 / X-09 | **对**（X-01 只对了一半） | 见 §2.1；X-01 的体例描述见 N-R3-06 |
| **R2-M05 / D-22「两档权重式」** | ❌ **错（方向性）** | 见 N-R3-03。**这是我本轮对止血的最主要反驳。** |
| R2-M01（180 处 `algorithms_phase*` 断链） | ❌ **三轮零进展** | 我复跑：仍是 **180 处 / 17 个目标路径全 MISSING**，分布 `eng/packaging/config/config_registry.json` 121、`defaults.json` 8、`lib/…/module_adapters.cpp` 7、`docs/science/PHOTOMETRY.md` 7、`PHOTOMETRY_RESEARCH_PACK.md` 6、`eng/contracts/schemas/phase_config_*.json` 12 等。`docs/detail/**` 内 0 处（本车道干净） |
| R2-M06（`mask` 三处同批） | ❌ **未落地** | `eng/contracts/data/phase_product_exchange.schema.json:162` `"enum": ["signal","support","variance","ivar","mask"]`；`lib/…/phase_product_exchange_validator.py:57` `_PLANE_ID_SET = {…, "mask"}` —— 我逐字核对，与 `UNIFIED_MODEL.md:58`「裸写 `mask` 一律判红」正面冲突 |
| R2-M07（语言轮残留） | ❌ **未落地** | `acsd.phase2.upm-fit.md:280` 仍逐字「（「`0 = auto`」的**旧述已按实测订正**）」；`acsd.phase3.wcs.md:68` 仍逐字「该处表述**待其车道订正**」 |
| R2-M08（`PUBLIC_API.md:1215`） | ❌ **未落地** | 仍指 `LOG_AND_ERROR.md`「验收」一节；该文件 9 节中无「验收」 |
| R2-M09（顶点 AUTO 三档） | ❌ **未落地** | `ACSD_DESIGN.md:296` 仍只写「生产算法集为 none、percentile、winsorized、linear fit」，未写 AUTO 只产三档 |
| R2-N09 / U-01（21 处假节名） | ❌ **零进展** | 我复跑：`grep -ro "数据对象（各自具名）" docs/ eng/ lib/` = **21**（`UNIFIED_OBJECTS.md` 5 + `ARTIFACTS.md` 16），与 `c0bec7f7` 时**完全相同**。实际节名是 `## 2. 数据对象与字段歧义消解` |
| R2-N10（`21_observability.md:109`） | ❌ **未落地，且被虚假自证覆盖** | 见 §2.2① |
| R2-§2.4「被推翻/降级条目的处置全部正确」 | ⚠ **一半对** | 对的一半：J-07（补定义不强改口径）、E-06（drizzle 双向）、E-09（改真实标题）—— 我复核 `reverse_drizzle.cpp/.h` 实存、`### 3.2 三个基本对象的语义` 实存，**处置正确**。<br>**但 T07-detail 的 O-4（`acsd.phase1.star-detection.md:13`「单源」不能留）在落地时又制造了新问题**：见 N-R3-06 |

### 3.3 前台点名的「四处自证数字」—— 我逐个重跑的真值

前台说「第一轮的审稿记录自身有四处自证数字复现不出（口径不一致）」。我把四条全部重跑，**四条里三条确为口径错、一条是数值错**。**收敛度量依赖它们，所以下面给的是可直接用的真值。**

| # | 记录里的断言 | 记录的数 | **我的真值（可复跑）** | 判定 |
|---|---|---|---|---|
| **①** | 第 1 轮订正记录 §6.2 表格行：标签写「`docs/detail/**`（根级+`anchors/`+`infrastructure/`）`§` 出现总数」，改前 **709** → 改后 **46** | 709→46 | **标签说的是子集，子集改前 = 299**；709 是**全树**数。真值：**子集 299 → 46**；**全树 709 → 433 → 432** | ❌ **口径错**。我把**全树改前数**与**子集改后数**配成了一行 |
| **②** | 「残留 **35 行**逐行确认全是外部文献/标准节号，不可改名」 | 35 行 | **子集 `§N` 出现次数 = 36、含 `§N` 的行数 = 32**（都不是 35）。逐条读后：30 条确为外部（B&A96 段为主），**6 条不是外部**，其中**至少 2 条是仓内跨文档引用**：`PHASE3_DETAILED_DESIGN.md:136 §6`、`STAR_DETECTION_IMPL_DESIGN.md:56 §3.3` | ❌ **数错 + 定性错**。那 2 条正是「仓内跨文档锚」，是 AGENTS §5 所指的对象，**不能以「外部节号」豁免** |
| **③** | 第 1 轮 §3 X-10「`GLOSSARY.md` **23** 条 term」 | 23 | **21 条**（`docs/GLOSSARY.md` 现 32 行，表格数据行 `:12–:32`）。改前也是 21 | ❌ **数错**，无出处 |
| **④** | 第 1 轮 §5 N-05「`n·Var(MAD)/σ² = 1/(16φ(d)²) = 0.618983`」 | 0.618983 | **0.618922489703423**（`d = Φ⁻¹(0.75) = 0.6744897501960817`，`φ(d) = 0.317776572684107`）。绝对差 **−6.051e-5**，相对差 **−9.776e-5**。要凑出 0.618983 需 `d = 0.6745622`，与真值差 7.2e-5 | ❌ **数值错**。处置方向（只写闭式）不受影响 |

**复跑命令（供前台自行验证）**：

```bash
# ①③（各 rev 的全树/子集 § 计数）
for r in 128a1b00 6934b1a1 c0bec7f7 18dd6c64; do
  git -c core.quotepath=false ls-tree -r --name-only $r docs/detail/ | grep '\.md$' | while read f; do
    git -c core.quotepath=false show $r:$f; done | grep -o '§' | wc -l
done
# ②（子集 §N 逐条分类）
grep -on '§[0-9]\+' docs/detail/*.md docs/detail/anchors/*.md docs/detail/infrastructure/*.md
# ④
python3 -c "import math;p=lambda x:math.exp(-x*x/2)/math.sqrt(2*math.pi);d=0.6744897501960817;print(1/(16*p(d)**2))"
```

**⇒ 给前台的度量建议**：「`§` 改前 709 → 改后 46」这类**跨口径配对**应从收敛度量里剔除，改用「全树 709 → 432」或「子集 299 → 46」单一口径；「残留 35 行全是外部」这条**不能作为 AGENTS §5 豁免的依据**，因为子集里至少 2 条是仓内锚。

---

## 4 本轮新发现清单

> 编号 `N-R3-xx`。每条我都自己打开目标文件读过原文。凡我只做了机器对账而没读内容的，明确标「未逐行」。
> **排序说明**：N-R3-01…17 是我第一遍读出来的（按严重度）；N-R3-18…20 是两个返回的子代理报、我逐字复核成立后补入的；N-R3-21 是自查防误报项。**编号不代表发现次序。**

### N-R3-01 🔴 顶点对 `F_ref` 的定义与 science 正本 + canonical schema 互斥（本车道文件，前三轮全漏）

- **位置**：`docs/ACSD_DESIGN.md:108`
- **逐字**：「帧级信噪比定义为 `SNR = F_ref/σ_F`：分子是参考通量 `F_ref`，**参考轮廓取星点目录的中位视宁度加本帧天光离散度**；`σ_F` 是该轮廓的 PSF 加权通量不确定度（`σ_F⁻² = Σ_i P_i²/σ_i²`），与分子同帧同源。」
- **对立口径（我逐字读过）**：
  - `docs/detail/UNIFIED_MODEL.md:68–74`：`F_ref,k = 10^(−0.4·(m_ref − ZP_k))`，`m_ref` 缺省 **6.0**；
  - `docs/science/noise_snr/NOISE_SNR.md:266`：`F0 = 10^(−0.4·(m_ref − ZP_syn))`，与帧无关的物理公共锚；
  - `eng/contracts/schemas/unified/frame_snr.schema.json#reference_baseline.reference_flux_common`：「**物理公共锚** F0 … fixed_magnitude 作用域下 = `10^(-0.4*(m_ref-ZP_syn))`」，且 `reference_mag` 的 description 逐字「注意 m_ref 是**参考电平**，不是「本帧能测到的星」—— 亮端外推（如 m_ref=6 对深窄带数据）仍是良定义」。
- **为什么是真问题**：顶点把 `F_ref` 定义成**视宁度/天光依赖的孔径轮廓通量**；science 与机器合同把它定义成**固定星等档的锚通量**（与视宁度、天光无关）。两者物理量级可以差几个数量级（`m_ref = 6.0` 是亮星外推）。**这是「同名不同义」的最危险形态：符号相同、物理量不同、且出现在权威顶点。**
- **建议改法**：顶点 `:108` 删「参考轮廓取星点目录的中位视宁度加本帧天光离散度」对分子的绑定，改为「分子是配置缺省参考星等档 `m_ref` 在本帧的仪器通量 `F_ref,k = 10^(−0.4(m_ref − ZP_k))`；`σ_F` 取该帧 PSF 拟合域的加权通量不确定度」。**顶点修改须负责人批准（`ACSD_DESIGN.md:23`）⇒ 登记。**
- **附带**：`ACSD_DESIGN.md:108` 的「参考轮廓取…中位视宁度」这句话在 science 侧**确实有对应物**，但它绑定的是 `σ_F`/`depth_m5` 的**参考轮廓**（`NOISE_SNR.md:314`「`sigma_F(ref)` 必须显式绑定**参考轮廓、孔径**、背景估计域与像素标度」），不是分子。⇒ 顶点的错在**把轮廓绑定挂到了分子上**。

### N-R3-02 🔴 `UNIFIED_MODEL.md` 的 `sky_samples` 点权重定义与 6 份文档 + 生产合同相反（本车道正本，前三轮全漏）

- **位置**：`docs/detail/UNIFIED_MODEL.md:48`
- **逐字**：「| `sky_samples` | …（坐标、值、variance、**点 SNR 权重**） | 点权重 = **`SNR²/F_ref²`**（= `1/σ_F²`，**已归一的逆方差**；…）——**禁读作裸 `SNR²`**（同 `registry/acsd.phase2.sample.md` 禁令）；仅用于天光面拟合 |」
- **对立口径（我逐条打开读过）**：
  | 位置 | 逐字 |
  |---|---|
  | `docs/ACSD_DESIGN.md:288`（顶点） | 「采样点权重取**噪声逆方差**」 |
  | `docs/detail/PHASE2_DETAILED_DESIGN.md:36` | 「采样点权重取**噪声逆方差 `control_ivar`**：被估量是变化的背景电平，`SNR²` 在该处**不是**有效逆方差代理；SNR 只作 veto/质量门」 |
  | `docs/detail/registry/acsd.phase2.sample.md:51` | 「`ivar` 弃用仅作诊断…**科学权重一律 `control_ivar`**」 |
  | 同文件 `:103`（mermaid） | 「每点赋 **control_ivar** 权重（= 1 / control_variance）」 |
  | 同文件 `:208` | 三臂对照 = `control_ivar` / `uniform` / **`SNR²`** —— 即 `SNR²` 是**对照臂**不是生产臂 |
  | `docs/detail/registry/acsd.phase2.upm-fit.md:33` | 「**production 控制点权重 = `quality_factor × control_ivar`**」 |
  | `docs/engineering/data/ARTIFACTS.md:112` | 「① 分子 `raw_w = quality_factor × control_ivar`…**禁静默回退 support/SNR**；`snr²/(1+snr²)` 只作 ablation/诊断」 |
  | `docs/engineering/governance/TRACEABILITY.md:266` | 「production UPM 权重 = quality × control_reliability × control_ivar；**禁止 star-SNR/support^p 乘因子**」 |
  | `docs/science/algorithms/PHASE2_COVERAGE.md:189` | 「`w_UPM = quality_factor × geometric_reliability × control_ivar`」 |
- **两处加重情节**：
  1. `UNIFIED_MODEL.md:48` **援引 `registry/acsd.phase2.sample.md` 作为禁令依据**，而该文件 `:51` 的禁令恰恰是「科学权重一律 `control_ivar`」——**援引的落点与自己的结论相反**；
  2. `upm-fit.md:86–87` 另写「与 P2 定权式『权重 ∝ 平方信噪比 / 参考通量平方 = 逆方差』同源」，把两个**不同物理量**（点源 SNR 逆方差 vs 背景 patch 估计量方差）说成同源。
- **建议改法**：`UNIFIED_MODEL.md:48` 改为「点权重 = `control_ivar`（= 1/`control_variance`），**不是** SNR² 族；SNR 只作 veto/质量门」，并把权威锚点改指 `docs/engineering/data/ARTIFACTS.md`「`upm.robust_control_weight`」行；删「同 `registry/acsd.phase2.sample.md` 禁令」的误援引。`upm-fit.md:86–87` 同批改。

### N-R3-03 🔴 权重归一 `F0²` vs `F_ref,k²`：science 层内部 4:10 分裂，canonical schema 明写 `F0²`，而 D-22 的「两档对账」把 science 禁止的写法合法化

- **一级正本逐字（我读过）**：
  - `docs/science/noise_snr/NOISE_SNR.md:336`：「**归一化因子必须取公共锚 `F0`，不能取该帧的 `F_ref,k`。**」
  - 同文件 `:339`：「用逐帧 `F_ref,k` 归一：`SNR²/F_ref,k² = W_k·k_photo,k²`，逐帧被该帧的测光标度**二次缩放**，**`sum_k` 不再等于 `1/Var(F_hat)`**。」
  - 同文件 `:359`：「该式**唯一会被误用的形态就是改掉归一化**…此时它连 `F0²/Var(F_hat)` 都不是。」
  - 同文件 `:325/:333/:353/:356/:361/:508/:535` 共 **10 处**用 `F0²`。
  - **但同层另 4 处用 `F_ref²`**：`docs/science/unified/DATA_SEMANTICS.md:171`、`docs/science/DISPUTE_RESOLUTION.md:164`（且写「由恒等式 `w = SNR²/F_ref² = 1/σ_F²` 与广义最小二乘最优性**唯一确定**」）、`:291`（写「权重换算式 `w = SNR²/F_ref² = 1/σ_F²` 是**代数恒等**」）、`:300`；另 `docs/DOCUMENT_INDEX.yaml:818` 写「`w = SNR²/F_ref² = 1/σ_F²` 是**全链唯一权重换算**」。
- **canonical schema 逐字（我 `json.load` 读过）**：`eng/contracts/schemas/unified/frame_snr.schema.json` 的 `reference_baseline.pairing_identity` description：「帧级 SNR 的分子参考通量必须与换算权重用的公共锚配对 —— **w = SNR_k²/F0² = a_k²/σ_k²**」。且 `scope` 的 description：「**两种作用域下 `reference_flux_common` 都必须是物理公共锚**」—— 即 `group` 与 `frame_independent_fixed_magnitude` **两个 scope 都不是「用 `F_ref,k` 作分母」**。
- **我的独立裁决（推导）**：统一测光系下帧 `k` 的最优权重是 `W_k = 1/σ_sys,k²`，`σ_sys,k = k_photo,k·σ_frame,k`。用 `SNR²/F_ref,k² = 1/σ_frame,k² = k_photo,k²·W_k` 作权重，等于把每帧的测光标度因子从权重里丢掉了（除非 `k_photo ≡ 1`），`Σ w_k F_k / Σ w_k` **不再是 GLS 估计**。⇒ **`NOISE_SNR.md` 的禁令是对的；`F_ref,k` 分母不是「另一档」，是失效形态。**
- **T07-D-22 的错在哪**：它的**代数**（两式相差 `a_k²`）我复算过，是对的；但它把 `scope` 枚举的两个值**误读成两个归一化分母档**，在 `UNIFIED_MODEL.md:89–100` 写下「两档权重式的差一个 `a_k²`（本页两处权重式不是同一式，必须分名读）…`scope` 声明决定走哪一档，**读侧不得跨档互相代入**」。这等于**在三级文档里给一级正本逐字禁止的写法发了一张许可证**，而且它没有写出该禁令的**实际后果**（`Σ w_k` 不再等于 `1/Var(F_hat)`、放弃 GLS 最优性）。
- **同一文件里被 D-22 漏改的两处**：`UNIFIED_MODEL.md:43`（`frame_snr` 行末「Phase2 归一后换算 w=SNR²/F_ref²=1/σ_F²」）与 `:101`（「**配对性**：权重换算 `w = SNR²/F_ref² = 1/σ_F²`」）—— 两处都**不带档限定**，与 `:83/:95` 的 `F0²` 在同一小节并存。
- **建议改法**：① 撤掉 `UNIFIED_MODEL.md:89–100` 的「两档」对账，改为「**唯一口径：`w = SNR²/F0² = 1/σ_sys²`；`F_ref,k` 作分母是失效形态（`Σ w_k` 不再等于 `1/Var(F_hat)`），一级正本 `NOISE_SNR.md` 明文禁止**」；② `:43`/`:101` 随改；③ `science/DISPUTE_RESOLUTION.md:164/:291/:300` 与 `DATA_SEMANTICS.md:171`、`DOCUMENT_INDEX.yaml:818` 与 `NOISE_SNR.md` 对齐（同批，跨 science 车道）；④ **须负责人裁**（顶点 `:111`/`:282` 与 20 余处文档/代码注释要同批改），按 AGENTS §9 登记 UNRESOLVED。

### N-R3-04 🔴 顶点把稀疏控制点层写成「默认开启、作为标准层插入」，detail 记「生产侧尚无产者、该路径不可兑现」

- **位置**：`docs/ACSD_DESIGN.md:231`（我逐字读过）
- **逐字**：「1. HiPS 文件：帧级信噪比写入文件头，PSF 有效面积一并落盘；**可选稀疏控制点层（默认开启）作为标准层插入**，存绝对信噪比控制点，不启用时只输出帧级。」
- **对立面**：`docs/detail/registry/acsd.phase1.noise-snr.md:62`：「**交付状态 = 合同面已冻结、生产侧尚无产者**：Phase1 既无稀疏层侧车写者，也无 HiPS 属性载体发布者；产者落地前 `sparse_reconstruct` 路径**不可兑现**，只余 `frame_reconstruct` 与稠密路径」。
- **为什么 T07 的「两层论证」在这里不成立**：T07 §2-A 把「配置默认值」与「生产侧产者状态」分成两层，说两句可同时为真。**但顶点 `:231` 写的是输出合同的产品形态**（「作为标准层插入」「不启用时只输出帧级」），这**正是生产侧那一层**，不是配置缺省层。⇒ 顶点与 detail 正本在同一层上互斥。
- **建议改法**：顶点 `:231` 加限定「（配置缺省请求产出；生产侧产者落地状态见 `docs/detail/registry/acsd.phase1.noise-snr.md` 的输出面表）」，或直接按 detail 的交付状态改写。**顶点须负责人批准 ⇒ 登记。**

### N-R3-05 🟠 `21_observability.md:109` 的「①②③ 任一违约」与本节 `:89/:91/:97` 直接矛盾（未落地，且被虚假自证覆盖）

见 §2.2①。依据：`git diff --numstat c0bec7f7 18dd6c64 -- docs/detail/infrastructure/21_observability.md` = `2 2`；`:109` 逐字未动。
**建议改法**：`:109` 的「①②③」改「②」（我与 T07 的判断一致：只有 `queue_low_window_enforcement = "hard_fail"` 一个键，我 `json.load` 读过 `eng/contracts/resource_gate_v1.json`）。

### N-R3-06 🟠 `00_INDEX.md:33` 的 registry 体例描述点错了对象、漏了两张卡；T07 的自证「点名这 5 张」不成立

见 §2.1 D-18。**建议改法**：改为逐字点名 5 张 —— `acsd.phase1.hips-writer`、`acsd.phase1.session`、`acsd.phase1.star-detection`、`acsd.phase2.resample`、`acsd.phase2.session`；**删掉 `acsd.phase3.session`**（它没有卡，也不在端口注册表内）。同时补一句说明为何这 5 张不进端口图。

### N-R3-07 🟠 `PHASE3_DETAILED_DESIGN.md:136` 的 `§6 L4` 是错锚 + 机械锚残留

- **逐字**：「…『无"黑洞"：无异常零值/死区/未填充孔洞』+ **§6 L4**（真实数据端到端视觉验收）+ 本设计「导出裁剪范围（crop）」一章」
- **事实**：`ACSD_DESIGN.md` 第 6 章是「export：投影导出」（`:301`），**L4 真实视觉验收在 §12.4「验证层级与四层验收」**（`:516`，表内 `| L4 真实视觉验收 |` 在 `:533`）。⇒ `§6` 既是 AGENTS §5 明禁的机械锚，**又指错了章**。
- **这条同时是我对「残留 35 行全是外部节号」的第二个反例**（§3.3②）。
- **建议改法**：删 `+ §6 L4`，并把该锚并入同句已有的「《ACSD 最高设计》的「验证层级与四层验收」一节（L4 真实视觉验收）」。

### N-R3-08 🟠 `depth_m5` 的参考轮廓口径：detail 明文「禁另起孔径」，science 明文「必须绑定孔径」

- `docs/detail/UNIFIED_MODEL.md:42`：「固定参考 **PSF** 下 5σ 深度（全链只有一个 `flux` 口径 = PSF 拟合域…**禁另起孔径口径**；孔径仅作**显式声明的诊断/交叉验证**）」
- `docs/science/noise_snr/NOISE_SNR.md:314`：「`sigma_F(ref)` **必须显式绑定参考轮廓、孔径**、背景估计域与像素标度」
- `docs/science/unified/SCIENCE_SCOPE.md:52`：「**固定参考 PSF 与孔径下**的 5σ 深度…参考轮廓、**孔径**、背景估计域与像素标度的绑定条件由《噪声、信噪比与不确定度》分册承载」
- `docs/science/unified/UNIFIED_SCIENCE_MODEL.md:140`：「`depth_m5` | 固定参考 **PSF 与孔径**下的 5σ 深度」
- ⇒ **三级 detail 逐字禁止一级 science 两处要求的口径。** T07 登记为 U-09，**至今未裁未改**。
- **建议改法**：须 science 车道先定「`depth_m5` 的参考是否含孔径」；若含，detail `:42` 的「禁另起孔径口径」须删；若不含，science 三处须改。**跨车道 ⇒ 登记。**

### N-R3-09 🟠 U-01「数据对象（各自具名）」假节名 21 处，三轮零进展

`grep -ro "数据对象（各自具名）" docs/ eng/ lib/` = **21**（`docs/engineering/UNIFIED_OBJECTS.md` 5 + `docs/engineering/data/ARTIFACTS.md` 16），与 `c0bec7f7` 时**逐数相同**。实际节名 = `docs/detail/UNIFIED_MODEL.md:15 ## 2. 数据对象与字段歧义消解`。
**建议改法**：21 处逐字替换为「数据对象与字段歧义消解」一节（跨 engineering 车道；我 `T07-重复断言收敛-engineering.md` 的 R1–R18 清单里**没有这一条**，是漏项不是有意保留）。

### N-R3-10 🟠 M01「`algorithms_phase*` 旧路径」180 处断链，三轮零进展

见 §3.2。17 个目标路径全部 MISSING。`docs/detail/**` 内 0 处（本车道干净），其余落在 `eng/packaging`（129）、`docs/science`（13+）、`eng/contracts`（15）、`lib/**`（19）。

### N-R3-11 🟠 R2 的 M06 / M07 / M08 / M09 四条全部未落地

见 §3.2 的四行。**M06 与 M07 都在本车道写面内**（`UNIFIED_MODEL.md` 侧已改、机器合同未改；两张 registry 卡的裁判语言未删），M08 在 engineering 车道，M09 在顶点。

### N-R3-12 🟡 GLOSSARY 只收录 5/13 个 canonical 对象，且恰漏歧义风险最高者 —— D-07 降级后没有补上登记

- 我逐个数：13 个对象在 `docs/GLOSSARY.md` 有词条的只有 **signal、variance、ivar、support、coverage 共 5 个**；**没有词条的是** `source_snr`、`depth_m5`、`frame_snr`、`point_information`、`sparse_snr_layer`、`validity`、`rejection`、`provenance` 共 **8 个**。
- **关键**：T07 自己在 §2-D/§2-F 记录了 `validity` / `mask` / `rejection` 的**同名不同义与四概念冲突**，而这**三个对象恰好一个词条都没有**。
- D-07 把自我声明降级成「叫法与单位口径的登记面」，**但没有交付该声明所承诺的东西** —— 对 8 个无词条的对象，「叫法」仍未登记。
- **建议改法**：只补 **`validity`、`rejection`、`point_information`** 三条（歧义风险最高、且已有明确的层级冲突可写），不补全 13 条（否则等于让 GLOSSARY 第二次自称对象正本，重犯它自己的病 —— 这正是我采信 T07 对子代理 V-2 否决的理由）。
- 另：`GLOSSARY.md:22` 的 `bad_mask` 词条只登记坏点掩膜，**未登记同文件同层的坏列掩膜**，而 `acsd.phase1.cosmetic.md:36` 与 `:42` 相邻两行给出**极性相反**的整型取值（`1 = 坏点` vs `1 = 已修 / 2 = 仅标记`）。（后者由子代理 C 报出，**我未逐行复核该卡**，标「子代理报的」。）

### N-R3-13 🟡 `UNIFIED_MODEL.md` 同一小节内 `:69` 与 `:76` 对 `m_ref` 的措辞自相矛盾（D-04 未清干净）

- `:68–69`：「三行共用的 `F_ref` 是同一口径，**冻结为固定参考星等档 `m_ref` 在本帧的仪器通量**」
- `:76`：「`m_ref` 是**配置缺省的**参考电平约定…合同只约束它是 number，**故它不是冻结常数**」
- ⇒ T07 清掉了 `:76` 的「冻结常数」，**没清同一小节 7 行外的「冻结为固定参考星等档」**。读者在同一屏内会读到两个相反的断言。
- **建议改法**：`:69` 的「冻结为固定参考星等档 `m_ref`」改为「取配置缺省的参考星等档 `m_ref`（缺省 6.0、可被输入 JSON 覆盖）」。

### N-R3-14 🟡 `PHASE3_DETAILED_DESIGN.md:118` 的「继承「WCS 计划」一章的 TAN 冻结域」指向的节里没有这条域

- `:118`：「| 适用域 | `sky` 边界点落在 TAN 半球之外 / abs(dec)>85° ⇒ 拒绝（**继承「WCS 计划」一章的 TAN 冻结域**）|」
- 我 grep 全文：`PHASE3_DETAILED_DESIGN.md` 的「WCS 计划」一节（`:22–31`）**不含 `85`、不含「半球」、不含任何适用域数值**。
- 真实落点：`docs/detail/registry/acsd.phase3.wcs.md:50`（「极点（|dec|≤85° 单一条件）/TAN 半球」）与 `:88`（`invalid 权威源=DATA-P3-WCS §28：parity 非法/|dec|>85°/…`）。
- **建议改法**：改指 `registry/acsd.phase3.wcs.md`（对应条款）。

### N-R3-15 🟡 `PHASE3_DETAILED_DESIGN.md:106` 对 `cpu_profile` 的键名陈述有误

- `:106` 逐字：「判别键名取 `crop_form` 而非 `mode`：`cpu_profile` 的 **`legacy_v1`/`kernel_v1` 已占用 `mode`**」
- 我 `json.load` 读过 `eng/contracts/schemas/cpu_profile.schema.json`：顶层 `properties` **确有 `mode`**；`$defs.legacy_v1.properties` **有 `mode`**；**`$defs.kernel_v1.properties` = `[kernel_id, kernel_version, precision, size_class, backend_id, workers, block_size, oracle_status, measurements]`，没有 `mode`**。
- ⇒ `kernel_v1` 不占用 `mode`。**建议改法**：删去 `kernel_v1`（或改为「`legacy_v1` 分支与顶层属性已占用 `mode`」）。

### N-R3-16 🟡 `merged_TROUBLESHOOTING.md:63–64` 的历史叙事

- 逐字：「命令一律以仓内实际脚本为准，**不要照抄旧目录结构下的可执行文件路径**——那些路径在当前仓库并不存在。」
- AGENTS §5：「正文无…『旧版/作废/**曾/原**』等历史叙事」。「旧目录结构」属同类。
- **建议改法**：改为正向陈述「命令一律以仓内实际脚本为准（下列路径在当前仓库存在）」。**低优先，属语言轮。**

### N-R3-17 🔵 诚实边界：本车道**零悬空仓内路径**（一条正面结论，防止前台误以为我没查）

我对 `docs/detail/**` 全部 47 份做了仓内路径引用抽取：**733 处引用，8 个「MISSING」全部是相对前缀组合**（如 `common.md:72` 的 `include/astro_scalar.h` 拼接 `lib/algorithms/shared/` 后存在、`noise-snr.md:351` 的 `include/acsd/…` 拼接 `lib/algorithms/noise_snr/` 后存在、`23_hips_browser.md:69` 与 `STAR_DETECTION_IMPL_DESIGN.md:15` 同理）。**拼接后 733/733 全部存在，0 真 MISSING。** 这一点与 R2-M01 报的「180 处断链」不冲突 —— 那 180 处全在 `eng/` `lib/` `docs/science/`，`docs/detail/**` 内确为 0。

### N-R3-18 🟠 `GLOSSARY.md:22` 的 legacy alias 与它自己援引的正本直接矛盾（子代理 C 报，我逐字复核成立）

- **词典逐字**（`docs/GLOSSARY.md:22` 的 legacy alias 列）：「`mask`（裸用） → **必须写 `bad_mask`**」
- **它援引的正本逐字**（`docs/detail/UNIFIED_MODEL.md:58`）：「三个掩膜面各管一件事、互不代用、都不是 canonical 对象：`star_mask`（天球坐标的星点/饱和/高结构掩膜，UPM 采样排除用）、`bad_mask`（校准域坏点掩膜，极性 1=坏…）、`validity`（canonical 状态对象）。**排异接受掩膜归 `rejection`，不进掩膜三面。裸写 `mask` 一律判红**」
- ⇒ 裸 `mask` 的正确落点是**四选一**（`star_mask` / `bad_mask` / `validity` / `rejection`），词典把它收敛成**单一 `bad_mask`**。这会把 `star_mask`（天球坐标域）与**极性相反**的排异接受掩膜静默折叠进坏点掩膜 —— 正是本仓最想防的极性错误（`docs/science/algorithms/COSMETIC_ALGORITHMS.md:132` 逐字警告 `bad_mask` 与 `accepted` 掩膜「极性**相反**」）。
- **加重情节**：D-07 刚刚把词典降级为「叫法与单位口径的**登记面**」，而这一格恰恰是一条例外的**迁移规则**，正是登记面最该做对的事，却做错了。
- **建议改法**：该格改为「裸 `mask` → 按 `UNIFIED_MODEL.md` 掩膜三面 + `rejection` **四选一**，语义确证前禁写任一面」；并给 `star_mask` 单独建词条（`GLOSSARY.md` 目前只有 `bad_mask`，无 `star_mask`）。

### N-R3-19 🟡 `GLOSSARY.md:12` 的 `EMVA 1288` 单位换算断言在仓内无承载，且**原文我核不到**

- 词典逐字：「DN → 禁用，一律写 ADU（**EMVA 1288** 等原文的 DN 即本仓 ADU，**换算比 1:1，数值不变**）」
- 我 grep：`EMVA` 在 `docs/` 下只命中 `GLOSSARY.md:12` 与 `docs/science/unified/DATA_SEMANTICS.md:296`（参考文献 [5]，EMVA 1288 R4.0 Linear）。**`docs/science/calibration/CALIBRATION.md`（该行的权威锚点）全文零命中 `DN`、零命中 `EMVA`**，其「标度的物理解释链」一节只定义 ADU 域、不提 DN、不提 EMVA。
- ⇒ 「换算比 1:1，数值不变」是一条**无仓内承载的单位等价断言**，而词典自称是「单位口径的登记面」。
- ⚠ **EMVA 1288 原文我没有取到**（标准 PDF 需下载核对），**「DN 即本仓 ADU、换算比 1:1」是否与原文一致我报「核不到」**，不凭印象裁。
- **建议改法**：删掉单位换比断言，或改为「`DN` 一律写 `ADU`（本链 ADU 域 = `BSCALE·样本 + BZERO` 的值域，见锚点节）」，把与外部标准的换算关系移出登记面。

### N-R3-20 🟡 `00_INDEX.md:49` 承诺的「各卡公共抬头面」有 6 张卡完全缺失；模块 id 与卡标题漂移

- `00_INDEX.md:49–51` 逐字承诺：「各卡的公共抬头面：上游条款（最高设计节号 + 一级正本 + **数据/API/算法正本**）…」
- 我逐卡统计前 12 行的 `科学正本|数据正本|API 正本|算法正本` 标签数，**6 张卡为 0**：`acsd.phase1.hips-writer.md`、`acsd.phase1.writer.md`、`acsd.phase2.resample.md`、`acsd.phase2.session.md`、`acsd.phase3.properties.md`、`acsd.phase3.verify.md`。
- **模块 id 漂移（本车道文件）**：`acsd.phase2.write.md:1` 的标题逐字是「`# 模块 acsd.p2.hips_writer（MOD-acsd-phase2-write）`」，而 `00_INDEX.md:46` 列的是 `acsd.phase2.write`；`acsd.phase1.hips-writer.md:1` 是「`# 模块 acsd.p1.hips_writer`」。⇒ **两个不同模块共享后缀 `hips_writer`**，且 `acsd.phase{N}.*`（registry 层）与 `acsd.p{N}.*`（descriptor 层）两套词汇的映射规则**全仓无处登记**。
- **建议改法**：① `00_INDEX.md:49` 的承诺放宽为「各卡至少给 `> 上游` + 一级正本」，或给 6 张零标签卡补齐；② 在 `00_INDEX.md` 加一张三列对照表（registry id / descriptor id / 模块中文名），并单独裁决 `acsd.phase2.write` 的 `hips_writer` 命名。

### N-R3-21 🔵 `mosaic_window.cpp:48` 的 `w = snr*snr` **不是**生产权重（我特此自查，避免误报）

`lib/infrastructure/scheduler/src/mosaic_window.cpp:38–48` 的注释逐字写「对覆盖该像素的各帧做 SNR² 加权（**模拟** SNR² 集成）…SNR 由 tile 尺寸与像素位置**确定性导出**」。这是调度器的**合成占位**，不是 Phase2 积分路径。**不报为缺陷**，仅提示：它以「SNR² 加权」命名，而 `NOISE_SNR.md:341` 与 `GLOSSARY.md:16` 都写「裸 `SNR²` 不是权重」，将来做归档时容易被误读。

---

## 5 本轮是否零发现

**明确写：本轮不是零发现。**

本车道 49 份文档中我自己逐行读完 14 份、逐段读完 + 动手 6 份，其余走「机器全量定位 → 逐条打开目标文件读内容」。跨四个一级目录做对照，亲自跑了 5 组一次性脚本（`§` 分 rev 计数、`§N` 分类、路径 `test -e`、GLOSSARY 锚解析、注册表差集、schema `json.load`、git diff 复核），派发 5 个只读子代理。

**产出**：止血 22 条抽查（有效 16 / 部分有效 2 / 无效或方向错 2 / 自证失真 2）、止血新造矛盾 **2** 处、前两轮订正回查 **18** 条（对 8 / 错或不完整 10）、**四处自证数字真值已给出**、**本轮新发现 21 条**（🔴 4 / 🟠 7 / 🟡 7 / 🔵 2）、**推翻止血 1 条（D-22）**、**证实止血自证失真 2 处**、**否决子代理 2 条**（见 §6 的 O-R3-7/O-R3-8）。

### 我检查过、**确认无问题**的项（供前台分辨「真零发现」与「没查到」）

| 检查项 | 方法 | 结果 |
|---|---|---|
| GLOSSARY 20 条 D-08 锚点是否兑现 | 自写解析器建全仓标题索引 + 前缀匹配 + 逐条开文件 | **20/20 命中，0 悬空** |
| `docs/GLOSSARY.md` 的 `§` 残留 | `grep -c` | **0** |
| `merged_TROUBLESHOOTING.md` 表行数/列数 | 逐行判表 | **20 数据行 / 5 列表头**（T07 的 20 正确，R2 的 21 错） |
| `registry/` 卡数 vs 端口注册表 | `json.load` + glob 差集 | **25 卡 / 20 `module_id` / 差集 5 / 注册表无卡 0** |
| `docs/detail/**` 仓内路径引用 | 733 处逐个 `test -e`（含前缀拼接） | **0 真 MISSING** |
| `00_INDEX.md` 的目录树与卡片计数 | `:9–24` 树逐个比对 + `find` 计数 | **3 子目录 + 11 顶层文件**；registry **25** 卡、infrastructure **7** 卡、`:41` 的 25 = 11+9+5 —— **全部兑现**（子代理先纠正了派单里「4 子目录 + 12 文件」的错误，我复核后采纳） |
| `DOCUMENT_INDEX.yaml` 语法 | `yaml.safe_load` | **OK**（`doc_index.schema_rev` 存在；条目对账由子代理做，**我未独立复核**） |
| 面亮度星等 `Ω_ref` 符号 | `python3` 独立复算 | **减号正确**，加号差 53.1443 mag |
| `δ(θ)` 四档读数 | `python3` 独立复算 + 回代 | **表对、旧摘要句错**，新边界 7″/217″/688″ 与我的 6.9/217.4/687.5 一致 |
| `c_se = 1/(4φ(d)d)` | `python3` | **1.1663872874444212 逐位相同** |
| `k = pixfrac²` / `ZP_k` 符号 / `F_ref,k = F0/k_photo,k` | 自己从头推 | **全部成立** |
| D-01 四处退出码落点 | 逐条打开目标文件 | **4/4 已改指 `ERROR_MODEL.md`**，同句域→码表已分名 |
| D-11 / D-12 / D-13 / D-14 / D-15 / D-16 / D-17 | 逐条打开目标文件核对节名 | **全部兑现** |
| D-20 面亮度星等 | 逐条打开 + 复算 | **已改对** |
| `acsd.phase3.sample.md` 的控制点权重 | 读 `:51`/`:103`/`:208` | **该卡自身正确**（问题在 `UNIFIED_MODEL.md:48`，见 N-R3-02） |

---

## 6 我推翻的既有结论

| # | 被推翻的结论 | 出处 | 我的反证 |
|---|---|---|---|
| **O-R3-1** | **T07-D-22：「两档权重式…不是同一式，必须分名读」「`scope` 声明决定走哪一档，读侧不得跨档互相代入」** | `T07-重复断言收敛-detail-glossary.md` §2-B、§3-8 | 我 `json.load` 读过 `eng/contracts/schemas/unified/frame_snr.schema.json`：`reference_baseline.pairing_identity` 的 description **逐字**是「w = SNR_k²/**F0²** = a_k²/σ_k²」；`scope` 的 description **逐字**是「**两种作用域下 `reference_flux_common` 都必须是物理公共锚**」。⇒ `scope` 的两个值区分的是「`F_ref,k` 是否逐帧落盘 / `F0` 从哪来」，**不是**归一化分母的两档。一级正本 `NOISE_SNR.md:336` 更逐字写「归一化因子**必须**取公共锚 `F0`，**不能**取该帧的 `F_ref,k`」，`:339` 给出用 `F_ref,k` 的后果（`Σ w_k` 不再等于 `1/Var(F_hat)`）。**D-22 把一个失效形态登记成合法档，方向错。** 我另行独立推导出 `SNR²/F_ref,k² = k_photo,k²·W_k`，故该分母丢的是逐帧测光标度因子 |
| **O-R3-2** | **T07 §3-5：「`21_observability.md:109` 已由『①②③』改为『②』」** | `T07-重复断言收敛-detail-glossary.md` §3 改动前后逐字表 | `git -c core.quotepath=false diff --numstat c0bec7f7 18dd6c64 -- docs/detail/infrastructure/21_observability.md` = **2 2**，全文只有 2 行改动（`:52`、`:97`）。`:109` 逐字仍是「判定域内 **①②③ 任一违约**且处于 enforce 面」。**这是一次被写进自证表但从未发生的改动** |
| **O-R3-3** | **T07 §4-f：「改后点名这 5 张并说明『一页一模块、只是不进端口图』」** | 同上 | `00_INDEX.md:33` 实际点名的是 `acsd.phase{1,2,3}.session` + `acsd.phase2.resample` = 4 个名字，其中 **`acsd.phase3.session` 没有卡**（也不在注册表），而真实差集里的 **`acsd.phase1.hips-writer` 与 `acsd.phase1.star-detection` 一个字未点**。差集真值（我 `python3` 算）：`[acsd.phase1.hips-writer, acsd.phase1.session, acsd.phase1.star-detection, acsd.phase2.resample, acsd.phase2.session]` |
| **O-R3-4** | **R2 的 O2（第 1 轮 §6.2「改前 709 → 改后 46」是全树/子集混配）** —— 我**维持**该判定，但把口径说准 | R2 §5 O2 | R2 说「子集改前实为 299」是对的，但把「真错」归给「子代理只测了全树」不完全 —— **记录自己的表格行标签就写着子集**，标签与数不匹配是记录本身的问题。我把四个 rev 的两个口径全跑了（见 §3.3），结论不变 |
| **O-R3-5** | **子代理 C：「假节名『数据对象（各自具名）』全仓 58 处」** | 子代理 C 报告 | 我用精确串复跑 `grep -ro "数据对象（各自具名）" docs/ eng/ lib/` = **21**（5 + 16）。58 这个数对应的是「含 `UNIFIED_MODEL.md` 的行」这一更宽口径（我数到 181 行含该文件名）。**结论方向不变（节名一处都不存在），但数必须用 21** |
| **O-R3-6** | **子代理 C：「`ARTIFACTS.md:62` 的 `DATA-OBJ-PROVENANCE-001` scalar 仍是 `int`」** —— 它自己后一轮又推翻了 | 子代理 C 补遗 | 我**独立复核**：`ARTIFACTS.md:64` 已是「非数值（字符串 / 键值对象 / 字符串数组，无标量数值面）」，与 `UNIFIED_OBJECTS.md:48` 一致；`:62` 的 `int` 是 **validity**。**子代理的两轮结论我采信后一轮（它自己纠错），并独立确认** |
| **O-R3-7** | **子代理 C：「`W_k` 在 `UNIFIED_SCIENCE_MODEL.md:58` 与 `NOISE_SNR.md:347` 定义不同，差 `a_k²·k_photo²`；同一符号在两本一级正本给出两个不同的量」** | 子代理 C F3-3 | **否决。这是「同名不同义」红线的典型误伤 —— 它把 `a_k` 当成独立因子，没有代入本仓已冻结的定义。** 我逐条读过：`UNIFIED_MODEL.md:92` 与 `NOISE_SNR.md:267` **都逐字写** `a_k := F_ref,k/F0 = 1/k_photo,k`。代入 `NOISE_SNR.md:347` 的 `W_k = P_kᵀ C_k⁻¹ P_k / k_photo,k²`，得 `= a_k² P_kᵀ C_k⁻¹ P_k` —— **与 `UNIFIED_SCIENCE_MODEL.md:58` 逐项相同，两式不是两个量**。子代理自己的措辞「差 `a_k²·k_photo²`」也暴露了它把两个**互为倒数**的因子当成了独立因子相乘。<br>**保留的部分**：子代理指出 `W_psf`/`W_info`/`W_k`/`point_information` **四名一物、全仓无词条**这一点我独立确认（`PHASE1:173`、`PHASE2:141`、`UNIFIED_SCIENCE_MODEL:58`、`UNIFIED_OBJECTS:42`），已并入 N-R3-12 与 §3.2 的 M02 行 |
| **O-R3-8** | **子代理 C：「假节名『数据对象（各自具名）』在 `ARTIFACTS.md` 引用 18 次 ⇒ 全仓 23 处」** | 子代理 C F4-2 | **数字更正，结论方向采信。** 我用精确串复跑：`UNIFIED_OBJECTS.md` = **5**、`ARTIFACTS.md` = **16**，合计 **21**，与 `c0bec7f7` 时逐数相同（⇒ 三轮零进展，见 N-R3-09）。子代理的 23/58 均偏高，**前台应使用 21** |
| **O-R3-9** | **子代理 C：「`docs/detail/` 下是 4 个子目录 + 12 个顶层文件」（派单原文的转述）** | 派单与子代理 C 的纠正 | **子代理的更正是对的，我采纳**：`docs/detail/` 实为 **3 个子目录**（`anchors`/`infrastructure`/`registry`）+ **11 个顶层 `.md`**，与 `00_INDEX.md:9–24` 的目录树逐个吻合。我按此复核了 `00_INDEX.md` 的 `:22`（25 卡）、`:23`（7 卡）、`:27`（四个 README）、`:41`（25 = 11+9+5）——**全部兑现** |

---

## 7 自证段

### 7.1 我自己做了什么

1. **逐行读完必读件**：`AGENTS.md`(127)、`docs/ACSD_DESIGN.md`(570)、标准 03(55)、标准 04(71)、`T06-r2-审稿-DOC-DETAIL-GLOSSARY.md`(176)、`T07-重复断言收敛-detail-glossary.md`(483)。
2. **逐行读完本车道 14 份**（清单见 §1），全部非抽样。
3. **自己动手重推的量**（`python3`，不采信任何前两轮结论作为唯一依据）：面亮度星等符号（差 53.1443 mag）、`δ(θ)` 六档 + 回代四档、`1/(4φ(d)d)`、`k = pixfrac²`、`ZP_k` 符号方向、`F_ref,k = F0/k_photo,k` 与 `a_k = 1/k_photo,k`、`SNR²/F_ref,k² = k_photo,k²·W_k` 的推导、`1/(16φ(d)²)`。
4. **自己跑的一次性对账**：
   - `git` 只读：四个 rev（`128a1b00`/`6934b1a1`/`c0bec7f7`/`18dd6c64`）的 `docs/detail/**` 全树与子集 `§` 计数；`git diff --numstat c0bec7f7 18dd6c64 --` 逐文件核对 T07 的 22 条声称改动。
   - 路径：`docs/detail/**` 733 处仓内路径引用逐个 `test -e`（含前缀拼接）。
   - 锚：`docs/GLOSSARY.md` 21 条词条、20 个节名锚逐条解析（建全仓标题索引 + 去章节号 + 前缀匹配 + 开括号截断）。
   - 引用：子集 `§N` 逐条分类（36 次出现 / 32 行，30 外部 / 6 非外部）。
   - 合同：`frame_snr.schema.json`（`reference_baseline` 全字段 description、`pairing_identity`、`required`）、`point_information.schema.json`、`cpu_profile.schema.json`（顶层 + `$defs` properties）、`resource_gate_v1.json`、`phase_product_exchange.schema.json`、`module_ports.registry.json` —— 全部 `json.load`。
   - 磁盘：registry 卡 glob 差集、`merged_TROUBLESHOOTING.md` 逐行判表、`DOCUMENT_INDEX.yaml` `yaml.safe_load`。
   - 代码阅读：`astro_sphere_sink.cpp:395–430`、`module_adapters.cpp:11900–11960`、`mosaic_window.cpp:25–104`、`snr_frame_science.cpp:173–197`（读，未改）。

### 7.2 派发了哪些子代理、否决/采信了哪些

用 `subagent` 派发 **5 个只读子代理**：

| 子代理 | 车道 | 交件 | 我的复核结论 |
|---|---|---|---|
| **A · 止血效果抽查** | T07 的 22 条逐条 + 删副本信息落点 + 全仓残留普查 | **交件时未返回** | **未采信任何内容**（无返回）。本车道 §2 的止血判定 **100% 是我自己做的** |
| **B · 跨切面数据对象口径** | 13 对象 × 4 层矩阵 + P1–P5 × 4 层 + detail 反查一级正本 | **已返回**（含补遗 F40–F46） | **部分采信**：F6（`F0` vs `F_ref,k`）、F18（顶点 `:231` vs detail `:62`）、F31（`mask` 在 schema/validator）、F11（假节名）**我独立复核全部成立**，已分别写进 N-R3-03/04/11/09 并给了一手原文。<br>**否决其数字**：假节名「58 处」→ 我复跑是 **21**（见 O-R3-5）。<br>**未复核即不采信**：F4（`depth_m5` 孔径，我另以自己的读数写为 N-R3-08）、F43（掩膜极性相邻两行相反，标「子代理报的」）、F41/F42/F45/F46。<br>**它越界建了新文件**（`run/CROSSCUT-13OBJ-AUDIT/`），已在卷首标明 |
| **C · 术语表与唯一权威自查** | GLOSSARY 21 条锚 + 反向覆盖 + 同名不同义 + 「唯一」类自我声明 | **已返回** | **部分采信**：F1-4（`GLOSSARY.md:22` 的 legacy alias 与 `UNIFIED_MODEL.md:58` 矛盾）、F1-1（`EMVA 1288` 无仓内承载）、F4-5（卡片抬头缺正本标签、模块 id 漂移）、F3-5（`stacking` 是造词）——**这四条我逐字打开双方文件复核，全部成立**，已分别写进 N-R3-18/19/20 与 §5 的清单。<br>**否决 3 条**：O-R3-7（`W_k` 两定义不同 —— 它没代入 `a_k = 1/k_photo`）、O-R3-8（节名断链 23/58 ⇒ 实际 21）、以及它列的「`signal` 单位只登记 1/3 承载面」我**未复核**，不采信。<br>**它关于 13 对象只有 5 个有词条的统计我独立复核过**（`signal`/`variance`/`ivar`/`support`/`coverage` = 5），**与我自己的 N-R3-12 数字一致**，这是本车道唯一一处我与子代理独立得数相同的关键数 |
| **D · 公式逐条重推**（含三条方向性错误） | detail 全部公式 + 方差方向 / 平场斜率截距 / 偏置归属 | **交件时未返回** | **未采信任何内容**。§3.1 的六组推导 **全部是我自己算的**。⚠ **三条方向性错误（方差方向、平场斜率/截距、偏置归属）本轮由子代理承担但未交件 ⇒ 这三块本轮不算已覆盖，须下一轮补** |
| **E · 引用路径与结构语言可复现轮** | 跨文档节名引用 / `DOCUMENT_INDEX` 对账 / 结构轮 / 语言轮 / 可复现轮 / 参考文献 | **交件时未返回** | **未采信任何内容**。§4 的路径、GLOSSARY 锚、§3.3①③ 的对账是我自己跑的；语言轮我只报了 1 条（N-R3-16）+ 2 条子代理未复核项，**语言轮本轮覆盖不完整** |

**⇒ 5 个子代理中 3 个交件时未返回（A 止血抽查、D 公式重推、E 引用/结构/语言轮）。本车道 §2–§4 的实质结论几乎全部来自我自己；B（跨切面）与 C（术语表）返回了，我只采信其中我亲手复核过的部分，并否决了 3 条。**

### 7.3 我**没有**做的事（诚实边界）

- **没有修改任何文档**，**没有任何 git 写操作**（git 只跑 `log` / `diff` / `grep` / `ls-tree` / `show`）。
- **没有编译、没有跑仓内测试、没有跑端到端**。所有「代码为准」的判定来自源码阅读 + `json.load` / `grep` / `python3`。
- **没有取任何一手文献全文**：`Rousseeuw & Croux 1993` Table 2 的 `1.361`、`B&A96 §3–§6`、`IVOA HiPS 1.0 §4.x`、`WD-HiPS-2.0 §4.3.2` 的原文 —— **一律报「核不到」**，不凭印象裁。因此 `merged_TROUBLESHOOTING.md`/`PRODUCT_STORAGE_FORM.md`/`STAR_DETECTION_IMPL_DESIGN.md` 里的外部节号引用我只判「形态与落点」，**不判真伪**。
- **§1 列出的 35 份文档未逐行读完**。这是本轮最大的覆盖缺口。
- **三条方向性错误（方差方向、平场斜率/截距、偏置归属）本轮未完成**（子代理未交件 + 我自己把预算投在止血与权重口径上）。我**只核到了一条相关事实**：`ACSD_DESIGN.md:106` 的 `I = O + N_e/g + N_read` 与 `NOISE_SNR.md:47` **逐字一致**，增益约定 `e⁻/ADU` 在 `NOISE_SNR.md:52` 明写并给了倒数约定的换算规则，**这一条我判定为对**；`NOISE_SNR.md:99–105` 关于「扣除偏置的原因是**截距**有偏、不是斜率有偏」的论证我**读到了但未独立复核其数值**。平场斜率/截距的**场向模型**在本车道 detail 层**找不到承载**（`ACSD_DESIGN.md:99` 只说「低阶乘性空间增益场…缺省为恒等（`m ≡ 1`）」），**本车道无处可核**；我在代码侧查到 `kDefaultSpatialGainOrder = 2`（`module_adapters.cpp:5679`），与顶点「缺省恒等」相反，但这已是 `UNRESOLVED.md` 裁-6/核-1 的登记项，**我不重复立项**。
- **没有裁决** `F0` vs `F_ref,k`、`depth_m5` 孔径、`validity` 三套枚举、`spatial_gain_order` 默认值 —— 全部按 AGENTS §9 登记，理由是它们要动一级正本或顶点，而顶点修改须负责人批准。
- **本单只出问题清单，不订正。**

### 7.4 收敛判定

**本车道未达收敛，且离收敛比前两轮更远。** 依据规范 03 的判据「连续两轮无新增实质问题」：

- 本轮新增 **21 条**实质问题（🔴 4）；
- 止血那一轮**自己造了 2 处新矛盾**，并有 **2 处自证失真**（一次未发生的改动、一次点名不全）；
- 第 2 轮的 **18 条回查里有 10 条未落地或改错**，其中 **4 条三轮零进展**（M01 的 180 处断链、U-01 的 21 处假节名、M06、M07）。

**⇒ 「连续两轮无新增实质问题」这个判据目前离得很远，前台不宜据此宣布收敛。**

### 7.5 给前台的处置次序建议（按依赖，不含跨车道裁决）

1. **先裁三项，裁之前不要动文字**：
   - **N-R3-03**（`F0` vs `F_ref,k`）—— 这条一动，`UNIFIED_MODEL`、`PHASE2_DETAILED_DESIGN`、`acsd.phase2.integrate`、`ACSD_DESIGN`、engineering 合同、代码注释、schema 注释共 **30+ 处**要同批，否则又是「改一处、坏三处」；
   - **N-R3-01**（顶点 `F_ref` 的参考轮廓定义）—— 与上条同源，须一起裁；
   - **N-R3-04**（顶点 `:231` 稀疏层「默认开启」）+ **N-R3-08**（`depth_m5` 孔径）。
2. **本车道内、可立即单批落地（不依赖裁决）**：N-R3-05（`:109`）、N-R3-06（`00_INDEX:33`）、N-R3-07（`§6 L4`）、N-R3-13（`:69` 措辞）、N-R3-14（`:118` 落点）、N-R3-15（`:106` 键名）、N-R3-16（`:63` 语言）、N-R3-11 的 M07 两条、**N-R3-18（`GLOSSARY.md:22` 的 mask alias）**、**N-R3-19（`GLOSSARY.md:12` 的 EMVA 断言）**、**N-R3-20（`00_INDEX.md:49` 承诺与 6 张零标签卡、模块 id 对照表）**。
3. **跨车道、已定位、纯机械**：N-R3-09（21 处假节名）、N-R3-10（180 处旧路径，需先定 `14_projection.md` 的 1:1 映射）、N-R3-11 的 M06/M08/M09。
4. **建议新登记进 `docs/engineering/governance/UNRESOLVED.md`**：N-R3-03（`F0`/`F_ref,k` 权重归一，**目前 27 条裁项未登记此项**）、N-R3-01（`F_ref` 参考轮廓双定义）、N-R3-04（稀疏层交付状态与顶点不符）、N-R3-08（`depth_m5` 是否含孔径）。

**⚠ 最重要的一条方法论结论**：`T07` 那一轮证明了「把重复断言收敛为唯一正本」这个动作本身是有效的（D-01/02/06/07/08/09/11/12/13/14/15/16/17/19/20/21 共 16 条确实兑现），**但它的自证表有 2 处不能采信**（§2.2）。**建议前台对 `T07-重复断言收敛-science.md` 与 `-engineering.md` 也做同样的「声称改动 vs `git diff`」逐条复核**，不要采信它们的自证表 —— 本车道抽样 22 条就抓到 2 处失真、1 处方向错。
