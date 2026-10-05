# T06 第 2 轮独立审稿 · 跨切面车道 DOC-DETAIL-GLOSSARY

**车道**：`docs/detail/**`、`docs/GLOSSARY.md`、`docs/README.md`、`docs` 顶层（`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md`），以及**跨全部正本的一致性**。
**基线**：`6934b1a1 第一轮审稿意见订正：五条车道逐条落实`。工作树在本车道范围内无未提交改动。
**只读**：全程零 git 写操作（git 仅只读，中文路径一律 `git -c core.quotepath=false`），零文件修改。
**L.key**：沿用第 1 轮取名 `DOC-DETAIL-GLOSSARY`（派单未给字面值）。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 本车道文档总数 | **48**（`docs/detail/**` 47 份 `.md` + `docs/GLOSSARY.md` + `docs/README.md`；另读 `docs/DOCUMENT_INDEX.yaml` 全文与 `docs/ACSD_DESIGN.md` 568 行全文） |
| **逐行读完** | **48 / 48**（全部读完，非抽样） |
| 逐行读完 | `UNIFIED_MODEL.md` 121、`GLOSSARY.md` 31、`README.md` 12、`00_INDEX.md` 79、`common.md` 72、`PHASE1_DETAILED_DESIGN.md` 227、`PHASE2_DETAILED_DESIGN.md` 145、`PHASE3_DETAILED_DESIGN.md` 173、`PRODUCT_STORAGE_FORM.md` 263、`LOG_AND_ERROR_SYSTEM.md` 214、`STAR_DETECTION_IMPL_DESIGN.md` 511、`merged_TROUBLESHOOTING.md` 90、`anchors/ANCHOR_CONTRACT.md` 152、`infrastructure/` 17/18/19/20/21/22/23 七卡 877 行、`registry/` 25 张卡 3982 行 |
| **未读完** | 无。**唯一一处未逐行**：`STAR_DETECTION_IMPL_DESIGN.md` 的 §6–§11（O5–O16 逐算子规格与文献台账）我按「与本车道跨片口径无关」只抽样读了 §5.1–§5.2 并核了 `1.482602218505602`；其余章节逐行读完。已在 §6 诚实边界中登记。 |
| 跨片对照面 | `docs/ACSD_DESIGN.md`（568 行全读）、`docs/science/unified/DATA_SEMANTICS.md`、`docs/science/algorithms/DRIZZLE_GEOMETRY.md`、`docs/science/noise_snr/NOISE_SNR.md`、`docs/engineering/UNIFIED_OBJECTS.md`、`docs/engineering/data/ARTIFACTS.md`、`docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md`、`docs/engineering/contracts/LOG_AND_ERROR.md`、`docs/engineering/contracts/HIPS_STORAGE_FORM.md`、`docs/engineering/standards/ERROR_MODEL.md`、`docs/engineering/standards/RESOURCE_MODEL`相关合同 `eng/contracts/resource_gate_v1.json` —— **对照面按需读段，非全读** |
| 代码正本 | `lib/infrastructure/pipeline/module_ports.registry.json`、`lib/infrastructure/cli/exit_codes.h`、`lib/infrastructure/cli/resource_gate.h`、`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp`、`lib/algorithms/star_detection/src/sdet_api.cpp`、`lib/infrastructure/aio/include/aio_hips.h`、`lib/algorithms/calibration/src/cosmetic_corrector.cpp`、`eng/contracts/schemas/unified/*.json`、`eng/contracts/schemas/hips_storage_form.schema.json`、`eng/contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json` |

---

## 2. 第 1 轮问题回查表

判据：**①** 是否真落地 **②** 方向对不对 **③** 订正引入的新问题。

### 2.1 已改对、方向正确、未引入新错（我亲自复跑）

| 编号 | 位置 | 我的独立核验 | 判定 |
|---|---|---|---|
| **S-01** | `UNIFIED_MODEL.md` 悬空 `[18]` | `grep -o "\[[0-9]\+\]"` → 正文 `[1]`/`[2]` 各 1 次 + 文末文献各 1 次 = 各 2 次，**闭合**；文末参考文献表 2 条真实存在（Horne 1986 DOI 10.1086/131801；PixInsight ImageWeighting 页） | 对 |
| **S-02** | `UNIFIED_MODEL.md` §2.1 参考通量基准 | **我自己重推**：`ACSD_DESIGN.md:100` 的 `m = ZP − 2.5 log10 F` ⇒ `F = 10^(−0.4(m−ZP))`，与 `:70` 的 `F_ref,k = 10^(−0.4(m_ref − ZP_k))` **同式同系数**。`ZP_k = ZP_syn − 2.5 log10(k_photo)` 的**负号方向**我独立验证：由 `F_syn = k_photo·F_adu` 代入 `m = ZP_syn − 2.5 log10 F_syn` 得 `ZP_k = ZP_syn − 2.5 log10 k_photo`，**符号正确**。进一步推出 `F_ref,k = F0/k_photo`（`F0 = 10^(−0.4(m_ref−ZP_syn))`），与 `w = SNR²/F0² = a_f²/σ_f²` 的 `a_f = 1/k_photo` 自洽。两档写侧约定与 `astro_sphere_sink.cpp:405-419` 的 FREF-BASELINE-001 注释**逐项对齐**；`m_ref` 默认 6.0 在代码 `module_adapters.cpp:7151` / 合同 / detail 三方一致 | 对（但见 §3-N21） |
| **S-04** | `acsd.phase1.photometry.md:86-89` 的 `√1.361` | **我自己 python 重算**：`1/(4φ(d)·d) = 1.1663872874444212`，与文档**逐位相同**；平方 `1.3604593043119548`，与 `1.361` 相对差 `−3.973e-4 = −0.04%`，文档标注**诚实** | 对 |
| **S-05** | `k = D_p/N_p = pixfrac²` | **我自己重推**：由 `A_drop,j = pixfrac²·A_pixel,j` 与 `w_jp = a_jp/A_drop,j` 得 `N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²` ⇒ `k = pixfrac²`，**成立**。detail 还补了球面残差律与适用域，比裸等式更严谨。掩膜残余系数 `k` 的消歧已在 `noise-snr.md:242` 落字 | 对（但见 §3-N11） |
| **E-05 / E-06** | 精度表述 | `grep -rn "float32\|float64\|f32\|f64" docs/detail/` → **0 命中**；三处承载处（`noise-snr.md:95-99` / `wcs-platesolve.md:59` / `photometry.md:58-61`）写法一致，与 `ACSD_DESIGN.md:160-164` §3.3 三档逐条对齐；冲突在 `noise-snr.md:508-513` 如实登记 | 对 |
| **E-07** | 删 `V15–V17` | `grep -rE "V[0-9]+[–—-]V[0-9]+" docs/` → **0**；`reject.md:207` 内容保留、只删版本代次 | 对 |
| **X-06** | `00_INDEX.md` 卡片计数 | 我自己数：`ls docs/detail/registry/acsd.*.md \| wc -l` = **25**；`:22`/`:41` 均已改为 25；`infrastructure/` 7 张卡正确 | 对 |
| **X-11** | `PRODUCT_STORAGE_FORM.md` 9 处缺口 | 逐条核到：schema `by_subproduct` = `{signal:fits, support:fits, variance:fits, ivar:fits, snr:tsv}` 与文档**逐字相符**；`DATA_SEMANTICS.md:60 ### 3.2 三个基本对象的语义` 真实存在；`10^7` 与合同 `HIPS_STORAGE_FORM.md:127` 一致；`grep -c IEEE docs/ACSD_DESIGN.md` → **0**，证实改前归因失实 | 对 |
| **§1.7-1** | `21_observability.md` 判据表 ①③ | 我自己跑合同：`compute` 段 ① 无 enforcement 键、② `queue_low_window_enforcement = hard_fail`、④⑤⑥ `record_and_justify`；代码 `resource_gate.h:268-270` `gate_enforcement()` **恒返回 `RecordOnly`**。改动与合同、代码三方一致 | 对（但见 §3-N10） |
| **§1.7-2** | `PHASE1_DETAILED_DESIGN.md:144,145` 破表 | 我自己逐行数竖线（不计 `\|`）：改前 144/145 各 4 竖线 vs 表头 5 竖线 = **真破表**；改后全文件 0 破表 | 对 |
| **§1.7-3** | `23_hips_browser.md` 花括号路径 | 我自己 `find` + 逐个 `test -e`：**24/24 全部实存** | 对 |
| **§1.7-4** | `GLOSSARY.md` `projection` 行恢复双向 | `ls lib/algorithms/drizzle/healpix_drizzle/ \| grep reverse` → `reverse_drizzle.cpp/.h` **实存**；`DRIZZLE_GEOMETRY.md:188 ## 4 反向 drizzle` 实存 | 对 |

### 2.2 **未落地 / 落地不完整**（本车道写面，本车道有责）

| 编号 | 位置 | 磁盘事实 | 判定 |
|---|---|---|---|
| **X-07** | `docs/GLOSSARY.md:5` | 仍逐字：「本词典是**唯一术语权威**」。`git diff 128a1b00 6934b1a1 -- docs/GLOSSARY.md` 对该行只有半角逗号→全角。第 1 轮记录 §3 X-07 明写处置是「保留『术语**名称与单位口径**的唯一权威』这一可兑现的窄声明」——**该窄声明一字未落**。与 `ACSD_DESIGN.md:562`「定义见 detail 与 GLOSSARY」、`:149`「全仓一份正本定义」三方冲突原样存续，而实际定义出现在三处（`detail/UNIFIED_MODEL.md` §2 / `engineering/UNIFIED_OBJECTS.md` / `science/unified/DATA_SEMANTICS.md`） | **未落地** |
| **X-09** | `LOG_AND_ERROR_SYSTEM.md:133`、`19_runtime.md:13,116`、`21_observability.md:52` | 四处仍逐字指 `docs/engineering/contracts/LOG_AND_ERROR.md`「**进程退出码**」一节。该文件实有 9 节：`1 合同范围 / 2 日志行格式 / 3 人可读摘要 / 4 run_log 与 manifest 登记字段 / 5 错误对象与退出码映射 / 6 显式降级登记要件 / 7 落点合同 / 8 脱敏与大小上限 / 9 判据与负例` —— **无「进程退出码」**。**真实落点是 `docs/engineering/standards/ERROR_MODEL.md:88 ## 7 进程退出码`**（我自己核过该文件全部标题）。第 1 轮给出的改法（「指向同文件『错误对象与退出码映射』」）**也指错了文件** | **未落地，且第 1 轮的改法本身有误** |
| **X-01 缺陷 1** | `docs/detail/00_INDEX.md:33` | 体例列仍写「`registry/` = 每个**生产 DAG 节点模块**一页」，而 §3 并列的 25 张卡里有 5 张 `module_id` 不在生产端口注册表内。我自己跑：`注册表 20 / 卡 25 / 有卡不在注册表 = [acsd.phase1.hips-writer, acsd.phase1.session, acsd.phase1.star-detection, acsd.phase2.resample, acsd.phase2.session] / 注册表无卡 = []`。第 1 轮只改了计数，未改体例 | **未落地** |
| **X-01 缺陷 2** | `acsd.phase1.star-detection.md:13` | 仍逐字：「模块词汇 `acsd.phase1.star-detection` 为 registry descriptor 单源」。生产端口注册表**无此 `module_id`**。承担检测+PSF 的生产模块是 `acsd.phase1.star-psf` | **未落地**（第 1 轮记录自己判「这句话是假的」） |
| **X-05** | `docs/detail/UNIFIED_MODEL.md` | 第 1 轮处置明写「在 `UNIFIED_MODEL.md` 如实写明 `mask` 不在 13 对象内，`star_mask`/`bad_mask`/`validity` 三个既有面各管什么」。实测：`grep -n mask docs/detail/UNIFIED_MODEL.md` 只有 `:47` 的 `star_mask` 行与 `:62` 的「weight/value/mask/snr 各字段各自具名」；`grep -rn bad_mask docs/detail/` → **0**。**无任何「mask 不在 13 对象内」的明示，三面分工未写** | **未落地** |

### 2.3 **订正引入的新问题**（本轮最高价值产出）

| 编号 | 位置 | 改前 → 改后 | 判定 |
|---|---|---|---|
| **N07** | `docs/detail/registry/acsd.phase1.noise-snr.md:62` vs `:138`/`:387`；`acsd.phase1.drizzle.md:172`；`docs/detail/UNIFIED_MODEL.md:45` | 第 1 轮把 S-03「默认产出」改成了「生产侧尚无产者」——**只改了 8 个副本中的 1 个**。全仓实测该断言的副本：`noise-snr.md:62`（已改）、`noise-snr.md:138`「`sparse_reconstruct`（**默认**）…存储/精度折中（**现行默认**）」、`noise-snr.md:387` 配置默认 `snr_path = sparse_reconstruct`、`drizzle.md:172`「…**默认产出**」、`UNIFIED_MODEL.md:45`「Phase1 标准层，**默认稀疏路径要求默认产出**」、`PHASE2_DETAILED_DESIGN.md:21,114`、`phase2.integrate.md:115,116`、`ACSD_DESIGN.md:279`。**结果：第 1 轮在它自己改的那张卡里造出了新的同文件自相矛盾**（`:62` 说「产者落地前 `sparse_reconstruct` 路径不可兑现，只余 `frame_reconstruct` 与稠密路径」，`:138`/`:387` 把它列为现行默认且可用），且 `drizzle.md:172` 断言的正是 `noise-snr.md:62` 判定不存在的那一层由「来自 noise-snr」插入 HiPS | **新问题（重）** |
| **N08** | 7 处 | 第 1 轮的「§N → 章节名」批量改写，把**原本正确的 `§N` 锚换成了不存在的节名**。我逐条用真解析器（比对目标文件标题集）核：改前正确、改后悬空的有 6 条；第 7 条改前即悬空且未修：<br>① `PHASE1_DETAILED_DESIGN.md:112` `STAR_DETECTION.md`「O3 检测阈值」一节 —— 实际标题是 `### 3.1 检测阈值及其噪声单位`（改前 `§3.1` **正确**）<br>② `PHASE2_DETAILED_DESIGN.md:11` `DATA_SEMANTICS.md`「量纲与逐像素语义」一节 —— 无此标题（改前 `§3.4` **正确**）<br>③ `common.md:32` `DATA_SEMANTICS.md`「HEALPix 索引与 tile 布局」一节 —— 该文件**无任何含 HEALPix/tile/索引 的标题**（改前 `§3.1` **正确**；同文件 `§3.7`→`「溯源最小集」一节` **改对了**，`### 4.4 溯源最小集` 实存）<br>④ `19_runtime.md:69` 「资源门」一章 —— 实际 `## 8. 重计算负载资源门（G-RES-01）`（改前 `§8` **正确**；同文件 `:15` 已改对）<br>⑤ `19_runtime.md:95` 「资源门判定与 exit 10」一段 —— 无此节（改前 `§8.4` **正确**）<br>⑥ `merged_TROUBLESHOOTING.md:29` 「症状索引」一节 —— 实际 `## 4 症状主表`（改前 `§4` **正确**；同句 `§3`→`「通用定位顺序」一节` **改对了**）<br>⑦ `PHASE3_DETAILED_DESIGN.md:15` 《ACSD 最高设计》的「投影算法」与「**FITS 产品**」两章 —— ACSD_DESIGN 第 6 章只有 `6.1 使命 / 6.2 流程 / 6.3 投影算法`，**无「FITS 产品」**（改前 `§6.3 / §6.4`，其中 `§6.4` 本就悬空 ⇒ **第 1 轮把一个悬空锚换成了另一个悬空锚，未修**）。我核对：`§6.3 投影算法` 的正文同时承载了「输入语义守卫」与「硬约束」两条，正确改法应是两处都点「投影算法」一章 | **新问题（重）** |
| **N09** | `docs/engineering/UNIFIED_OBJECTS.md` 5 处 + `docs/engineering/data/ARTIFACTS.md` 16 处 = **21 处** | 第 1 轮把 `UNIFIED_MODEL.md:15` 的节标题从「数据对象（各自具名）」改成「数据对象与字段歧义消解」，并在 §9 R-2 / §5 N-12 登记「这 21 处必须同批提交」。**同批提交发生了（`6934b1a1`），但节名一处未改**：`grep -ro "数据对象（各自具名）" docs/` → **21**，`grep -ro "数据对象与字段歧义消解" docs/engineering/` → **0**。路径侧已被 engineering 车道修好（旧路径 `docs/detail/common/UNIFIED_MODEL` 命中 0），所以现在这 21 处是**纯节名断链** | **新问题（重）**——第 1 轮自己登记的交付风险未闭环 |
| **N10** | `docs/detail/infrastructure/21_observability.md:109` | 第 1 轮改了 `:89`/`:91` 的判据表（①③ → `record_and_justify`，并加「任何资源判据都不改变程序退出码」），**没改紧随其下的 bullet**：`:109` 仍写「判定域内 **①②③ 任一违约且处于 enforce 面**——该路径只存在于资源门判定面」。新表已声明 ①③ **没有 enforce 键**，该 bullet 仍把三者并列 ⇒ **改表不改 bullet，矛盾从「不存在」变成「存在」** | **新问题（中）** |
| **N11** | `docs/detail/registry/acsd.phase1.drizzle.md:114` | 第 1 轮新写入的同一行里：「`N_p = Σ_j a_jp/pixfrac² = D_p/pixfrac²`，即 `k = pixfrac²`（**与 `pixfrac` 取值无关**，`pixfrac = 1` 时 `k ≡ 1`）」。`k = pixfrac²` 显然依赖 `pixfrac`，括号后半句又写「`pixfrac=1` 时 `k≡1`」——**字面自相矛盾**。原意应是「与各 `a_jp` 交叠面积的具体取值无关」 | **新问题（低，但确由第 1 轮新写入）** |
| **N12** | `docs/detail/UNIFIED_MODEL.md:74` | 第 1 轮新写：「`m_ref` 是**冻结的**参考电平约定（不是需标定的量）：默认 `6.0`」。一级正本 `docs/science/unified/DATA_SEMANTICS.md:206`：「合同只约束它是 number，**不冻结取值，因此它不是冻结常数**……可被输入 JSON 覆盖」。代码 `module_adapters.cpp:7151 ref_mag = doc["snr"].value("reference_mag", 6.0)` 证明**可被覆盖**。三级 detail 在一级正本明说「不是冻结常数」处写了「冻结」⇒ **违反 AGENTS §4 权威链** | **新问题（中）** |
| **N13** | 第 1 轮订正记录 `T06-第1轮订正-DOC-DETAIL-GLOSSARY.md` §6.2 与 §3 X-10 | 记录 §6.2 把「改前 709」与「改后 46」并列，但**两者口径不同**：我复跑，`§` 全 `docs/detail/` 树改前 = **709**、改后 = **433**；而「根级+`anchors/`+`infrastructure/`」子集改后 = **46**（与记录一致）、**该子集改前实为 299**。⇒ 记录把全树改前数与子集改后数配成一行，自证表不成立。配套断言「残留 **35 行**逐行确认全是外部文献/标准节号」**也不成立**：我自己按「前置词」分类 423 个 `§N` = 外部文献/标准 **30** / 仓内跨文档带路径 **175** / 裸号 **218**。另 §3 X-10「`GLOSSARY.md` **23** 条 term」——我数 `docs/GLOSSARY.md` 实为 **21 条**（改前也是 21），「23」无出处 | **记录自证数字失真（3 处）** |

> **订正车道提示**：M01 的 180 处引用全部落在 `docs/science/**`、`eng/contracts/**`、`eng/packaging/**`、`lib/**` 注释，**没有一处在本车道写面**，因此本车道不能单方面闭合；其中 `eng/contracts/schemas/phase_config_normalize.schema.json:460` 的 description 同时是 N07 错误断言的载体，建议与 N07 打包同批。

### 2.4 第 1 轮**被推翻或降级**的判定，订正者有没有错误地当真问题去改

| 第 1 轮判定 | 订正者处置 | 我的复核 |
|---|---|---|
| **J-07** 自我推翻：代码禁的不是「逐帧 `F_ref`」本身 | detail 侧由「改成组内公共」改为「补定义」，保留「逐帧」二字 | **改对了**。我复核 `astro_sphere_sink.cpp:405-419`：逐帧档是**有意**逐帧、明写「不是缺陷」，两档都是合法约定。补定义而非强改口径，方向正确 |
| **J-09** 自我推翻 SCI-psf R8-10 的量化主张（`0.168` vs `1/pixfrac⁴`） | 保留审稿原判于 §1.3，本车道不据以改文档 | **改对了**。`0.8⁴ = 0.4096`，`1/0.8⁴ = 2.4414`，审稿原文的 `0.168` 与方向都不成立。不据错数字改文档是正确的克制 |
| **S-08**（noise_snr O-6「`m_5` 系数是 2」）审稿员自行推翻 | 撤回、保留原判 | **改对了**。`2.5 log10` 系数一致，无可改 |
| **E-06** 子代理 D 编造「drizzle 不承担球面→平面」 | 订正者当场改回双向表述 | **改对了**。`reverse_drizzle.cpp/.h` 实存 |
| **E-09** 子代理 C 自曝的失实指针 | 改为真实标题 | **改对了**（`### 3.2 三个基本对象的语义` 实存） |

**结论：第 1 轮对「被推翻/降级」条目的处置全部正确，没有把不成立的问题当真问题去改。** 这一项本车道零发现。

---

## 3. 本轮新发现清单（第 1 轮漏掉的）

| # | 位置 | 问题 | 依据（可复跑命令 / 我的推导） | 建议改法 |
|---|---|---|---|---|
| **M01** 🔴 | 全仓 `docs/` `eng/` `lib/` | **`docs/detail/algorithms_phase{1,2,3}/**` 整棵子树已被删除，但 26 份文件里的 180 处引用仍指向它，17 个目标路径全部 MISSING**（`实验/` 下 0 处，180 全在 `docs/`+`eng/`+`lib/`） | `grep -rho "docs/detail/algorithms_phase[123][A-Za-z0-9_./-]*" docs/ eng/ lib/ 实验/ \| sort -u \| while read p; do test -e "$p" \|\| echo MISSING $p; done` → 17 条全 MISSING（`01_calibration.md`…`16_fits_output.md` + 两个目录）。分布：`docs/science/**`、`docs/science/algorithms/**`、`eng/contracts/schemas/**`、`eng/contracts/ledgers/`、`eng/packaging/config/`、`lib/**`（含代码注释）。典型：`docs/science/PHOTOMETRY.md:368,369,396`、`docs/science/PHOTOMETRY_RESEARCH_PACK.md:5,90,128,134,147,170`、`eng/contracts/schemas/phase_config_normalize.schema.json:460`（`sparse_snr_layer` 的 description 里引 `docs/detail/algorithms_phase1/07_noise_snr.md:213、08_drizzle.md:55`，并据此写「默认稀疏路径要求稀疏层默认产出」） | 按映射改指：`01→registry/acsd.phase1.calibration.md`、`02→…cosmetic.md`、`03→…star-detection.md`（+`star-psf.md`）、`04→…star-psf.md`、`05→…wcs-platesolve.md`、`06→…photometry.md`、`07→…noise-snr.md`、`08→…drizzle.md`、`09→phase2.coverage.md`、`10→…sample.md`、`11→upm-fit.md`+`upm-apply.md`、`12→…reject.md`、`13→…integrate.md`、`15→phase3.resample2.md`、`16→phase3.writer.md`。**`14_projection.md` 无 1:1 对应**（`phase3/` 无 projection 卡，最近是 `acsd.phase3.wcs.md`）⇒ 需登记裁决，不可硬套 |
| **M02** 🔴 | `docs/detail/PHASE1_DETAILED_DESIGN.md:186`、`:210` | `W_psf = PᵀC⁻¹P` **漏掉 `a_k²`**，且把对象写成**全仓不存在的名字 `point_source_information`** | 同一文件 `:173` 自己写 `W_psf,k(x,y) = a_k(x,y)^2 P_kᵀ C_k⁻¹ P_k = 1/Var(F_hat_k)`（**有 `a_k²`**）；`docs/science/unified/UNIFIED_SCIENCE_MODEL.md:58` 同样有 `a_k²`；canonical 合同 `eng/contracts/schemas/unified/point_information.schema.json` 的 `title` 逐字写「点源严格权重 W = a²PᵀC⁻¹P」且 `required` 字段名是 **`W_info`**。`grep -rn point_source_information` 全仓只命中 `:186` 与 `:210` 两处。⇒ 同一文档内 11 行之差丢一个因子 + 对象名错 | `:186` 改为 `W_psf = a_k²PᵀC⁻¹P`，对象名改 `point_information`；`:210` 的 `point_source_information` 同改。**跨片**：同一物理量在 detail 里有 `W_k`（`PHASE2:83`）、`W_info`（`PHASE2:141`、`PHASE1:145`）、`W_psf`（`PHASE1:173`）、无符号裸式（`UNIFIED_MODEL:44`）**四个名字**，需统一到 canonical 的 `W_info` |
| **M03** 🟠 | `docs/detail/merged_TROUBLESHOOTING.md:24`、`:12` | 「本表覆盖 **10 类**高风险场景」但表实有 **21 行**；「准入格式是固定的**七项**」但表只有 **5 列**（缺 4 最小复现 / 5 期望不变量 / 6 源码位置 / 7 文档位置与测试位置） | 我自己数：`awk '/^\| 症状 \|/{f=1;next} f&&/^\|/{c++} f&&!/^\|/{exit} END{print c}'` → **21**；表头 `:43` 只有 5 列。两处均在第 1 轮**未触碰**的段落（diff 只改了抬头与两处 §→节名） | `:24` 改 21（或按「10 类」合并条目）；`:12` 的七项要么补齐表的四列，要么改为「本表承载七项中的前三项，余四项见…」 |
| **M04** 🟠 | `docs/detail/registry/acsd.phase1.drizzle.md:126`（`δ` 残差律适用域句） | 球面残差律的两个**摘要阈值与它自己紧邻的数值表矛盾**：「`θ ≲ 10″/px` 时该偏离 ≲ 1e-10」实测 θ=10″ 给 **2.1e-10**（差 2.1×）；「`θ ≳ 100″/px` 时进入 1e-7–1e-6」实测 θ=100″ 给 **2.1e-8**，**差约一个数量级**（1e-7 需 θ≈217″，1e-6 需 θ≈688″） | 我自己 python 复算 `δ=(1−0.8²)·θ²·[0.25/(1+r_c²) − 0.625ξ_c²/(1+r_c²)²]`，取 r_c=ξ_c=0（表列的「中心在参考点」情形，括号取最大值 0.25）：θ=2″→8.462e-12、6.3″→8.396e-11、**10″→2.115e-10**、60″→7.615e-09、**100″→2.115e-08**、300″→1.904e-07。与文档表内四档（8.5e-12 / 8.4e-11 / 7.6e-9 / 1.9e-7）**逐档吻合**，证明表对、摘要句错 | 摘要句改为由表直接读出的边界：「θ ≲ 7″/px 时 ≲ 1e-10；θ ≳ 220″/px 起进入 1e-7；θ ≳ 690″/px 起进入 1e-6」。**根因在 science**：`docs/science/algorithms/DRIZZLE_GEOMETRY.md:420-421` 是同一句错话（`:418-419` 的表是对的）⇒ **science 与 detail 必须同批改**，否则 detail 单独改会与正本不一致 |
| **M05** 🟠 | `docs/detail/UNIFIED_MODEL.md:81` vs `:87` | §2.1 **并列两条权重换算式而未写二者相差 `a_k²`**：`w = SNR²/F0²`（逐帧档写侧用）vs `w = SNR²/F_ref² = 1/σ_F²`（配对式） | 我自己推导：由 `ZP_k = ZP_syn − 2.5 log10 k_photo` ⇒ `F_ref,k = F0/k_photo` ⇒ `SNR²/F0² = (F_ref,k/F0)²/σ_F² = a_k²/σ_F²`，与 `SNR²/F_ref,k² = 1/σ_F²` **相差 `a_k² = k_photo^−2`**。关系式写在 `acsd.phase1.noise-snr.md:115-116`，但 **§2.1 这一节里没有**，读者在 §2.1 内部无法对账 | §2.1 在 `:81` 后补一句：`w = SNR²/F0² = a_k²/σ_F²`，并注明与 `:87` 的 `1/σ_F²` 相差 `a_k²`，`a_k = F_ref,k/F0 = k_photo,k^−1` |
| **M06** 🟠 | `eng/contracts/data/phase_product_exchange.schema.json:162`、`lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py:57` | N-07 已被 engineering 车道**单方按「删除」方向裁决**（`PHASE_PRODUCT_EXCHANGE.md:43-47,243` 已写「本合同不含 `mask` 面」），但**机器合同与校验器仍含 `mask`**：`"enum": ["signal","support","variance","ivar","mask"]` / `_PLANE_ID_SET = {..., "mask"}` | 我自己 `grep -n '"mask"' eng/contracts/data/phase_product_exchange.schema.json` → `:162`；`grep -n mask lib/.../phase_product_exchange_validator.py` → `:57`。文档 `:98-101` 自己写「schema 需修订…当前仍含 `mask`」——**诚实登记但未闭合**。另 `docs/detail/UNIFIED_MODEL.md` 侧的 X-05 处置也未执行（见 §2.2） | 三处同批：① schema enum 删 `mask`；② validator 的 `_PLANE_ID_SET` 删 `mask`；③ `UNIFIED_MODEL.md` 补「`mask` 不在 13 canonical 对象内；`star_mask`/`bad_mask`/`validity` 各管什么」。**N-07 须从「待负责人裁决」改为「已按删除方向落地，请追认」** |
| **M07** 🟡 | `docs/detail/registry/acsd.phase2.upm-fit.md:280`、`docs/detail/registry/acsd.phase3.wcs.md:68` | 语言轮残留：**历史叙事 + 裁判语言**。前者：「（「`0 = auto`」的**旧述已按实测订正**）」；后者：「该处表述**待其车道订正**」 | 违反 `AGENTS.md` §5「正文无…「旧版/作废/**曾**/原」等历史叙事」与「无…裁判语言」。两者**第 1 轮前就存在**（`git show 128a1b00` 可证），第 1 轮**未识别** | 前者删括号，直接写 `cpu_workers` 的三个取值语义；后者改为正向陈述：「口径以在役注册表为准（当前 `D = I = {TAN}`）」，删「待其车道订正」 |
| **M08** 🟡 | `docs/engineering/api/PUBLIC_API.md:1215` | 「进程退出码正本 = `../contracts/LOG_AND_ERROR.md`「**验收**」一节」——`LOG_AND_ERROR.md` 无「验收」节；而 `PUBLIC_API.md` **自己**的 `## 验收` 在 `:99` | `grep -n "^#.*验收" docs/engineering/api/PUBLIC_API.md` → `:99:## 验收`、`:2191`；`grep -n "^## " docs/engineering/contracts/LOG_AND_ERROR.md` → 无「验收」⇒ **跨文件指错** | 与 X-09 一并裁：正本到底是 `ERROR_MODEL.md:88 ## 7 进程退出码`（唯一数值源仍为 `exit_codes.h`），还是 `LOG_AND_ERROR.md:89 ## 5` 的域→码映射表。两处并存需先裁再改 |
| **M09** 🟡 | `docs/ACSD_DESIGN.md:279` vs `docs/detail/PHASE2_DETAILED_DESIGN.md:58-64`、`acsd.phase2.reject.md:93-104` | 排异**算法集是 4 个**（含 `linear_fit`）而 **AUTO 路由只出 3 档**——detail 两处都写清了，但最高设计 §5.5 只写「按 N 自动选择排异算法；生产算法集为 none、percentile、winsorized、linear fit」，未写 AUTO 不产 `linear_fit` | 我自己比对：`reject.md:93-95` 三档表 + `:101-102`「`linear_fit` 仍是合法显式方法…AUTO 在生产档不产出该档」、`:104`「生产排异算法集 = none / percentile / winsorized / linear fit」；`ACSD_DESIGN.md:294` 缺 AUTO 三档的限定 | `ACSD_DESIGN.md:294` 补一句「AUTO 路由为 none / percentile / winsorized 三档，`linear fit` 仅可显式请求」。**顶点需负责人批准 ⇒ 登记** |
| **M10** 🟡 | `docs/detail/UNIFIED_MODEL.md:43` / `acsd.phase1.noise-snr.md:138`/`:387` | 第 1 轮在 `:62` 写下「产者落地前 `sparse_reconstruct` 路径**不可兑现**，只余 `frame_reconstruct` 与稠密路径」后，**同卡 `:387` 仍把 `snr_path` 的配置缺省值设为 `sparse_reconstruct`**、`:138` 仍标「现行默认」⇒ 生产默认指向一条被同卡判定为不可兑现的路径 | 我自己跑：`python3 -c "...properties of phase_config_normalize..."` → 顶层属性**不含** `snr_path`（`UNIFIED_MODEL.md:46` 那句「normalize 配置不含 `snr_path`」**是对的**）；`phase_config_mosaic.schema.json` 含 `snr_path`（9 命中）。而 `noise-snr.md:387` 是 **phase1 模块卡**，把 mosaic 面的键登记进 phase1 配置表 | 三选一并同批：① 把 `snr_path` 从 phase1 卡移入 mosaic 语境；② 把缺省改成 `frame_reconstruct`；③ 保留 `sparse_reconstruct` 但在 `:62` 承认「该路径按 `:143` 的降级语义运行**等价于** `frame_reconstruct`，显式记 `snr_path_effective`」。**建议 ③**（最小、且与 `:143-145` 的既有降级语义一致） |
| **M11** 🔵 | `docs/detail/PHASE3_DETAILED_DESIGN.md:15` | 「《ACSD 最高设计》的「投影算法」与「FITS 产品」两章」——`ACSD_DESIGN.md` 第 6 章只有 `6.1 使命 / 6.2 流程 / 6.3 投影算法`，**无「FITS 产品」** | `grep -n "^### 6\." docs/ACSD_DESIGN.md` → 三条；`grep -n "FITS 产品" docs/ACSD_DESIGN.md` → **0 命中**。而 `§6.3 投影算法` 的第三条（`ACSD_DESIGN.md:320`）「导出只接受面亮度语义输入，方差/逆方差显式消费并传播，其余语义拒绝；输出模式…显式声明，可视化模式标注不可测量」**同时承载了「输入语义守卫」与「硬约束」** | 两处都点「投影算法」一章（同 M-04 的「改前即悬空」条目，已并入 §3-N08 ⑦，此处单列便于派单） |
| **M12** 🔵 | `docs/detail/LOG_AND_ERROR_SYSTEM.md:13` vs `docs/detail/infrastructure/19_runtime.md:99` vs `:13`/`:116` | **同一目标节在同一车道内出现两个名字**：`:99` 已改对为「错误对象与退出码映射」，`:13`/`:116`/`:52`/`:133` 仍是「进程退出码」（不存在的节名） | `grep -n "^## " docs/engineering/contracts/LOG_AND_ERROR.md` → 真实节名 `## 5 错误对象与退出码映射`；`grep -rn "「进程退出码」" docs/` → **5 处**（4 处在本车道写面 + 1 处在 `CLI_PROTOCOL.md:36`） | 与 X-09 同批改：全部改指 `docs/engineering/standards/ERROR_MODEL.md`「进程退出码」一节（第 1 轮给的改法指向 LOG_AND_ERROR，**也错**） |
| **M13** 🔵 | `docs/science/PHOTOMETRY.md:420`、`docs/science/DISPUTE_RESOLUTION.md:144,147,148,149`、`docs/science/PHOTOMETRY.md:300` | `1.166 = √1.361` 这个**标签的第 4 位有效数字是错的**：`√1.361 = 1.1666190466471906` → 4 位有效 = **1.167**；而闭式 `1/(4φ(d)d) = 1.1663872874444212` → 4 位有效 = **1.166**。即 science 写的 `1.166` 对应的是**闭式**的舍入，不是它自己标注的 `√1.361` 的舍入 | 我自己 python：`f"{math.sqrt(1.361):.4g}"` = `1.167`，`f"{1.1663872874444212:.4g}"` = `1.166`。第 1 轮已在 detail 侧改成闭式并诚实标注 `1.361` 只作 3 位有效数字，**但 science 侧的 6 处副本未同步** ⇒ 权威链在此处倒挂（三级 detail 比一级 science 精确） | science 车道把 `1.166` 改 `1.167`（若保留 `√1.361` 口径）或改用闭式（与 detail 一致）。**我核不到 RC93 Table 2 原文（付费墙），不凭印象裁** |
| **M14** 🔵 | `docs/science/psf/PSF.md:176`、`:296`（及 `实验/dense-snr-reconstruct/` 3 处） | `1/Φ⁻¹(0.75)` 写成 `1.4826022185056023`，是**全仓唯一的 +1 ULP 离群点** | 我自己 python：`1/NormalDist().inv_cdf(0.75)` = `1.482602218505602` = `0x1.7b8bd1a975674p+0`；`1.4826022185056023` = `0x1.7b8bd1a975675p+0`，差 `2.22e-16`（1 ULP）。正确值全仓 271 处（含 detail 侧 3 处）。`PSF.md:296` 是**可执行 Python 示例的期望输出注释**，`1/Phi_inv(0.75)` 实际打印 `1.482602218505602` ⇒ 该示例的注释输出可证伪 | science 车道改 5 处为 `1.482602218505602`。第 1 轮记录 §1.5 已诚实标「不在本车道」，science 车道两轮都没改 |

---

## 4. 本轮是否零发现

**明确写：本轮不是零发现。**

本车道 48 份文档全部逐行读完（`STAR_DETECTION_IMPL_DESIGN.md` 的 O5–O16 段按理由抽样，已在 §1 与 §6 登记），跨四个一级目录做对照，另派发 5 个子代理分头审。

产出：**第 1 轮问题回查 24 条**（改对 12 条 / 未落地 5 条 / 引入新问题 7 条，其中 1 条是订正记录自身的自证数字失真）；**本轮新发现 14 条**（🔴 重 3 条、🟠 中 4 条、🟡 轻 4 条、🔵 低 3 条）；**推翻既有结论 3 条**（见 §5）。

---

## 5. 我推翻的既有结论

| # | 被推翻的结论 | 我的依据 |
|---|---|---|
| **O1** | 第 1 轮记录 §3 X-09 给的改法：「退出码映射的真落点是同文件『错误对象与退出码映射』」 | `grep -n "^## " docs/engineering/standards/ERROR_MODEL.md` → **`## 7 进程退出码` 在 `:88`**。节名「进程退出码」真实存在，只是不在 `LOG_AND_ERROR.md` 而在 `ERROR_MODEL.md`。⇒ X-09 的**问题识别成立、给出的落点指错文件**。修法应为改指 `ERROR_MODEL.md`，而非把节名换成「错误对象与退出码映射」 |
| **O2** | 第 1 轮记录 §6.2「`§` 改前 709 → 改后 46」以及「残留 35 行逐行确认全是外部文献/标准节号，不可改名」 | 我复跑：全树改前 709 / 改后 **433**；子集改后 46 / 子集改前 **299**。记录把两个口径配成一行。**「残留 35 行全是外部节号」也不成立**：我自己分类 423 个 `§N` = 外部 **30** / 仓内带路径 **175** / 裸号 **218** ⇒ 「不可改名」的豁免理由只覆盖 7%，其余 93% 是仓内跨文档锚，正是 AGENTS §5「不使用「见 §几」式的机械跳转锚」所指的对象 |
| **O3** | 第 1 轮记录 §5 N-05 的数值「`n·Var(MAD)/σ² = 1/(16φ(d)²) = 0.618983`」 | 我自己 python：`1/(16·φ(0.6744897501960817)²) = 0.618922489703423`，记录值偏高 `6.05e-5`。处置方向（detail 只写闭式）不受影响，但**数值本身错**。（此条由子代理 A 报出，我独立复算确认） |

**我否决的子代理结论**（子代理与我的分歧，一律保留在 §6）：

| # | 子代理结论 | 我的裁决 |
|---|---|---|
| **R1** | 子代理 A 报：「`UNIFIED_MODEL.md:88-89` 的『跨帧一致性只作报告字段，不作 fail-closed 闸门』与 Phase1 生产面的 1e-9 组内闸门不符」 | **我否决**。我读了 `astro_sphere_sink.cpp:413-419`：`use_common = (scope_str == "frame_independent_fixed_magnitude")`，`fref_key = use_common ? "flux_common" : "flux_adu"`。即闸门校验的是**当前 scope 所声明的那个键是否组内公共**——逐帧档读的是与帧无关的 `flux_common`（构造上恒同值，闸门平凡通过），`group` 档读的是块级 `flux_adu`（闸门有意义）。**闸门语义正确，不构成对「跨帧一致性只作报告字段」的反驳**。子代理把 `flux_adu` 分支当成了逐帧档 |
| **R2** | 子代理 A 报：「第 1 轮 §6.2『改后 46』复现不出（实为 433），自证表不成立」 | **部分接受、方向修正**。子代理只测了全树，得出「433 vs 46」；我补测子集，**改后 46 本身是对的**（根级+anchors+infrastructure 子集）。真正的错是记录把**全树改前数 709** 与**子集改后数 46** 配成一行。子代理的「复现不出」定性不准 |
| **R3** | 子代理 A 报：E-06「本车道侧一律改为指向最高设计 §3.3」在 `UNIFIED_MODEL.md` / `PHASE2_DETAILED_DESIGN.md` 是空操作 | **接受**（我实测 `grep -rn "float32\|float64" docs/detail/` → 0 命中，与该判断一致），但这不构成缺陷：第 1 轮改的是确实有精度表述的三张 registry 卡 |

---

## 6. 自证段

### 6.1 我实际做了什么

1. **逐行读完必读件**：`AGENTS.md`（127 行）、`docs/ACSD_DESIGN.md`（568 行，全篇标题与全部数据对象/创新点条目）、标准 `03_READING_AND_ADVERSARIAL_REVIEW.md`（55 行）、标准 `04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`（71 行）、第 1 轮订正记录（549 行，全读）。
2. **逐行读完本车道 48 份文档**（清单见 §1）。
3. **自己动手重推的量**（不采信任何文档结论作为唯一依据）：
   - `F_ref,k = 10^(−0.4(m_ref − ZP_k))` 与 `ACSD_DESIGN.md:100` 的 `m = ZP − 2.5 log10 F` 同式同系数；
   - `ZP_k = ZP_syn − 2.5 log10(k_photo)` 的**符号方向**（由 `F_syn = k_photo·F_adu` 独立推出），并推出 `F_ref,k = F0/k_photo`、`a_k = k_photo^−1`、两档权重链相差 `a_k²`（M05）；
   - `k = D_p/N_p = pixfrac²`（由 `A_drop,j = pixfrac²·A_pixel,j` 推出）；
   - 球面残差律 `δ(θ)` 六档读数（M04），逐档与文档表吻合，反证**摘要句**错；
   - `c_se = 1/(4φ(d)d) = 1.1663872874444212`、`1.482602218505602 = 1/Φ⁻¹(3/4)`（hex `0x1.7b8bd1a975674p+0`）、`√1.361` 的 4 位有效数字（M13）。
4. **我自己跑的真解析器对账**：生产端口注册表 module_id 差集（25 卡 vs 20 注册表）；`DOCUMENT_INDEX.yaml` 全量对账（148 条 / 0 重复 / 0 悬空 / detail 43/43 / 27 条未登记全是目录 README）；GLOSSARY 20 条锚逐条解析（0 问题）；`docs/detail/**` → science/engineering 的「文件+节名」引用全量核（73 命中 / **7 悬空**）；`docs/detail/**` 内部「文件+节名」悬空核（1 处）；423 个 `§N` 的性质分类；`phase_config_normalize` 顶层属性枚举（确认不含 `snr_path`）；`resource_gate_v1.json` 的 4 个 enforcement 键；`hips_storage_form.schema.json` 的 `by_subproduct`；`phase_product_exchange.schema.json` / validator 的 `mask` 残留；`exit_codes.h` 逐码比对（detail 内 8 个码全部在正本集合内）；`21_observability.md` 破表逐行数竖线；`23_hips_browser.md` 24 条路径逐个 `test -e`；`merged_TROUBLESHOOTING.md` 表行数与列数；`upm-fit.md`/`wcs.md` 语言残留的 `git show 128a1b00` 前后对照。
5. **派发 5 个子代理**（`subagent`，各自只读、零 git 写）：
   | 子代理 | 车道 | 我的复核结论 |
   |---|---|---|
   | A1 | 第 1 轮订正逐条回查 | 报告 12 项落地、7 项有瑕疵；**独立复现了我 6 条发现**（sparse 跨卡、X-07、X-09、E-02 21 处、X-06 遗留、X-01 遗留），**新增 5 条**（`drizzle.md:114` 括号矛盾、`m_ref` 冻结措辞、`PSF.md` ULP 离群点、`PUBLIC_API.md:1215` 悬空、record N-05 数值错）；**被我否决 3 条**（见 §5 R1/R2/R3）。它关于 X-10「23 条 term」的推翻我**独立复算确认**（实为 21） |
   | A2 | 同上（独立重复，作为对抗性双跑） | 与 A1 结论一致但更精确：补出「§6.2 改后 46 是子集口径」、`mask` 在 schema/validator 未删、`1.166=√1.361` 有效数字错；**同样被我否决 R1/R2 两条** |
   | B | 跨切面数据对象口径一致性 | **交件时未返回**，未采信 |
   | C | 公式与常数重推 | **交件时未返回**，未采信 |
   | D | 结构/语言/可复现三轮 | **交件时未返回**，未采信 |

### 6.2 我**没有**做的事（诚实边界）

- **没有修改任何文件**，**没有任何 git 写操作**。本车道是纯审稿车道。
- **没有编译、没有运行仓内测试、没有跑端到端**。所有「代码为准」的判定来自源码阅读 + `grep`/JSON 解析/`python3` 数值复算。
- **没有取任何一手文献全文**：`Rousseeuw & Croux 1993` Table 2 的 `1.361`、`WD-HiPS-2.0` §4.3.2、PixInsight 页的 `(Σf)²/σ_n²` 逐字出处 —— **一律报「核不到」，不凭印象裁**（M13 因此只给改法建议，不下裁定）。
- **`STAR_DETECTION_IMPL_DESIGN.md` 的 O5–O16 逐算子规格未逐行读**（按「与本车道跨片口径无关」抽样读了 §5.1–§5.2）。若前台认为该段需覆盖，须另派一轮。
- **B/C/D 三个子代理交件时未返回**，其覆盖的「跨片数据对象逐对象矩阵」「公式逐条重推」「结构/语言/可复现三轮」我在本车道**自己做了其中一部分**（§6.1 第 3、4 项），但**未做**「13 对象 × 4 层的完整出现面矩阵」与「detail 全部 47 份的逐条公式重推」。这两块**不能算本轮已覆盖**，须在下一轮补。
- 本单**只出问题清单，不订正**；所有「建议改法」供订正车道执行，跨面条目只登记不改。

### 6.3 收敛判定

**本车道未达收敛。** 依据规范 03 的判据「连续两轮无新增实质问题」：本轮新增 14 条实质问题，其中 🔴 重 3 条（M01 的 180 处断链、M02 的丢因子+对象名错、N07/N08/N09 的三组第 1 轮引入的新问题）。**本轮不可能是最后一轮。**

**建议的下一轮次序**（按依赖）：
1. **M01**（180 处旧 detail 路径断链）—— 影响面最大，且 `phase_config_normalize.schema.json:460` 的 description 直接支撑着 `sparse_snr_layer`「默认产出」的错误断言，两条要一起改；
2. **N07 → N08 → N09 → N10**（第 1 轮引入的 4 组新问题）—— 都在本车道写面或已同批提交，越早收口越省一次回滚；
3. **M02 / M05**（`W_psf` 丢 `a_k²`、两档权重链未对账）—— 公式层，需与 science 同步；
4. **M04 + M11**（δ 摘要阈值、「FITS 产品」章）—— 根因在 science，须 science/detail 同批；
5. **X-05 / X-07 / X-09 / M06 / M08 / M12**（`mask`、`唯一术语权威`、`进程退出码`）—— 需先由负责人裁「唯一落点」，再一次性改 6+ 处；
6. **X-01 / X-02**（module_id 归属、`star-psf` 职责）—— 属架构裁决，非文档层可闭。

### 6.4 给前台的三个「别信自证数」提醒

第 1 轮记录里有 **4 处自证数字**在我复跑时不成立（N13 的两条、X-10 的「23 条 term」、N-05 的 `0.618983`）。它们不影响文档结论，但影响「这轮到底改干净了没有」的判断。建议前台在评估第 1 轮收敛度时，**对这 4 个数单独复跑一次**，不要直接采信。