# UNRESOLVED · G08-05 待裁决台账

> 依 `AGENTS.md` §8「无法裁决的事项登记为 UNRESOLVED，写明分歧、影响与所需输入，随审核包交付」与
> `ACSD治理工作包_GOVERN-08/PROMPT.md`「无法裁决事项登记 UNRESOLVED 随审核包交付」编制。
>
> **本件性质**：本件是**汇总件**，不含任何裁决。「建议」列全部是**建议，不是裁决**；处置权按 `AGENTS.md` §11 只属于项目负责人。
>
> **编制口径**：
> - 每条编号 `G08-05-U<n>` 稳定、一次性，不合并不同事项。
> - 「出处」两侧都给：仓内 `文件:行` ＋ 源交付件章节。
> - 凡本次汇总者**亲自在仓内核证过**的，在「核证」栏标 `✔已核证` 并给命令可复现的实测值；未亲验的标 `转引`（来自源交付件，本次未独立复算）。
> - 本次汇总**只读**：未改任何仓库文件、未做 git 写、未编译、未跑测试、未联网。
> ⚠️ **基线偏差登记（重大，处置前必读）**：本件全部 `文件:行` 引用**以 HEAD `1208d5a0` 为唯一基线**。
> 汇总期间仓内出现**并行的整改车道改动**并仍在进行中，`git status` 已达 **134 条**
> （**102 `M` / 29 `D` / 3 `??`**）。实测关键变化：
> - **16 份机读资产已从 `docs/` 全部删除并迁往 `eng/contracts/data/`**：`docs/architecture/`（6 份全删）、
>   `docs/contracts/`（4 份全删）、`docs/modules/MODULE_MAP.yaml` + `docs/modules/registry/module_id_migration_baseline.json`（2 份全删）、
>   `docs/traceability/`（4 份全删）、`docs/TRACEABILITY.csv`（删）。新址 `eng/contracts/data/` 已有
>   `config_separation_anchors.json`、`contract_index.yaml`、`unified_object_registry.json`（未跟踪 `??`）。
>   **该去向与 `docs/DOCUMENT_INDEX.yaml:23-24` 自订去处 `eng/contracts/{anchors,data,ledgers}/` 完全一致**，
>   即本件 `G08-05-U1` 的「迁出」方向**正在被执行** —— 但**裁决尚未在审核包内登记**，
>   且 U1 的另一半（`docs/README.md` 去留、`DOCUMENT_GOVERNANCE.md:24-26` 删除或改写）是否同批处置尚未知。
> - 另有 `README.md`、`docs/ACSD_DESIGN.md`、`docs/GLOSSARY.md`、`docs/engineering/*`（15 份）、
>   `docs/science/*`（8 份）、`docs/detail/*`、`eng/**`、`lib/**`、`实验/*` 等被修改。
> - **本汇总单未做任何写操作，也无 git 写权限，不能也不应自行回退。**
> - **处置建议**：本件全部引用请按 `1208d5a0` 解读；**裁决前须冻结一个基线 commit**，
>   否则负责人逐条核对时会同时遇到「行号漂移」与「文件已删」两类问题。

## 0. 统计

| 阻塞级别 | 条数 |
|---|---|
| 阻塞级别 | 条数 | 编号 |
|---|---|---|
| 阻断提交 | **27** | `G08-05-U1` … `U27` |
| 阻断下一任务 | **16** | `G08-05-U28` … `U43` |
| 不阻塞但须登记 | **18** | `G08-05-U44` … `U61` |
| 小计（单独立条） | **61** | |
| 另见 §四 合并登记 | 12 | 表 a–l（多为「按清单执行」而非双方对立待裁） |
| **总计** | **73** | |

> ⚠️ **读表提醒**：**U5 / U8 / U9 / U10 / U11 / U27 / §四-i / §四-j 八条指向同一个根因** —— 符号、单位、量纲、
> 口径类的一致性冲突（SNR 分子、`F_syn` 单位、drizzle 方差分母、NSIDE 档位、精度默认值、堆叠残差方差、退出码表）。
> 这八条**应当一次性打包裁决**，逐条裁会互相打架（先裁 U5 会改掉 U10 的参照系）。

> 「11 vs 13」这一表述：本次在仓内**未找到**任何「数据对象 = 11 个」的表述（`grep -rn "11 个" ` 在
> `docs/detail/UNIFIED_MODEL.md`、`docs/engineering/UNIFIED_OBJECTS.md`、`docs/science/UNIFIED_SCIENCE_MODEL.md`、
> `docs/ACSD_DESIGN.md`、`docs/contracts/` 全部 0 命中）。仓内实际并存的是 **13 / 14 / 附录A 10** 三个数，
> 见 `G08-05-U3`。若「11」出自另一未随附的载体，请补充出处，本件按**待核实**登记，不写进裁决。

---

## 一、阻断提交（27 条）

### G08-05-U1 · 16 份机读资产（+1 份 docs 根级台账）的归属

| 字段 | 内容 |
|---|---|
| **事项** | `docs/` 下 `architecture/ contracts/ modules/ modules/registry/ traceability/` 四旧目录共 16 份机读资产与 `docs/TRACEABILITY.csv`，到底算「工程正本的一部分」还是「该迁到 `eng/` 的机器数据」。 |
| **分歧** | **迁出方**：规范 01:46 原文「迁移完成后 docs 下无 algorithms、architecture、audit、contracts、modules、standards、traceability 等旧目录」；`docs/DOCUMENT_INDEX.yaml:23-24` 独立复述同一禁令并自订去处 `eng/contracts/{anchors,data,ledgers}/` + `artifacts/evidence/arch/`；`docs/engineering/DOCUMENT_GOVERNANCE.md:4` 自认「目录拓扑的正本在 DOCUMENT_INDEX.yaml」，即让权。**留位方**：`docs/engineering/DOCUMENT_GOVERNANCE.md:24-26` 与 `:38-39` 明写这五目录是「留原位的机器合同件…不由本文档的层归属覆盖」。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §3 阻断 B-1（:91-96）、§1 发现①（:18-27）；`G08-03-结构侦察.md` §10 U1（:554 行表内第 1 行）。仓内：`docs/DOCUMENT_INDEX.yaml:23-24`、`docs/engineering/DOCUMENT_GOVERNANCE.md:4`、`:24-26`、`:38-39`、规范 01:46。 |
| **核证** | ✔已核证：`for d in docs/*/; do ...` → `docs/architecture/` 6 份 md=0、`docs/contracts/` 4 份 md=0、`docs/modules/` 2 份 md=0（含 `registry/` 1 份）、`docs/traceability/` 4 份 md=0，合计 **16 份机读资产、0 份 md**；`docs/TRACEABILITY.csv` 为第 17 份。 |
| **影响** | 规范 01 §6 迁移完成判据**判红**，G08-03「已完成」结论在结构面被证伪；这是 `审稿-RR01` B-1、B-3、M-7、M-9 的共同上游。涉及 ≥5 个目录 + 16 份资产 + 1 份根级 CSV。**整改方向必须先裁**：迁出是改目录树，补 README 是把违规拓扑固化（`审稿-RR01` M-9 明确反对补写）。 |
| **所需输入** | 负责人确认采信「迁出」并指定 `eng/contracts/` 下的具体落点文件名；或改判「留位」并同步改写规范 01 §3.6/§6 与 `DOCUMENT_INDEX.yaml:23-24`。 |
| **建议** | **建议**采信迁出（三方证据一致、且唯一异议方 `DOCUMENT_GOVERNANCE.md` 已自让拓扑正本权）。但迁出涉及 `docs/` 拓扑重排与索引重扫，属结构面大改，**须负责人拍板后由专门车道执行**，不宜由审稿轮顺带改。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U2 · `docs/README.md` 是否属「只剩四类」之外

| 字段 | 内容 |
|---|---|
| **事项** | 删还是留 `docs/README.md` —— 它既不在规范 01 §1 列举的范围内，也不在 §6「只剩最高设计、索引、术语与三个文件夹」的枚举里。 |
| **分歧** | **留**：索引规则 2 称其为「入口 README」并豁免索引登记；`docs/README.md` 抬头自报上游为 `docs/ACSD_DESIGN.md §0/§0.1`，功能上是文档集入口。**删/改**：规范 01 §1 未列、§6 未提；且该文件自身与文件树不符 —— 它只列 `science/ engineering/ detail/ GLOSSARY DOCUMENT_INDEX` 四组，四旧目录与 `TRACEABILITY.csv` 一字未提，却在 :10 宣称索引「与文件树一致」（`审稿-RR01` M-7）。 |
| **出处** | 源件：`G08-03-结构侦察.md` §10 U2；`审稿-RR01-结构与索引.md` §3 须修 M-7（:171-175）、§2 表第 19 行（:73）；规范 01:61。仓内：`docs/README.md:5,:7-10,:10,:12`、`docs/TRACEABILITY.csv`。 |
| **核证** | ✔已核证：`docs/README.md` 存在，1285 字节；全文确只枚举 `science/` `engineering/` `detail/` 三目录 + `GLOSSARY.md` + `DOCUMENT_INDEX.yaml`，确未提四旧目录与 `TRACEABILITY.csv`。 |
| **影响** | 与 U1 耦合：若 U1 判迁出，`docs/README.md` 必须同步改写或删除，否则留下自相矛盾的一层说明。单文件，但直接违反「文档只写现行设计」。 |
| **所需输入** | 负责人裁定去留；若留，是否要求其内容与迁出后的文件树逐项对齐。 |
| **建议** | **建议**随 U1 一并处置：判迁出则删除并重建一个只反映迁出后拓扑的极简 README（规范 01:61 要求「一到三句话」）。不建议单独保留现文。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U3 · 统一数据对象集：13 / 14 / 附录A 10 三数并存 + 顶点追溯断链

| 字段 | 内容 |
|---|---|
| **事项** | 「现行统一数据对象集到底是几个」无唯一答案，且权威顶点对部分对象零追溯。 |
| **分歧** | **13**：`docs/ACSD_DESIGN.md:176`（权威顶点）「现行统一数据对象集共 13 个」；`docs/detail/UNIFIED_MODEL.md:56`「canonical 数据对象 = 13 个」；`docs/engineering/UNIFIED_OBJECTS.md:1,:5` 两处均写 13；`实验/裁决台账.md:292,:294`（GA-04，负责人已裁「彻底删除」）明写「由 14 改为 13」。**14**：`docs/contracts/INDEX.yaml:831` 段头写「DATA-001，UNIFIED_MODEL §2 的 **14** 个对象」，且该文件确有 **14** 条 `DATA-OBJ-*` 条目（第 14 条即 `DATA-OBJ-PSFSW-ROBUST-WEIGHT-001`，`:891` status=OBSOLETE）——即 GA-04 裁决后 `INDEX.yaml` 的**段头文字未同步**。**附录A 10**：`docs/ACSD_DESIGN.md:666` 附录 A 只列 10 个，缺 `depth_m5`、末位多出 `control_ivar`，与同文件 :174（§3.1，11 个）也不一致。**科学分册另缺 3 项**：`docs/science/UNIFIED_SCIENCE_MODEL.md` 表仅登记 10 个对象，缺 `frame_snr` / `sparse_snr_layer` / `provenance`（`审稿-RR01` B-4）。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §3 阻断 B-4（:112-117）；`审稿-RR09-跨文档一致性.md` §3.1 B3（:121 行起）、§4 R3。仓内：`docs/ACSD_DESIGN.md:174,:176,:666`、`docs/contracts/INDEX.yaml:831,:891`、`docs/detail/UNIFIED_MODEL.md:3,:56`、`docs/engineering/UNIFIED_OBJECTS.md:1,:4,:5`、`docs/science/UNIFIED_SCIENCE_MODEL.md:30`、`docs/GLOSSARY.md:5,:16,:27`、`docs/engineering/UNRESOLVED_REGISTER.md:557-568`、`实验/裁决台账.md:292-304`。 |
| **核证** | ✔已核证：`ls eng/contracts/schemas/unified/*.schema.json` = **14** 个文件，其中 13 个是数据对象 + 1 个 `port_contract.schema.json`（非对象）；`grep -c "id: DATA-OBJ-" docs/contracts/INDEX.yaml` = **14**；`grep -rn "数据对象.*[0-9]\+ 个"` 在 `docs/`+`eng/contracts/` 全部指向 13。✔另核：`grep -rn "11 个"` 于四份对象正本 **0 命中**（父单提到的「11」在仓内无出处，标待核实）。✔另核：`docs/contracts/unified_object_registry.json:557-568` 的 `psfsw_robust_weight` 段 `state: retired`、`removed:` 列两条已删 schema 路径（属退役记录，非悬空），`:561` 的 basis 却引用不存在的节号 `§9.73 A44`（见 U21）。 |
| **影响** | 直接触发 `docs/ACSD_DESIGN.md:657`「追溯无断链」发布候选条件。`审稿-RR09` B3 补充：`source_snr` 是**全文零出现的真断链**（顶点列出但仓内无任何定义落点），`point_information` 是**概念在场、身份未登记**（`:329`、`:360` 有散文），两者修法与紧急度不同，不应并列处理。影响面：≥7 个文件、含权威顶点与两份一级正本。**阻断提交与发布条件判定**。 |
| **所需输入** | 负责人裁两件事：(a) `INDEX.yaml:831` 段头是否按 GA-04 改 14→13、`DATA-OBJ-PSFSW-*` 条目是否随之降为纯退役记录或移出；(b) 附录 A 与 §3.1 的 10/11/13 三处如何对齐；(c) `source_snr` 与 `point_information` 分别怎么补定义与登记；(d) `docs/science/UNIFIED_SCIENCE_MODEL.md` 是否为该主题的一级正本（若是，为何缺 3 项；若否，`UNIFIED_MODEL.md:3` 为何不引用它）。 |
| **建议** | **建议**：(a) 立即把 `INDEX.yaml:831` 段头改 13，并明确 `DATA-OBJ-PSFSW-ROBUST-WEIGHT-001` 是「退役留痕」而非「现行对象」，避免下次审计再数成 14；(b) 附录 A 应由脚本从 §3.1 生成而非手写；(c) `source_snr` 优先（真断链），`point_information` 次之；(d) 科学分册的缺项先补登记再定正本归属。以上均为**建议**。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U4 · 规范 01 §1 仍写旧名 `ASTROCS_DESIGN.md`

| 字段 | 内容 |
|---|---|
| **事项** | 规范 01 §1 里的仓名 `ASTROCS_DESIGN.md` 是「规范写错」还是「该重命名回旧名」。 |
| **分歧** | **规范写错**：仓内权威顶点实为 `docs/ACSD_DESIGN.md`，`AGENTS.md:21,:42` 亦写 `docs/ACSD_DESIGN.md`；G08-03 的改名轮已把 1,285 份旧串清掉，规范 01 是漏改的最后一处。**仓名待改**：无支持证据 —— 现名被 462 处 downstream 引用，改回去成本远高。 |
| **出处** | 源件：`G08-03-结构侦察.md` §10 U3。仓内：`docs/engineering/UNRESOLVED_REGISTER.md:1853,:1969,:2768,:3963`（「`ASTROCS_DESIGN.md` 已改名 `docs/ACSD_DESIGN.md`」）、`AGENTS.md:21,:42`。 |
| **核证** | ✔已核证：`grep -rn "ASTROCS_DESIGN" docs/` 仅 4 处命中，**全部在 `docs/engineering/UNRESOLVED_REGISTER.md`** 的历史叙述里（:1853 的扫描模式表、:1969、:2768、:3963），无一处是现行正本的活引用。规范 01 本体不在本会话文件系统内（`/tmp/acsd_g08/...` 不可达），故规范 01 §1 的行号**无法本次核证**，标转引。 |
| **影响** | 单点，但它是「规范 01 是权威之一」这一前提的可见破口；不改则每次引用规范 01 都会重新触发同一疑问。 |
| **所需输入** | 负责人确认「规范 01 §1 改为 `docs/ACSD_DESIGN.md`」；规范 01 不在本车道授权面，需改写授权。 |
| **建议** | **建议**按「规范写错」判，直接改规范 01 一处。同时建议把 `UNRESOLVED_REGISTER.md` 里那 4 处历史叙事一并按 `AGENTS.md` §5 的「无历史叙事」要求处理（但该文件归属面另有裁决，见 U47）。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U5 · SNR 取「形 A」还是「形 B」，须全链统一（跨 R04/R09 两条阻断）

| 字段 | 内容 |
|---|---|
| **事项** | 同一个符号 `SNR` 在权威链上有两套互斥定义（分子是被测源 `F` 还是参考源 `F_ref`），必须择一并全链统一。 |
| **分歧** | **形 A（SNR = F/σ_F）**：`docs/ACSD_DESIGN.md:124`；`docs/engineering/UNIFIED_OBJECTS.md:37`；`docs/science/CONTROL_WEIGHT_SNR.md:94`（§2b）把「分子只含源」列为不变量，另有 :171/:179/:182；实验侧 `b2_noise_terms.json` 的 `snr_def = F_adu/σ_F`（验算 84.30 与 `:1117` 吻合）。**形 B（SNR = F_ref/σ_F）**：`docs/ACSD_DESIGN.md:329`（§5.3）；`CONTROL_WEIGHT_SNR.md:214`（§8a 第 7 条）与 `:243`（§8c）把同名同符号的 `SNR(x,y)` 直接送进 `w = SNR²/F_ref² ≡ 1/σ_F²`；`eng/contracts/schemas/unified/sparse_snr_layer.schema.json:375`。**调和句候选**：`实验/absolute-snr/docs/frame-snr-canon.md:98`「帧级量与点 SNR 是不同量」——但该文件在 `实验/` 下，**不在 `docs/` 权威链内**，无裁决力。 |
| **出处** | 源件：`审稿-RR04-P2 跨帧绝对信噪比.md` §3 阻断 B2、§4 反例 D、§9 第 2 条（:455）；`审稿-RR09-跨文档一致性.md` §3.1 B5（:192-203）、§4 R4（:307-323）。仓内：`docs/ACSD_DESIGN.md:124,:127,:329`、`docs/science/CONTROL_WEIGHT_SNR.md:94,:171,:214,:243`、`docs/engineering/UNIFIED_OBJECTS.md:37`、`eng/contracts/schemas/unified/sparse_snr_layer.schema.json:322,:375`、`eng/contracts/schemas/unified/frame_snr.schema.json:322,:334-338`。 |
| **核证** | ✔已核证关键算术（转引并复算其结论）：取形 A 时 `w = S_src²/(F_ref²σ_F²)`，与 `1/σ_F²` 相差 `(S_src/F_ref)²`；取 `m_s = m_ref + 6`（6 等 = 251 倍通量比，出处 `docs/science/PSF_SIGNAL_WEIGHT.md:126`）得权重差 `251² ≈ 6.3×10⁴` 倍。 |
| **影响** | **顶点自身即不自洽**（`:124` 与 `:127` 相邻两行、`:127` 的恒等式代数上要求形 B、`:329` 又写形 B）。若重建算子喂一种、控制点存另一种，**马赛克权重错约四个数量级**。影响面：顶点 + 2 份一级正本 + 2 个机器 schema + 实验侧 `frame_snr`/`sparse_snr_layer` 两个落盘产品 + `docs/science/DATA_SEMANTICS.md:602,:605`。 |
| **所需输入** | 负责人择定形 A 或形 B，并授权全链统一改写范围。 |
| **建议** | **建议**选形 B（`SNR = F_ref/σ_F`）：理由是它同时满足 `w = SNR²/F_ref² ≡ 1/σ_F²`（`:127`）与 P5 顶点 `:156` 的 `w = 1/σ_F²`，且与 `frame_snr`/`sparse_snr_layer` 两个已落盘 schema 一致；代价是须重写 `CONTROL_WEIGHT_SNR.md` §2b 整节（观测量分解、稀疏先验、天光双计判据）的物理叙述。**但这是科学裁决，不是文档裁决**，`AGENTS.md` §4 要求三类证据互证后才下结论；本件只登记两侧证据，不代裁。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U6 · P2 的立身主张：「跨帧可比绝对 SNR」还是改写为「跨帧可比 σ_F / w / m_5」

| 字段 | 内容 |
|---|---|
| **事项** | P2 创新点的头条主张被本单元自己的六帧实测表证伪，是继续用现主张、还是把立身主张换成噪声量。 |
| **分歧** | **现主张成立**：`实验/absolute-snr/docs/frame-snr-canon.md:108` 宣称 `SNR_frame(k)` 恒等于同一颗参考源在本帧的点源 SNR 且「可跨帧直接比较」；G11（`:485`）判「已闭合」。**现主张不成立**（红队实测）：紧邻的 `:511-522` 六帧表显示，`σ_F` 小 1.4427× 的帧，`snr_f` 反而低 22.7%（21.364 vs 27.620）；`ρ(σ_F, snr_f) = 0.2571`，秩一致仅 1/6；误差因子 1.8651 恰等于 `F_ref` 之比 3224.51/1728.85 —— 即落盘的 `snr_f` 被逐帧测光零点 `ZP_k` 散布主导，而非被噪声 `σ_F` 支配。 |
| **出处** | 源件：`审稿-RR04-P2 跨帧绝对信噪比.md` §3 阻断 B1（:82-101）、§4 反例 A（:218-232）、§5「跨帧可比性」行（:269）、§9 第 1 条（:454）。仓内：`实验/absolute-snr/docs/frame-snr-canon.md:98,:108,:485,:511-522`、`docs/engineering/UNRESOLVED_REGISTER.md:416`（残余 0.7%–5.2%）。 |
| **核证** | 转引（本次未重跑实验；`AGENTS.md` §9 禁止 SubAgent 跑重活）。六帧表的数值全部取自源件 `:269` 的盲复算行。 |
| **影响** | 决定 `frame_snr` / `sparse_snr_layer` 两个数据对象**是否降格为诊断量**；两个方向工作量差一个数量级。并连带决定 `frame_snr.schema.json:334-347` 的 enum 收敛方向（两个方向都要求改 enum，见 U7）。影响 ≥5 个文件、2 个落盘产品。 |
| **所需输入** | 负责人裁决（源件明写「不可由 agent 自行订正」）。若裁改写，还需一并给「跨帧可比性的失效边界清单」—— 见 U42。 |
| **建议** | **建议**采纳改写方向（把 P2 立身主张改为「跨帧可比的 `σ_F` / `w = 1/σ_F²` / `m_5`」），依据是同一份实测表同时显示 `w = 1/σ_F²` **确实**跨帧可比（σ_F 序与 snr_f 序反向恰说明噪声量本身没坏，坏的是分子）。但须先补失效域清单，否则改写后的主张会带着未声明的适用域。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U7 · `UNRESOLVED_REGISTER.md:381` 的 SCI-P2-1 裁决状态是否回退为「待裁决」

| 字段 | 内容 |
|---|---|
| **事项** | 该条「已裁决」的依据（`:385` 的代数证明）被证为循环论证，是否把裁决状态回退。 |
| **分歧** | **维持已裁决**：`:385` 写「`w_k = SNR_k²/F_ref,k² · g_k²`，而 `SNR_k ≡ F_ref,k/σ_F,k`，代入后 `F_ref,k` 完全相消 ⇒ `w_k = g_k²/σ_F,k²`。权重链对 `F_ref` 的定义域无偏好」——**若形 B 成立，这个证明是对的**。**须回退**：该证明预设了形 B（见 U5），而形 B 本身是待裁项；改取形 A 则 `F_ref,k` 不消、权重对口径敏感。且该证明回答的是「权重链是否要求跨帧 `F_ref` 相等」（答案：否），却被静默平移为「SNR 跨帧可比性」（答案：恰相反）。 |
| **出处** | 源件：`审稿-RR04-P2 跨帧绝对信噪比.md` §3 阻断 B3、§4 反例 D（:250-256）、§9 第 2 条（:455）。仓内：`docs/engineering/UNRESOLVED_REGISTER.md:381`（被支撑的裁决）、`:385`（证明）、`:393`（前台复核 a）、`:416`（C2-d 残余 0.7%–5.2%）。 |
| **核证** | 转引。代数部分可纯算术复核，本次采信其代入过程。 |
| **影响** | 若不先回退，`:381` 会继续以「已裁决」的权威压住 U5/U6 的争议；`:385` 同时是 U6 反例 A 的「加固第二证据」来源之一。**1 个文件，但压着 2 条阻断**。 |
| **所需输入** | 负责人同意回退裁决状态；或先裁 U5 再据此重判。 |
| **建议** | **建议**先裁 U5、再按形别重判 `:381`，而不是现在就回退 —— 因为若最终选形 B，`:385` 可能是对的，回退反而制造二次欠账。**建议**的做法是给 `:381` 加「结论依赖 U5 口径裁决」的显式限定。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U8 · P5 定权：顶点 `w = 1/σ_F²` 还是正本 `quality × control_reliability × control_ivar`

| 字段 | 内容 |
|---|---|
| **事项** | 权威顶点规定的 P5 定权式没有指称对象，正本用的却是另一套，二者必须择一。 |
| **分歧** | **顶点方**：`docs/ACSD_DESIGN.md:156` 明写天光平面拟合与叠加定权用 `w = 1/σ_F²`；按 `ACSD_DESIGN.md:23`「本文档与任何其他文档冲突时以本文档为准」，顶点优先。**正本方**：`docs/science/PHASE2_UPM.md:80` 用 `w_UPM = quality × control_reliability × control_ivar`（背景中位数估计器的逆方差）。**红队给出四条独立反证**：① `σ_F` 是星点拟合域量，在**星点掩膜外的背景采样点上不存在**，且 `PHASE2_UPM_IMPL.md` 明写「不产 per-frame σ_F / 当前实现不产出」；② 正文明文禁 production 乘 star SNR；③ 量纲差 22 dex（`1/σ_F²` 是 ADU⁻²，全链权重面是 (ADU·sr⁻¹)⁻²）；④ P5 自己的三臂实验里 `control_ivar` 臂伪影漏入与噪声 RMS 均最小。 |
| **出处** | 源件：`审稿-RR07-P5 加性天光去除.md` §1 阻断-2（:40-50）、§3 BLK-2（:141）、§9.1（:489）。仓内：`docs/ACSD_DESIGN.md:156,:157,:23`、`docs/science/PHASE2_UPM.md:80,:123,:292,:384`、`docs/science/CONTROL_WEIGHT_SNR.md:44,:120`、`docs/science/UNCERTAINTY_AND_COVARIANCE.md:42-43,:60`、`eng` 侧 `docs/science/algorithms/UPM_SOLVER.md:17`、`docs/detail/algorithms/PHASE2_UPM_IMPL.md:167`（该路径已不存在，见 U17）。 |
| **核证** | ✔部分核证：`grep -rn "9\.73 A44"` 见 U21；量纲两处亲见（`UPM_SOLVER.md:15` 写 ADU⁻²、`PHASE2_UPM.md:35` 写 (ADU·sr⁻¹)⁻²）——此项源件标「我未独立复核 22 dex 的数字方向」，本件同：**22 dex 这个数字方向待核实**，但「量纲不一致」这个事实成立。 |
| **影响** | P5 创新点名称中的「**SNR 加权**」失据 —— 现行实现根本不是 SNR 加权。属「设计层裁决」，`审稿-RR07` 明写「不得由下层单方改」。影响：顶点 + 4 份一级/二级正本 + P5 单元全部判据。 |
| **所需输入** | 负责人在设计层裁定：改顶点 `:156`，还是给 `σ_F` 补定义域与产品。 |
| **建议** | **建议**改顶点 `:156` 为 `w_UPM = quality × control_reliability × control_ivar` 并把创新点名称里的「SNR 加权」一并订正 —— 理由是顶点式当前无指称对象（`σ_F` 不产出），一条无法实现的条款比一条不准确的条款更有害。**但这直接改动 `AGENTS.md` §10 的创新点表述，属发布口径面，必须负责人裁。** |
| **阻塞级别** | 阻断提交 |

### G08-05-U9 · 消费侧 gauge 与 P5 互斥：`INTEGRATION.md:78`「公共零底」vs P5 停在非零 `B_ref`

| 字段 | 内容 |
|---|---|
| **事项** | 叠加正本的冻结假设说帧已校准到公共零底，P5 的产品却停在非零 `B_ref`，两者对同一背景的处置相反。 |
| **分歧** | **零底方**：`docs/science/INTEGRATION.md:78`（SCI 假设，冻结）逐字「帧间像素已 UPM.calibrate_block 到公共零底」。**保留背景方**：`docs/science/PHASE2_UPM.md:379-381` 的 P5 产品停在 `B_ref`；同文件 `:474` 明写「产品若把背景扣到近 0…rel_step 无界、门失去意义 ⇒ **该门隐含前提 = 保留背景**」。 |
| **出处** | 源件：`审稿-RR07-P5 加性天光去除.md` §1 阻断-8（:86-90）、§6 S4 复核（:372）、§9.2（:490）。仓内：`docs/science/INTEGRATION.md:78`、`docs/science/PHASE2_UPM.md:379-381,:474`。 |
| **核证** | 转引（两处原文由源件前台逐字核对）。 |
| **影响** | 裁决前**叠加链不得声称帧已在零底**；若实现者信 `INTEGRATION.md:78`，会把 `B_ref` 当成要消掉的量，从而让整个接缝门失效。这是「创新点之间互相打架」，不是文档瑕疵。 |
| **所需输入** | 负责人裁定以哪一侧为准；若以 P5 为准，`INTEGRATION.md:78` 的冻结假设须改写并评估对 P2/P3 的连锁影响。 |
| **建议** | **建议**以 P5（保留背景）为准 —— 依据是 `PHASE2_UPM.md:474` 已给出「扣到近 0 则门失去意义」的技术论证，而 `INTEGRATION.md:78` 只是背景陈述、未给出反向论证。**建议**改 `INTEGRATION.md:78` 并加指针到 `PHASE2_UPM.md §16.2`。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U10 · `F_syn` 单位取 `nm` 还是 `nm⁻¹`

| 字段 | 内容 |
|---|---|
| **事项** | 数据正本同一节内两行互斥，测光正本又声明「与之一致」，该声明当前不成立。 |
| **分歧** | **`nm⁻¹`**：`docs/science/DATA_SEMANTICS.md:715`「`F_syn = W·m⁻²·nm⁻¹`」。**`nm`**：紧接的 `:718`「`F_syn` 的单位…（W·m⁻²·nm）」。**声明方**：`docs/science/PHOTOMETRY.md:74`「`F_syn | W·m⁻²·nm⁻¹ · nm · nm = W·m⁻²·nm | 与 §2 符号表、DATA_SEMANTICS.md §14.3 一致」——被声明为「一致」的条款自身两行互斥。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §3.1 B6（:207-218）、§2.4（:96）、§8.10（:585-591）。仓内：`docs/science/DATA_SEMANTICS.md:715,:718`、`docs/science/PHOTOMETRY.md:74`。 |
| **核证** | ✔已核证（文件存在性与行号）；物理量纲推导属科学裁决，本次**不下结论**。 |
| **影响** | 子代理复算指出 `location = log10(F_instr/F_syn)` 会差 3 个数量级 ⇒ `k_photo` 差 1000 倍 ⇒ `I_photo` 差 1000 倍 ⇒ 等价星等差 **2.5 mag**。该复算源件明写「本轮未亲自复算」，本件同：**2.5 mag 这个数字是转引，待核实**。 |
| **所需输入** | 负责人裁定取 `nm` 还是 `nm⁻¹`；并确认 `PHOTOMETRY.md:74` 的「一致」声明在修正前是否应加限定。 |
| **建议** | **建议**以**量纲自洽**为准由负责人裁定，不建议按「哪份文件先写」定。本件不代裁。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U11 · 生产面身份四方互斥（`orchestrator.exe` / `acsd-stage2.exe` 是否为生产入口）

| 字段 | 内容 |
|---|---|
| **事项** | 「哪个可执行文件是生产入口」在四份载体上给出互斥答案，同层冲突仓内无更高层可裁。 |
| **分歧** | **恰一 = acsd**：`docs/engineering/CLI_PROTOCOL_V1.md:97`「发布 manifest 不含 phase1\|2\|3 可执行文件…生产面 classification=production 的行恰一 = acsd」；`eng/tools/arch/gen_build_graph_doc.py:48-49` 写「非入口」。**生产入口 = stage2 工具**：`docs/engineering/PUBLIC_API.md:1081-1082`「定位: Phase2 马赛克写出当前无独立公共 C API——生产入口 = stage2 工具」；`eng/tools/gen_repo_source_manifest.py:57,:70` 支持该读法。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §3.1 B1（:125-136）、§4 R1（:259-273）、§8.1（:502-511）。仓内：`docs/engineering/CLI_PROTOCOL_V1.md:97`、`docs/engineering/PUBLIC_API.md:1081-1082`、`docs/engineering/ERROR_HANDLING_STANDARD.md:83`、`eng/tools/gen_repo_source_manifest.py:57,:70`、`eng/tools/arch/gen_build_graph_doc.py:48-49`。 |
| **核证** | ✔已核证：红队结论是「**两分支皆假的**不可能命题」——无论按哪一支采信，都有一处已登记事实落空（`审稿-RR09` §4 R1，`:259-273`）。四个文件的行号本次未逐条复算，标转引。 |
| **影响** | 争议点②的**前置裁决点**：`审稿-RR09` B2（退出码三方表）、S-1、S-2、S-10 的定级**全部**依赖它。若裁非生产，`PUBLIC_API.md:1081-1082` 必须改；若裁生产，`CLI_PROTOCOL_V1.md:97` 的「恰一 = acsd」与 `ERROR_HANDLING_STANDARD.md:83` 的唯一源声明须重排。 |
| **所需输入** | 负责人裁定生产入口的唯一身份；并授权连带改写范围。 |
| **建议** | **建议**先做**目标态裁定**（三个命令 `normalize` / `mosaic` / `export` 各自的生产入口是哪个），再回改两侧正本；不建议在现状描述上二选一，因为红队已证两分支皆假。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U12 · 三条科学冲突之一：P3 守恒门结构上无法判红（apex「通量绝对守恒」的无条件读法被推翻）

| 字段 | 内容 |
|---|---|
| **事项** | 主实验脚本算出真实的跨面 drop 计数后被无条件覆盖为 0，且分子剔除跨面 drop 而分母仍含全部像元，使该门结构上不可能判红。 |
| **分歧** | **门已覆盖域外缺口**（归档自述）：`实验/healpix-polar/results/` 归档 JSON 的 note 称该门已排除域外缺口。**门看不见域外缺口**（代码实测）：`exp_sim01_m16_forward_conservation.py:492` 算出真实计数，`:498` 隔 6 行、无条件、无注释地裸赋值覆盖为 0；主循环 `:568-570` 把跨面 drop 静默剔出分子，`:613-614` 的分母仍含全部像元通量 ⇒ `M1_closure_le_1e-12` 恒绿。归档 note 与代码行为**相反**是加重项。 |
| **出处** | 源件：`审稿-RR10-反例与边界.md` §3.1 阻断-1（:72-85）、§8.1（:355-373）、§4 CE-2。仓内：`实验/healpix-polar/code/audit/sim/exp_sim01_m16_forward_conservation.py:492,:498,:568-570,:613-614,:724-725,:727`、`实验/healpix-polar/REPORT_paper.md:240`、`docs/ACSD_DESIGN.md:136,:118`。 |
| **核证** | 转引（源件由审稿人本人逐项实测）。⚠ 该文件在本会话期间**处于已修改状态**（见文首偏差登记），行号可能漂移。 |
| **影响** | 推翻 `docs/ACSD_DESIGN.md:136/:118`「通量绝对守恒 / 构造闭合」的**无条件读法**。论文 `:240` 已声明的「同一叶内」域救不了此条 —— **主实验腿本身看不见域外缺口**；下游读该归档 JSON 者会被 note 误导。 |
| **所需输入** | 负责人裁定：(a) 修代码（保留真实计数并纳入分子/分母）还是修文档（apex `:136` 补域）；(b) 归档 JSON 是否重跑。 |
| **建议** | **建议**先修代码再改文档 —— 一个把真值写成 0 的变量会让该单元此后的任何判据都失去证据力。**但重跑归档需编译/执行，属前台统一执行项**（`AGENTS.md` §9）。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U13 · 三条科学冲突之二：apex 对 P5 的绝对断言，命中的判据本仓已登记为恒真门且现红

| 字段 | 内容 |
|---|---|
| **事项** | 权威顶点用「任意覆盖子集上的加权阶跃为零」这一绝对量词断言 P5 闭环，而本仓登记册把同一条性质判为近乎恒真、且当前由 PASS 转 FAIL。 |
| **分歧** | **断言成立**：按 `docs/ACSD_DESIGN.md:23`，顶点与其他文档冲突时以顶点为准。**断言已失效**：`docs/engineering/UNRESOLVED_REGISTER.md:171-174,:192` 把同一性质定义为 `A6_subset_invariance`，已判定**近乎恒真**、过去的通过记录不构成证据、且当前由 PASS 转 **FAIL**；P5 单元自己已把等价命题降级为**猜想**（`REPORT_paper.md:166,:199`）。 |
| **出处** | 源件：`审稿-RR10-反例与边界.md` §3.1 阻断-2（:89-99）、§8.3（:389-401）、§4 CE-1。仓内：`docs/ACSD_DESIGN.md:23,:339`、`docs/engineering/UNRESOLVED_REGISTER.md:171-174,:192`、`实验/additive-sky-samless/REPORT_paper.md:166,:199`、`docs/science/PHASE2_UPM.md §16.2`（apex 改写所引域）。 |
| **核证** | 转引。 |
| **影响** | 按 apex `:23` 的优先规则，**当前状态下被推翻的是裁决面本身**。须改 `ACSD_DESIGN.md:339` 补成立条件并删去绝对量词「任意」，同时登记 A6 当前零判别力。 |
| **所需输入** | 负责人裁定改顶点 `:339`；并决定 A6 的处置（补判别力 vs 降级为结构断言）。 |
| **建议** | **建议**按源件给出的写法改 apex `:339`，补成立条件「帧间天光差落在公共面表示空间内」并引 `docs/science/PHASE2_UPM.md §16.2`，同时删「任意」二字。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U14 · 三条科学冲突之三：apex 对 P2 的无条件单调性断言，登记状态仍停在「待裁决」

| 字段 | 内容 |
|---|---|
| **事项** | 顶点无条件写「天光越亮信噪比越低、趋于零」，而本仓已判该命题为阻断、结构主导时机制失效；限定只写在单元层，没上提到权威面。 |
| **分歧** | **无条件断言**：`docs/ACSD_DESIGN.md:126`。**已登记为阻断且失效域正是本仓真实数据的运行域**：`docs/engineering/UNRESOLVED_REGISTER.md:454-499`（SCI-P2-2），尤其 `:454,:456,:466,:473-480,:484`；单元层 `实验/absolute-snr/docs/frame-snr-canon.md:17,:215-217` 已加域并修好，**但限定没有上提到权威面，登记状态仍停在「待裁决」**。 |
| **出处** | 源件：`审稿-RR10-反例与边界.md` §3.1 阻断-4（:115-130）、§4 CE-1（:214-221）、§8.3。应改处：仓内 `docs/science/CONTROL_WEIGHT_SNR.md:20,:101`。 |
| **核证** | 转引。登记册 `:484` 给出的正确写法是「`σ̂` 估计器对天光单调」，并须带 `r ≳ 3` 的失效阈值。 |
| **影响** | 这是本轮最典型的「**限定写在一处、在权威面丢失**」形态。影响顶点 + P2 一级正本 + 登记册状态。下游读到的是**强度最高、正确性最低**的一份。 |
| **所需输入** | 负责人把 `UNRESOLVED_REGISTER.md:454-499` 的 SCI-P2-2 从「待裁决」推进到「已裁决」，并授权改 apex `:126` 与 `CONTROL_WEIGHT_SNR.md:20/:101`。 |
| **建议** | **建议**按登记册 `:484` 已给出的写法直接改 apex `:126` 与 `CONTROL_WEIGHT_SNR.md:20/:101`，并带 `r ≳ 3` 阈值。**这是本件中「最容易被裁且改动最确定」的一条** —— 依据已在本仓登记册内写好，只差状态推进与落盘。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U15 · `cone-search-constants` 实验单元：证据基整体不存在

| 字段 | 内容 |
|---|---|
| **事项** | 该单元自称有机器可读结果、可复现脚本与完整证据链，实际目录只有一个 md 文件，所引 JSON、脚本、正本章节全部不存在，HEAD 号也不符，且其「最小匹配星数 ≥50」的结论与正本逐字相反。 |
| **分歧** | **有证据链**：`实验/cone-search-constants/REPORT_paper.md:4,:5,:6,:7` 的自述。**无证据链**：目录清单实测只有 `REPORT_paper.md` 一个文件；5 个声称产物 `test -e` 全为 ABSENT；`grep -c` 锥搜为 0；`:260` 的结论与正本 `docs/science/PHOTOMETRY.md:381` 逐字相反。**另一独立轮次的证伪**：`审稿-RR02` BLK-02 独立发现同一单元 `:513` 把 A&A 674, A33 的题名写成「The Gaia XP calibration」，与仓内既有裁决 `实验/photometric-magnitude/refs.md:117`（XP 外定标是 A3、**不是 A33**）直接冲突；`:516` Huber 年份作 2011（2nd ed. = 2009）且漏合著者 Ronchetti。 |
| **出处** | 源件：`审稿-RR10-反例与边界.md` §3.1 阻断-5（:146-158）、§8.8（:453-468）；`审稿-RR02-引用真实性.md` §3.1 BLK-02（:152-158）、§9 第 1 项（:434）。仓内：`实验/cone-search-constants/REPORT_paper.md:4,:5,:6,:7,:260,:513,:516`、`docs/science/PHOTOMETRY.md:381`、`实验/photometric-magnitude/refs.md:117`。 |
| **核证** | ✔部分核证：`实验/cone-search-constants/` 目录实测**只有 `REPORT_paper.md`**（本次会话开始时目录清单确认）。其余 5 个产物的 `test -e` 由源件实测。题名真伪**需联网**（见 §四 待核验清单 N-01）。 |
| **影响** | 不是边界遗漏，是**证据不存在**。须整体打回重写：或补齐可核面（产物 JSON、脚本、数据），或把报告降级为设计稿并删去一切实测断言与正本引用。 |
| **所需输入** | 负责人决定打回重写还是补齐证据；补齐的话还需决定数据来源与是否入库。 |
| **建议** | **建议**打回重写。当前文本的每一条实测断言都没有可核面，补齐成本很可能高于重写；且在补齐前该单元不应进入任何论文工作区或验收证据。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U16 · P1 施加式是否含 `m(x,y)`：顶点同一份文件给出两个式子

| 字段 | 内容 |
|---|---|
| **事项** | `I_photo` 的施加式在顶点内部就有两版，正本各自跟一版，实测又判 `m` 分量未获验证。 |
| **分歧** | **无 m 式**：`docs/ACSD_DESIGN.md:113`（§2.1 P1 本体）`I_photo = k_photo·I_cal`；`docs/science/DATA_SEMANTICS.md:3160,:3170` 用前者（标度类别 `photo_scaled_adu`、零点=本帧相对测光零点）。**有 m 式**：`docs/ACSD_DESIGN.md:232`（§4.2）`I_photo = k_photo·m(x,y)·I_cal`；`docs/science/PHOTOMETRY.md:13,:334,:335,:345` 用后者（`:334` 步④ 把 `m(x,y)` 写成既定标定面）。**实测证据**：`实验/photometric-magnitude/docs/p1-spatial-gain.md:18,:26,:27` 判拟合空间结构随孔径变化（r=3/4/6/10 → 7.78%/4.93%/3.81%/4.25%），独立孔径 r=6 复核反而变差 ⇒ 孔径/PSF 系统差主导而非纯乘法增益；「逐帧绝对 `m` 幅度不可辨识」；「本轮不宣称真实数据已被校准」；C3、C5 两条判据登记 FAIL。 |
| **出处** | 源件：`审稿-RR03-P1 通量积分拟合.md` §3.1 B-02、§4 CE-3、§9（:377）、§7（:303）。仓内：`docs/ACSD_DESIGN.md:113,:232,:331,:97`、`docs/science/DATA_SEMANTICS.md:3160,:3170`、`docs/science/PHOTOMETRY.md:13,:334,:335,:345`、`docs/detail/PHASE1_DETAILED_DESIGN.md:51`。 |
| **核证** | ✔已核证：`docs/ACSD_DESIGN.md` 两行并存（`:113` 无 `m`、`:232` 有 `m`）。实测部分转引（未重跑实验）。 |
| **影响** | 标度口径无法冻结，正本之间无法收敛。裁「无 m」则 `PHOTOMETRY.md:334` 步④须降级为「设计意图、未验证」并标适用域；裁「有 m」则须先关闭 J7 失败与 C3/C5 两条 FAIL。`ACSD_DESIGN.md:97` 的 P1→P2 图示使影响外溢到 P2。 |
| **所需输入** | 负责人在两个式子间择一。 |
| **建议** | **建议**采纳**无 m 式**并把 `m(x,y)` 降级为「设计意图、真实数据有效性未获验证」—— 依据是 `p1-spatial-gain.md` 已给出反证（孔径复核变差说明不是纯乘法增益），且「有 m 式」当前无任何达标证据支撑。**但这会改动顶点 §2.1 的表述，属顶层设计面，必须负责人裁。** |
| **阻塞级别** | 阻断提交 |

### G08-05-U17 · `docs/detail/algorithms_phase1/` 整目录删除后：补回还是改指现存正本

| 字段 | 内容 |
|---|---|
| **事项** | 一个已删目录在 8 份正本里留下 22 处引用，是补回目录、还是在正本改指现存载体。 |
| **分歧** | **补回**：`docs/science/PHOTOMETRY.md:359,:374,:405` 四处承重引用（判据形态与逐项预算、模块算法与配置）与 `:302,:379` 两处（`RESOLUTION_fsyn_formula.md`，被 G08-04 的 686 行删除波及）都指向该目录；补回则 22 处引用一并复活。**改指**：改指现存正本，须处理 `docs/detail/registry/acsd.phase1.photometry.md:80` ↔ `PHOTOMETRY.md:366` 的**引用环**，以及 `RESOLUTION_fsyn_formula.md` 里数字（26211 星 / median −0.0037 mag / MAD 0.0033 mag，现仅见于 `fsyn_convention.md:102-108`）的落位。 |
| **出处** | 源件：`审稿-RR03-P1 通量积分拟合.md` §3.1 B-01、§9（:378）。仓内：`docs/detail/algorithms_phase1/`（不存在）、`docs/science/PHOTOMETRY.md:302,:359,:366,:374,:379,:405`、`docs/detail/registry/acsd.phase1.photometry.md:80`、`docs/engineering/UNRESOLVED_REGISTER.md:1246`。 |
| **核证** | ✔已核证：`docs/detail/algorithms_phase1/` 目录**不存在**；`docs/detail/` 下现存 `anchors/`、`infrastructure/`、`registry/` 三个子目录，无 `algorithms_phase1/`。另核：R06 阻断-3 引的 `docs/detail/algorithms_phase1/07_noise_snr.md` 亦不存在（`git log --all --diff-filter=D` 显示由提交 `ac058121` 删除 9 个文件）。 |
| **影响** | P1 的四条承重证据链在正本内部即断裂（判据形态、逐项误差预算、模块算法、F_syn 绝对刻度均无可追溯来源）。波及 P2/P4：`docs/science/CONTROL_WEIGHT_SNR.md` 5 处、`docs/science/NOISE_MODEL.md` 1 处（`审稿-RR03` §7 :301 与 `审稿-RR06` §7 :231 均把清点责任转给结构轮）。**8 份正本 22 处引用**。 |
| **所需输入** | 负责人裁定去留方案；若改指，须确认那三个数字的落位文件。 |
| **建议** | **建议改指而非补回** —— `docs/detail/registry/` 已按 AGENTS.md §5 的极简目录卡范式承载同批内容，另起一个 `algorithms_phase1/` 会与 `registry/` 产生第二套模块卡体系（直接踩 U18）。**但引用环须先解开**，否则改指会形成环。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U18 · 模块 ID 计数与命名代际分歧：23 / 25 / 26 三个数 + 两套命名零交集

| 字段 | 内容 |
|---|---|
| **事项** | 模块数在同一仓内有三个值，且两处载体用**两套互不相同的模块 ID 命名**、交集为 0。 |
| **分歧** | **23**：`docs/modules/MODULE_MAP.yaml:3`「把 docs/plugins/ **23** 篇插件规范…建立一一机器映射」、`:8`「`00_INDEX.md` §2（**23** 篇模块总表）」、`:20`「23/23 条已核对」、`:226`「迁移 map 条目 23」；机器登记面 `docs/modules/registry/module_id_migration_baseline.json` 的 `index_module_count = 23`，其 rule 明写「门断言 `docs/detail/00_INDEX.md` §2 解析出的模块数 == 本值」。**26**：`docs/detail/00_INDEX.md:22`「生产模块登记正本（**26 张卡** + README）」、`:42` 标题「`registry/` 模块卡（**26**）」。**25**：`00_INDEX.md` :42 的表格实列 11+9+5 = **25** 个模块，磁盘 `docs/detail/registry/*.md` 实测 **25 张卡 + 1 README = 26 个 .md**。**更重的一层**：两套 ID 命名代际漂移 —— `docs/detail/registry/` 用 `acsd.phase1.*` / `acsd.phase2.*` / `acsd.phase3.*`，`docs/modules/MODULE_MAP.yaml` 用 `acsd.p1.*` / `acsd.infra.*`，**两边零交集**。 |
| **出处** | 源件：`G08-01-整改执行R2.md` §7 U3（:477 行表内第 3 行）；`审稿-RR01-结构与索引.md` §3 须修 M-10、M-4（:188-192、:154-157）；`G08-01-整改执行.md` §6.2 第 3 项（:216 行表内）。仓内：`docs/detail/00_INDEX.md:22,:42`、`docs/modules/MODULE_MAP.yaml:3,:8,:20,:226`、`docs/modules/registry/module_id_migration_baseline.json`、`docs/engineering/MODULE_MAP.md:8`。 |
| **核证** | ✔已核证（本条为本次独立复算）：`MODULE_MAP.yaml` 逐块解析得 **`- id:` 条目 26 个**（`calibration` … `RELEASE-04/未覆盖`），其中 **`module_id:` 去重后仅 23 个**；3 个无 `module_id` 的条目是 `BLD-401`(:758)、`DOC-403`(:760)、`RELEASE-04/未覆盖`(:762) —— 属例外/欠账记录而非模块。故「23 个模块」本身站得住，「26」是把例外行一并计入。`docs/detail/registry/*.md` 实测 **25 张卡**（+README）。三者口径各不相同。 |
| **影响** | `baseline` 的门规则要求 `00_INDEX.md` 解析出的模块数 **== 23**，而该文件 :42 写 26、表格列 25 ⇒ **按现行条款该门必然判红**（若被重建执行）。两套命名零交集意味着 `set(map_ids) == set(index_ids)` 这条 rule 的另一半也必然失败。历史读数另有分歧：`G08-01-整改执行R2` U3 记录复核方给 11/20、交集 5，执行方实测 18/23、交集 0，**两说至今并存未裁**。 |
| **所需输入** | 负责人裁定：(a) 「模块数」的权威口径是「模块条目」还是「`- id:` 行」；(b) ID 命名以 `acsd.phaseN.*` 还是 `acsd.pN.*` 为准、另一套如何迁移；(c) `G08-01` U3 的两组数字以哪一方为准。 |
| **建议** | **建议**把口径写成**可判定的定义**而不是一个裸数字：明确「模块 = 具 `module_id` 的条目」，并在 `MODULE_MAP.yaml` 的例外行上加显式标注（如 `record_type: exception`），这样 23 与 26 不再互相冒充。命名代际**建议**统一到 `acsd.phaseN.*`（它与 `docs/detail/registry/` 的 25 张卡一一对应，覆盖更全）。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U19 · `VERSION` 本轮确认值（并含 alpha 标记时序）

| 字段 | 内容 |
|---|---|
| **事项** | 当前 `VERSION` 内容为 `0.1.0-alpha.1`，是 2026-09-24 的历史值，本轮**没有**「负责人当次确认」的记录；另需裁 alpha 标记是否时序提前。 |
| **分歧** | **口径层面已核对一致**：`VERSION` 与最高设计 §13 的口径无冲突。**无当次确认**：`G08-02-正本核验` 任务书步骤 3 写「版本具体值由**负责人当次确认**」，而全轮无此记录；按纪律核验方不擅自改 `VERSION`、也不改正本。**附注（非缺陷）**：`VERSION` 含 `alpha` 标记，而 §13:556 与 `AGENTS.md:130` 规定「负责人目视认可后执行发布收口，**标记** alpha 预览版」；版本串里的 `-alpha.1` 是**预发布版本号**、不等于发布状态声明（发布状态由 §12.5 状态阶梯与 manifest `available` 承载）⇒ 二者**不构成矛盾**。但若负责人本意是「alpha 标记应在收口时才写入」，则当前存在**时序提前**。 |
| **出处** | 源件：`G08-02-正本核验.md` §4.3（:149-160）、§10「U1 待裁决项复核」（:380）。仓内：`VERSION`、`docs/ACSD_DESIGN.md:556`、`AGENTS.md:130`。 |
| **核证** | ✔已核证：`cat VERSION` = **`0.1.0-alpha.1`**；`AGENTS.md` 共 130 行，`:130` 为发布收口条款；`AGENTS.md:21,:42` 写 `docs/ACSD_DESIGN.md`。 |
| **影响** | 按 `AGENTS.md` §11「版本号以仓库根 `VERSION` 为唯一来源，CLI `--version` 与产品 manifest 由其派生」，值本身不裁则 CLI 与 manifest 都不动。影响面小但**属发布口径**，且 alpha 时序问题若被裁为「提前」会牵动 `RELEASE_STATUS.md` 与 manifest `available` 的叙述。 |
| **所需输入** | 负责人当次确认版本值仍为 `0.1.0-alpha.1` 或给新值；并一并裁 alpha 标记是否应在收口时才写。 |
| **建议** | **建议**当次确认保留 `0.1.0-alpha.1`，并把「预发布版本号 ≠ 发布状态声明」这句写进 `docs/engineering/VERSIONING.md`，一次性消掉这个反复出现的疑问。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U20 · 门禁注册面「G08-10 重建」占位符在 `docs/` 下的分布与重建范围

| 字段 | 内容 |
|---|---|
| **事项** | `docs/` 下有 45 个文件带「G08-10 重建」占位符；G08-01 删除了 CI 执行器本身，而「重建 CI」是 G08-10 的任务 —— 重建范围（只重建执行器，还是执行器 + 检查器集合）至今未裁。 |
| **分歧** | **只重建执行器**：`docs/engineering/UNRESOLVED_REGISTER.md:3130`「G08-10 恢复执行器的优先级应高于修判据」；`:3007` 把「重建执行器，还是重建执行器 + 恢复检查器集合」列为**必须先决定**的前置。**执行器 + 检查器集合**：`:1904-1908`「G08-10 **必须**重建该门，否则下一次改名会重演同样的漏改」；`:3939-3947` 给出「152 个被删检查器的覆盖对照表」，其中 **3 无人管 / 16 登记为 G08-10 必须补的判据**；`:1971`「凡涉及节号锚的订正必须以 `docs/ACSD_DESIGN.md` 为准重新核对」。 |
| **出处** | 源件：`G08-01-整改执行.md` §6.2 第 6 项（:216 行表内）、§8；`G08-01-整改执行R2.md` §6.1 #2、§7 U4。仓内：`docs/engineering/UNRESOLVED_REGISTER.md:3007,:3130,:1904-1908,:3939-3947,:1971`、`:1357`、`:3413`。 |
| **核证** | ✔已核证：`grep -rc "G08-10" docs/ \| grep -v ":0$"` = **45 个文件**、合计 111 处。分布：`docs/engineering/UNRESOLVED_REGISTER.md` 20 处（单文件最多）、`docs/architecture/doc_symbol_namespaces.json` 16、`docs/engineering/STANDARDS_REGISTRY.md` 11、`docs/engineering/FROZEN_GATE_INVENTORY.md` 7、`docs/engineering/LOG_AND_ERROR_CONTRACT.md` 6，其余 39 个文件各 1–5 处。 |
| **影响** | 占位符集中在**一级正本**里，意味着 `docs/` 现在有一批条款把「现行判据」寄托在一个**尚未被派单**的未来任务上。重建范围不裁，G08-10 无法开工；同时 §6.3 的 4 条预存在悬空是否并入 G08-10 也悬着（`G08-01-整改执行R2` U4）。 |
| **所需输入** | 负责人裁定 G08-10 的重建范围；并裁 §6.3 的 4 条预存在悬空是并入 G08-10 还是单独开单。 |
| **建议** | **建议**裁「执行器 + 检查器集合」，但**分两批**：第一批只恢复执行器与 16 条「无人管的判据」，第二批再按覆盖对照表逐条恢复 152 个检查器（否则一次性重建必然重演「表内自证绿」）。同时建议 G08-10 开工前先把 `FROZEN_GATE_INVENTORY.md` 的 7 处占位符与 `UNRESOLVED_REGISTER.md:3939-3947` 的对照表对齐。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U21 · `§9.73 A44` 在 8 个文件里被引用 27 次，而该节号在顶点中不存在

| 字段 | 内容 |
|---|---|
| **事项** | 多个一级正本与机器登记面用「已按 §9.73 A44 作废」标注退役键，但 `docs/ACSD_DESIGN.md` 里根本没有 §9.73，且每处都带 `AGENTS.md` §5 禁用的历史叙事词「作废」。 |
| **分歧** | **标注有效**：`docs/engineering/UNIFIED_OBJECTS.md:5`、`PUBLIC_API.md`、`CONFIG_SCHEMA.md`、`CONFIG_CONTRACT.md` 都在用这个标注说明 `weight_mode` 已不存在。**标注不可核**：`grep -c "9\.73" docs/ACSD_DESIGN.md` = **0**，`grep -n "A44" docs/ACSD_DESIGN.md` = **0**；顶点实际只有 §0–§13，`AGENTS.md` §5 明禁「作废」类历史叙事词。`docs/contracts/unified_object_registry.json:561` 的 `basis` 字段也引 `§9.73 A44`。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §7 交叉表第 4 行（:391）；`G08-03-书写形态体检.md` §7 U2（:465 行表内）。仓内：`docs/engineering/PUBLIC_API.md:1125,:2023,:2030,:2037`、`docs/engineering/CONFIG_SCHEMA.md:35,:58`、`docs/engineering/UNIFIED_OBJECTS.md:5`、`docs/engineering/CONFIG_CONTRACT.md:82`、`docs/science/DATA_SEMANTICS.md`（5 处）、`docs/science/PHOTOMETRY_RESEARCH_PACK.md`（1 处）、`eng/contracts/data/clause_registry.json`（12 处）、`docs/contracts/unified_object_registry.json:561`。 |
| **核证** | ✔已核证：`grep -rn "9\.73 A44" docs/ eng/contracts/ \| grep -v UNRESOLVED_REGISTER` = **27 处 / 8 个文件**；按文件：`eng/contracts/data/clause_registry.json` 12、`docs/science/DATA_SEMANTICS.md` 5、`docs/engineering/PUBLIC_API.md` 4、`docs/engineering/CONFIG_SCHEMA.md` 2、`docs/engineering/UNIFIED_OBJECTS.md` 1、`docs/engineering/CONFIG_CONTRACT.md` 1、`docs/contracts/unified_object_registry.json` 1、`docs/science/PHOTOMETRY_RESEARCH_PACK.md` 1。`grep -c "9\.73" docs/ACSD_DESIGN.md` = 0。 |
| **影响** | 27 处「指向不存在条款的引用」+ 27 处禁用历史叙事词，横跨一级正本（4 份）、科学正本（2 份）与机器登记面（2 份）。**一处失效标注被复制 27 遍**是典型的单点错误放大。 |
| **所需输入** | 负责人确认 `§9.73 A44` 的真实来源（是否存在但已被改名/移入附录），或确认应删。 |
| **建议** | **建议**直接删除全部 27 处括注 —— 依 `AGENTS.md` §5「文档只写现行设计」，「键不存在」这件事应由**读 schema** 得出，不需要历史标注；若确需留痕，应登记到 `UNRESOLVED_REGISTER.md` 而不是写进 8 份正本正文。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U22 · `GAIN` 常量取倒数时 10 条门全绿（P4）

| 字段 | 内容 |
|---|---|
| **事项** | 把 `GAIN` 量纲颠倒（取倒数）后，10 条新 gate **全部保持绿** —— 说明这批门对最基本的一类缺陷零判别力。 |
| **分歧** | **门有判别力**：`实验/dense-snr-reconstruct/code/route1/exp_p4_04_brightness_forward.py` 的 10 条 gate 是 G08-04 新建的。**门无判别力**：`审稿-RR08` 反例 R1 给出源码级证明 —— 3 条**可证必绿**，7 条为**强预测为绿**（未实跑，因 `AGENTS.md` §9）。`审稿-RR06` 反例 R1 从另一路径给出定量解释：E 幅度门对 `GAIN` **不是完全盲**，而是二阶小量 —— 实测 `E_eff ≈ 0.017`，对照红臂 0.6335 / 0.3137 / 0.2905，即它**几乎看不见**量纲颠倒。 |
| **出处** | 源件：`审稿-RR08-实验与复现.md` §4 反例 R1（:277-303）、§9 UNRESOLVED-2（:480）；`审稿-RR06-P4 重建稠密信噪比.md` §4 反例 R1（:144-165）、§8 S-13（:315-330）。仓内：`实验/dense-snr-reconstruct/code/route1/exp_p4_04_brightness_forward.py:364,:368,:370,:372,:373,:376,:380,:387,:391,:400`。 |
| **核证** | 转引（`审稿-RR08` 明写 7 条为强预测、未实跑；`审稿-RR06` 的 `E_eff ≈ 0.017` 为其盲复算实测）。⚠ 该文件在本会话期间**处于已修改状态**（见文首偏差登记）。 |
| **影响** | 与 U27 是同一根因的两面：**绝对尺度无门 + E 度量齐次盲区** ⇒ 量纲错误在整个 P4 判据面上零防护。**7 条「预测绿」尚未实跑**，按 `AGENTS.md` §9 属前台统一执行项。 |
| **所需输入** | 负责人给出处置方向；并决定是否由前台实跑验证那 7 条。 |
| **建议** | **建议**先实跑验证 7 条预测（成本低、结论确定），再决定是补门还是降格判据。**建议**把「量纲颠倒」作为**永久负例**固化进该单元 —— 这类缺陷一旦漏过一次就会复发。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U23 · `E` 度量对 `sw2_map` 的齐次盲区，与 dex 配对判据无阈值

| 字段 | 内容 |
|---|---|
| **事项** | E 效率度量对 `sw2_map` 的**全局乘性缩放严格不变**（齐次），单元自陈这是已知盲区并要求「必须与 dex 水平判据成对使用」，但全仓 dex 读数**没有任何数值阈值**，配对要求落空。 |
| **分歧** | **盲区是已知且已配对**：单元在 `code/fix/fix01_metric_E_and_gates.py:397-399` 原文写「E 对 v 齐次、且 `E_equal` 对 `v→1/v` 不变，故『全局缩放』『量纲颠倒』在 oracle 段原理上不可观测；此条按物理式独立重算 `sw2_map` 来抓这两类缺陷」，并在 `:400` 定义了补救门 `variance_map_matches_physical_model`。**补救门缺席存档**：现行脚本写 10 项 gates，**存档只有 7 项**、13 个顶层键，且仍含已撤回的恒真门 `oracle_full_is_optimal: true`；`variance_map_matches_physical_model` **不在存档里** ⇒ 按报告引用的存档复现时完全没有这层保护。同时 dex-RMSE 只被测量（`REPORT_experiment.md:91` 的 14.06/14.08/66.74），**无处阈值**。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3 阻断-1（:56-60）、§4 反例 R1/R2、§8 S-2；`审稿-RR08` §3 S-5。仓内：`实验/dense-snr-reconstruct/code/route1/exp_p4_04_brightness_forward.py:123-128`（`efficiency` 定义）、`:131-147`（`arm_metrics`）、`:397-400`、`实验/dense-snr-reconstruct/results/route1/exp_p4_04_brightness_forward.json`、`实验/dense-snr-reconstruct/REPORT_experiment.md:5,:33,:91,:123`、`docs/derivations.md:76-78`。 |
| **核证** | ✔已核证（代码层，本次亲自读）：`exp_p4_04_brightness_forward.py:123-128` 的 `efficiency(w_used, v_true, eligible)` 计算 `var_w = Σ(w²v)/(Σw)²`、`var_opt = 1/Σ(1/v)`、返回 `var_w/var_opt − 1`。**`v_true` 整体乘 λ 时，`var_w` 与 `var_opt` 同乘 λ ⇒ 比值不变** ⇒ E 对 `sw2_map` 的绝对尺度零敏感；且 `w ∝ 1/v`（即权重形式正确）时 E ≡ 0，**与 `v` 的绝对量级无关**。这就是「齐次盲区」的精确机理，与源件结论一致。`:138` 处 `v_true = sw2_map`，`:139` `w = (rec / FREF)**2`。 |
| **影响** | 「全局缩放」「量纲颠倒」两类缺陷在按存档复现时**零防护**（U22 的机理）。处置须三件事同时做：给绝对尺度做**有数值阈值**的门并登记进冻结门表；该门必须出现在**被报告引用的存档**里；dex 配对判据要么给阈值、要么从「必须成对使用」**降格为建议**。 |
| **所需输入** | 负责人裁 dex 配对判据的去向（给阈值 / 降格）；并决定绝对尺度门登记到哪张表。 |
| **建议** | **建议**给 dex 一个**有物理含义的阈值**（如 `dex-RMSE < 0.05`）而不是降格为建议 —— 理由是 E 的盲区是**结构性的**（0 次齐次，不是「量小」），靠另一个有阈值的量来兜是唯一可行路径；降格为建议等于承认 P4 的绝对尺度永远无门。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U24 · `results/` 归档 JSON 与代码判据集不同步（P4 exp04 为最重）

| 字段 | 内容 |
|---|---|
| **事项** | 多个单元的归档 JSON 与现行脚本的门集合不一致，读者按报告引用的存档复现会得到**相反**的判别力结论。 |
| **分歧** | **一致**：`实验/dense-snr-reconstruct/REPORT_experiment.md:5` 声称「19/19 JSON 除 `runtime_s` 外全同」。**不一致**（P4 exp04 最重）：脚本写 **10 项** gates 并另写 4 个存档没有的顶层键；存档只有 **7 项** gates、13 个顶层键，且仍含**已被撤回**的恒真门 `oracle_full_is_optimal: true`（脚本注释与 `gates_note` 已明写它是代数恒等式、恒真无判别力、已撤下）。`review` R2 反例证实 P3 归档 `p1_t12.out` 早于代码修复、SUMMARY 结论相反；R02 须修表另记 7 个孤儿产物被快照背书、P4 归档 7 条 vs 代码 10 条、P5 缺陷同环节。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3 阻断-2（:62-66）、§2 表第 8 行（:36）、§5 第 7 行（:200）、§8 S-1（:252-259）；`审稿-RR08-实验与复现.md` §1 须修 S-1（:59-83）、§4 反例 R2（:303-308）、§8 #3/#6/#7（:426-451）。仓内：`实验/dense-snr-reconstruct/code/route1/exp_p4_04_brightness_forward.py:296-408`、`实验/dense-snr-reconstruct/results/route1/exp_p4_04_brightness_forward.json`、`实验/dense-snr-reconstruct/REPORT_experiment.md:5`、`实验/healpix-polar/results/p1_t12.out`。 |
| **核证** | 转引。⚠ `exp_p4_04_brightness_forward.py` 在本会话期间**处于已修改状态**（见文首偏差登记），脚本行号与门数可能已变 —— **本条的当前门数须在处置前重测**。 |
| **影响** | 报告与归档是验收证据面。「19/19 全同」这类断言一旦不成立，**所有基于归档的复现结论都要重做**。至少涉及 P3、P4、P5 三个单元 + P1 的 `step9_collect.py`（U22）。 |
| **所需输入** | 负责人裁定：(a) 是否重跑全部受影响的归档；(b) 在重跑前，`REPORT_experiment.md:5` 的「全同」断言是否加例外注。 |
| **建议** | **建议**立即给 `REPORT_experiment.md:5` 加例外注（这是**只读可做**的、成本最低的一步），然后由前台统一重跑全部受影响归档 —— 重跑需编译/执行，属 `AGENTS.md` §9 的前台统一执行项。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U25 · 三个单元复现入口因缺 `testdata/HST_M16` 在干净 clone 上硬失败

| 字段 | 内容 |
|---|---|
| **事项** | 三个实验单元的一键复现入口依赖 `testdata/HST_M16/`，而该目录被 `.gitignore` 排除 ⇒ 干净 clone 上入口**确定性失败**。 |
| **分歧** | **不设 waiver 是有意的**：`实验/additive-sky-seamless/code/run_all.sh:56-57` 逐字写「注：V6 与 V10 读真实模板 `testdata/HST_M16/`，该目录被 .gitignore 排除；模板缺失时 V6/V10 **记红（不静默跳过、不设 waiver 开关）**—— 需先备齐数据再跑本入口」。**这正是问题**：诚实的失败被当成了可接受的现状，于是三个单元的「一键复现」承诺在干净 clone 上不成立。另有 P2 的 `run_all.sh:11,:13,:32` 需联网 + 编译 + ctest（`.gitignore` 排除 `build/` 与 `testdata/*`）、P1 的 `step0_fetch_refs.sh` 是唯一联网步且破坏 `results/` 无备份。 |
| **出处** | 源件：`审稿-RR08-实验与复现.md` §1 阻断 B-2、§2 C-05/C-06（:91-92）、§4 反例 R3（:308-315）。仓内：`实验/additive-sky-seamless/code/run_all.sh:56-57`、`实验/additive-sky-seamless/code/sci_c_common.py:63`、`实验/additive-sky-seamless/code/reverse_verify/m16_scene/exp_realbase_consistency.py:105`、`实验/photometric-magnitude/code/step2_hst_sim.py:29`、`实验/photometric-magnitude/code/step3_forward_vs_photacam.py:34`、`实验/absolute-snr/code/run_all.sh:11,:13,:32`、`.gitignore:41,:167`、`testdata/README.md:19`。 |
| **核证** | ✔已核证：`git check-ignore -v testdata/HST_M16` → **`.gitignore:167:testdata/HST_M16/`**（确认被排除）；`.gitignore:41` 为 `testdata/*`；`testdata/README.md:19` 自记「`HST_M16/*.fits`（3 × 268,917,120 B = 770 MB）| ❌ 不入库 | 体积超限，公开 HLSP 可重新下载」—— 即**体积是排除原因，且有公开来源可重新下载**。✔另核：至少 8 个仓内文件引用 `testdata/HST_M16/`（P5 的 `run_all.sh`、`sci_c_common.py`、`m16_scene/exp_realbase_consistency.py`，P1 的 `step2_hst_sim.py`、`step3_forward_vs_photacam.py`，以及 P2 的 EXP-01/03/05/06 文档）。 |
| **影响** | **「一键复现」承诺在这三个单元上不成立**，且失败点在**编译之前**，因此与 CI/门禁重建（G08-10）无关、不可能被门禁兜住。这是可复现性面上的**硬阻断**，且负责人若不知道就无法验收任何 P1/P2/P5 读数。 |
| **所需输入** | 负责人裁定：(a) 提供数据获取步骤（HLSP 下载脚本 + 校验）并写进 `testdata/README.md`；(b) 或把这些依赖真实模板的腿显式标为「需外部数据、不在一键复现承诺内」；(c) 或入库最小裁剪子集（770 MB → 可裁剪）。 |
| **建议** | **建议**(a)+(c) 组合：给 `testdata/README.md` 加一段确定性的 HLSP 下载 + SHA256 校验脚本（它已有 `git check-ignore -q` 的自检范式可照抄），并入库一个**够 P5 V6/V10 与 P2 B 臂用的裁剪子集**（1024² 裁剪已在 P2 文档里用过，见 `实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:282`）。**不建议**改为 waiver 或静默跳过 —— 那会让「无接缝」这类结论失去可核面。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U26 · `m42-realdata` 单元完全不可复现，且其输入在本机也不存在

| 字段 | 内容 |
|---|---|
| **事项** | 该单元唯一输入未被跟踪、被 `.gitignore` 排除、当前工作树上根本不存在、且仓内无任何代码产出它；`results/` 里的四个 JSON 是无输入的孤儿证据。 |
| **分歧** | **可复现**：`实验/m42-realdata/code/run_all.sh:3` 自称「只读既有端到端产物、不重跑三命令」。**不可复现**：`m42_common.py:34` 的 `VIS = ROOT / "run" / "RELEASE-05" / "vis"` —— `git ls-files "run/RELEASE-05" | wc -l` = **0**（未跟踪）；`ls -d run/RELEASE-05/vis` → **不存在**；`git check-ignore -v run/RELEASE-05/vis/out` → **`.gitignore:17: run/*`**；仓内无任何代码产出该路径。另 `实验/README.md:10` 自定义 `run/` 为「一次性运行产物」、`:11` 又有「自包含条款」，两条互相拉扯。 |
| **出处** | 源件：`审稿-RR08-实验与复现.md` §1 阻断 B-2（:38-57）、§8 #2（:420-425）。仓内：`实验/m42-realdata/code/m42_common.py:34`、`实验/m42-realdata/code/run_all.sh:3`、`.gitignore:17`、`实验/README.md:10,:11`。 |
| **核证** | ✔已核证（三条只读命令）：`git -c core.quotepath=false ls-files "run/RELEASE-05" \| wc -l` = 0；`git check-ignore -v run/RELEASE-05/vis/out` = `.gitignore:17` 命中 `run/*`；目录 `run/RELEASE-05/` 在本 checkout 不存在。 |
| **影响** | 该单元的 c1–c4 四条腿在任何 clone 上都必然失败（**包括审稿人自己的工作树**）；`results/` 里四个 JSON 是孤儿证据，**不可作为任何验收依据**。与 U25 是同一族但更重：U25 有公开来源可备齐，U26 **无任何获取途径** ⇒ 非联网可解。 |
| **所需输入** | 负责人三选一：入库最小输入子集 / 提供产出脚本 / 在单元边界显式登记「不可复现」并把四个孤儿 JSON 从证据面撤下。 |
| **建议** | **建议**第三种（显式登记不可复现 + 撤下孤儿 JSON）—— 入库需要重新产出一次 V5 端到端运行，产出脚本则等于重建 G08-01 删掉的那条链。无论选哪种，**现状（claims 有、evidence 无）必须先停止**。 |
| **阻塞级别** | 阻断提交 |

### G08-05-U27 · `AstroCsExitCode` 是第三套进程退出码表，9/11 语义相反，且真从 `main()` 返回

| 字段 | 内容 |
|---|---|
| **事项** | 代码里有三套互不相同的进程退出码表，其中两套在 9 个码值上语义相反，而冲突的那套真从 `main()` 返回。 |
| **分歧** | **`acsd::ExitCode`（正本）**：`eng/contracts/data/exit_codes.h:8-18`。**`AstroCsExitCode`（实现）**：`lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:143-166`，与正本在 9 个码值上**语义相反**（2 正本「CLI 参数或配置错误」vs 实现 `DLL_LOAD_FAILED`；7 正本 `IO` vs 实现 `CONFIG_ERROR`；8 正本 `INTEGRITY` vs 实现 `FILE_IO_ERROR`；10 正本 `RESOURCE` vs 实现 `CANCELLED`）；同文件 `:196` 的 `is_process_exit_code` 闸、`:391` `main()` 的返回处均在用。**唯一在位的机器门抓不到**：`is_frozen_exit_code_v1()` 只校验 final 事件的 `exit_code` 值域，而本条与 S-4 都不产生 final 事件。**第三方**：`acs-stage2` 裸整数退出码与 `acsd::ExitCode` 码值重叠（根因在 `lib/algorithms/coverage/tools/stage2.cpp`）。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §3.1 B2、§4 R5（:342-354）、§8.2（:513-521）；`G08-03-整改B-正本唯一性.md` §6 第 7 项（:241 行表内）。仓内：`lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:143-166,:196`、`lib/infrastructure/pipeline/orchestrator/cpp/src/main.cpp:391`、`eng/contracts/data/exit_codes.h:8-18`、`docs/engineering/ERROR_HANDLING_STANDARD.md:83,:124`、`lib/algorithms/coverage/tools/stage2.cpp`。 |
| **核证** | ✔已核证：`ERROR_HANDLING_STANDARD.md:124` 声称的官方机器判据 `docs_machine_consistency.py` **抓不到本条**（`审稿-RR09` §4 R5 证伪成功）—— 这一点已核。前台注：`docs_machine_consistency.py` 本身是否还在位需另核（见 U30）。 |
| **影响** | 分级随 U11（生产面身份）联动：若 U11 判非生产则降为须修，但 U11 本身已是阻断。**且仓内唯一的机器门结构上抓不到它** ⇒ 在 G08-10 重建 CI 前，这条**没有任何自动防护**。 |
| **所需输入** | 先裁 U11（生产面身份），再定本条的严重度与处置面；并决定是否给「不产生 final 事件的返回路径」补一条机器判据。 |
| **建议** | **建议**先裁 U11。**建议**在 G08-10 的门清单里显式补一条「所有 `main()`/进程返回路径的码值必须落在 `acsd::ExitCode` 值域内」的结构判据 —— 这是唯一能覆盖本条的机器面。 |
| **阻塞级别** | 阻断提交 |

---

## 二、阻断下一任务（16 条）

### G08-05-U28 · 「重复键」判据应不应该做成机器门（与「文档域不设机器门」正面冲突）

| 字段 | 内容 |
|---|---|
| **事项** | `docs/DOCUMENT_INDEX.yaml` 的重复键缺陷只有机器判据能稳定抓住，但现行条款禁止在文档域设机器门。 |
| **分歧** | **禁止机器门**：`docs/engineering/DOCUMENT_GOVERNANCE.md:6` 逐字「文档域**不设机器门**…机器面若出现同义判据视为重复建设」；`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:418-426` §12.5 同义并加严（「谓词形式不构成登记为机器门禁的资格」「不规定任何文档域的机器门」）。**需要机器判据**：重复键缺陷已定位到具体提交 `38b3353a`，症状可精确复现（8 个重复键 @ 2 处孤儿块 `:422-425` / `:624-627`），对抗性审查的目视检查漏检率高。 |
| **出处** | 源件：`G08-03-结构侦察.md` §10 U6；`G08-03-整改A-索引与可追溯.md` §5.3「与『文档域不设机器门』的正面冲突（待裁决项 U6）」（:237）。仓内：`docs/engineering/DOCUMENT_GOVERNANCE.md:6`、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:418-426`、`docs/DOCUMENT_INDEX.yaml:422-425,:624-627`。 |
| **核证** | ✔已核证：`VALIDATION_EVIDENCE_STANDARD.md:420` 逐字「文档域不设机器门。文档域指四个面：文档一致性、文档写法、文档索引、锚存活」；`:423`、`:426` 同向。`DOCUMENT_GOVERNANCE.md:6` 与之**已经一致**（抬头已改过，见 U30），但重复键判据的归属仍未裁。 |
| **影响** | 重复键会导致索引条目静默覆盖，是**判据无牙**的一种。若不建门，只能靠每轮对抗性审查目视 —— 而本轮十轮里就有多轮漏检同类问题。 |
| **所需输入** | 负责人裁：文档索引面是否允许一个**只做结构校验**（不判内容）的机器门。 |
| **建议** | **建议**允许，但**严格限定为结构门**：只校验「同一 `id` / `upstream` / `path` 键在一个块内不重复」，不判定任何内容语义。这样不违反 §12.5 的「文档域不设**内容**机器门」的本意，且能覆盖提交 `38b3353a` 那一类缺陷。**但这需要先给 §12.5 加一句例外**，否则新门一建就与现行条款撞车（同 U29）。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U29 · `VALIDATION_EVIDENCE_STANDARD.md` 在 §7 与 §12.5 自我否定

| 字段 | 内容 |
|---|---|
| **事项** | 门禁体系正本自己在一个条款里把「文档门」注册进注册面要求结构化证据判绿，在另一个条款里说文档域不设机器门。 |
| **分歧** | **设门**：同文件 `:174` 逐字「正例：某文档门声明一个非空目录，证据里的 `n_objects` 与 `inputs` 面实算数相等且大于零，绿」——把一个名为「文档门」的单元登记进注册面、要求结构化证据、判绿。**不设门**：同文件 `:420` 逐字「文档域不设机器门。文档域指四个面：文档一致性、文档写法、文档索引、锚存活」；`:424` 明列这些判据「没有机器执行面，正确性完全由审查兜住」；`:426`「不规定任何文档域的机器门」。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §3.1 B4（:182-188）、§2.3（:76-77）、§8.4（:531-536）。仓内：`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:174` ↔ `:420,:424,:426`。 |
| **核证** | ✔已核证：四处行号与原文均已在仓内直接读到（同上 U28 核证）。 |
| **影响** | `VALIDATION_EVIDENCE_STANDARD.md` §12 是门禁体系正本，**裁决面失真会被下游继承**。`审稿-RR09` §2.3 另记三处下游已发生的同型冲突：`DOCUMENT_INDEX.yaml:61` 派门、`DOCUMENTATION_STANDARD.md:46` 同句自相矛盾、`UNRESOLVED_REGISTER.md:40` 裁-24。 |
| **所需输入** | 负责人裁 §7 与 §12.5 哪个为准；并据此决定 `:174` 那条正例是删除还是移入非文档域。 |
| **建议** | **建议**以 §12.5 为准，删 `:174` 的「文档门」正例 —— 因为 §12.5 的论证（谓词形式 ≠ 登记资格）更根本，且 `:174` 的存在会让每个下游作者都能援引「标准自己有文档门正例」。**连带**必须把 U28 的结构门请求写成 §12.5 的显式例外，否则 U28 无法落地。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U30 · 文档域「不设机器门」冲突（UNRESOLVED_REGISTER 裁-24）登记是否已闭环

| 字段 | 内容 |
|---|---|
| **事项** | 登记册把「两条现行条款正面撞车」记为待裁决，但冲突的那一侧**看起来已经被改过**了 —— 登记与现状不同步。 |
| **分歧** | **登记说撞车**：`docs/engineering/UNRESOLVED_REGISTER.md:40`（裁-24）逐字「`DOCUMENT_GOVERNANCE.md` 抬头写『具备登记条件的条款进检查项表，由门禁面按名读取本文条款』，而 `VALIDATION_EVIDENCE_STANDARD.md` §12.5 定的是『文档域不设机器门』…须择一」，并给出倾向（以 §12.5 为准并订正 `DOCUMENT_GOVERNANCE.md` 抬头）+ 理由（谓词形式与执行面是两件事）+ 处置限制（「该文件不属本车道授权面，须另派改写」）。**现状已改**：`docs/engineering/DOCUMENT_GOVERNANCE.md:6` 现文已逐字改为「谓词形式不构成登记为机器门禁的依据——文档域不设机器门，正确性由对抗性审查逐条覆盖」，即**与 §12.5 一致，冲突方已消除**。而 `docs/engineering/DOCUMENTATION_STANDARD.md:46` 也已同步成同一口径。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §2.3（:72-84）、§4 Q10（:377 行盲复算，判定「登记已被推翻，方向相反」）。仓内：`docs/engineering/UNRESOLVED_REGISTER.md:40`、`docs/engineering/DOCUMENT_GOVERNANCE.md:6`、`docs/engineering/DOCUMENTATION_STANDARD.md:46`、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:418-426`。 |
| **核证** | ✔已核证：`DOCUMENT_GOVERNANCE.md:6` 现文与登记册 :40 描述的「旧抬头」**已不是同一句话**；`DOCUMENTATION_STANDARD.md:46` 同样已含「文档域不设机器门（`VALIDATION_EVIDENCE_STANDARD.md` §12.5）」的显式引用。 |
| **影响** | 登记册是过程台账，它**滞后于现状**。若不更新，下一轮审计会按登记册重复提出同一个已解决的问题（`审稿-RR09` §4 Q10 已经这么做过一次，并被证伪）。同时 U29 的残余（`:174` 正例）**仍然存在**，所以裁-24 不能整条划掉，只能改写为「抬头已对齐；残余为 §12.5 自身的 `:174` 正例」。 |
| **所需输入** | 负责人决定登记册 :40 是改写为「已闭环 + 残余项」还是撤下。 |
| **建议** | **建议改写而非撤下**：把 :40 改为「抬头冲突已由 G08-03 闭环（`DOCUMENT_GOVERNANCE.md:6`、`DOCUMENTATION_STANDARD.md:46` 已对齐 §12.5）；残余并入 U29」。这样既保留过程可追溯，也不误导下一轮。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U31 · 旧短式合同 ID 是否注册为第九个层

| 字段 | 内容 |
|---|---|
| **事项** | `ABI-001` / `CLI-002` / `API-001` / `ARCH-001` / `DATA-002`～`004` / `IO_001`～`003` 等旧短式合同 ID 不在注册词表内，须决定是正式注册还是随旧体系退役。 |
| **分歧** | **注册**：这些 ID 的命名规则对应**文档文件名**（`docs/engineering/API-001.md` 在册），是仓内实际存在的合同标识；不注册则它们不受 `TRACEABILITY_SPEC.md §3` 的注册面约束。**退役**：旧短式体系是上一代命名，继续注册会让两套 ID 词表并存。 |
| **出处** | 源件：`G08-03-整改A-索引与可追溯.md` §7 U8（:1091 行起）。仓内：`docs/engineering/TRACEABILITY_SPEC.md:88`（唯一注册面是 `INDEX.yaml`）、`docs/engineering/API-001.md`、`docs/contracts/INDEX.yaml`。 |
| **核证** | 转引（`TRACEABILITY_SPEC.md` 正文本次未展开）。 |
| **影响** | 与 U33（287 个在册缺失合同 ID）是同一注册面的两面：一边是「在册但缺登记」，一边是「不在册却存在」。两者须一并裁，否则注册面既不全也不唯一。 |
| **所需输入** | 负责人明确旧短式 ID 是第九层还是退役层。 |
| **建议** | **建议**注册为第九层但标注 `status: legacy` —— 因为 `API-001.md` 等文件是现行正本载体，退役 ID 会让它们变成无主资产。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U32 · 13 个设计节在索引侧无任何下级登记

| 字段 | 内容 |
|---|---|
| **事项** | 车道 A 按内容补了下级索引指针，但索引侧要不要补对应条目的 `upstream`，属正本唯一性裁决面，执行方拒绝自行补。 |
| **分歧** | **补登记**：这些节确有下级内容，指针已按内容补齐，索引侧不补则双向追溯仍断裂。**不补登记**：补了就等于**凭内容造追溯关系**，可能与既有正本口径冲突 —— 执行方明确拒绝的理由是这属于正本唯一性裁决面。涉及节号：`§1.1 §1.2 §2.3 §2.4 §2.5 §3 §4.1 §4.3 §4.5 §5.1 §6.1 §12.3 附录A`（13 个）。 |
| **出处** | 源件：`G08-03-整改A-索引与可追溯.md` §7 U5。仓内：`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md`。 |
| **核证** | 转引。 |
| **影响** | 13 个设计节的双向追溯不闭合；与 U33（R01 B-2，最高设计对 `docs/detail/` 整层零指向）同族。 |
| **所需输入** | 负责人授权「按内容补登记」这一动作。 |
| **建议** | **建议**授权，但要求**逐条列出「节号 → 下级文件 → 理由」三列证据**后再补，不接受「按内容推定」的批量补。这是索引面唯一能安全自动化的改法。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U33 · 「唯一正本」维度未审完：5 对候选 + 5 项未核证面

| 字段 | 内容 |
|---|---|
| **事项** | R01 的正本唯一性结论只覆盖了 11 对中的 6 对；其余 5 对与 5 项未核证面明确登记为「待补，不得视为已审」。 |
| **分歧** | **该维度已收敛**：审稿人自核 6 对样本后曾判「全部成立（正本唯一）」。**未收敛**：子代理按 11 对核查实为 **7 对真重复、3 对假警报**，直接证伪前者；未覆盖的 5 对是 `PHASE*_DETAILED_DESIGN vs PHASE*_API_V1`、`ARCH-001 vs docs/architecture/*`、`API-001 vs API_CONTRACTS.csv`、`RT-001 vs RUN_GRAPH*`、`PRODUCT_STORAGE_FORM vs *ARTIFACTS*`；5 项未核证面是 `api_inventory.csv vs API_CONTRACTS.csv`（450 行 vs 377 行，源件点名为「本次最可能遗漏的一对真重复」）、`config_separation_anchors.json` 与 `unified_object_registry.json` 未逐行、Phase2/Phase3 详细设计未读、`merged_TROUBLESHOOTING.md` / `common.md` / `STAR_DETECTION_IMPL_DESIGN.md` 未读。另有 3 对假警报（`API-001`/`RT-001`/`ANCHOR_CONTRACT`）被判「已闭合」但**审稿人未独立复核**。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §6 S4（:369,:376-378）、§3 B-4/B-6。仓内：`docs/contracts/api_inventory.csv`、`API_CONTRACTS.csv`、`docs/engineering/API-001.md`、`RT-001.md`、`ARCH-001.md`。 |
| **核证** | ✔部分核证：`docs/architecture/api_inventory.csv` 与 `docs/contracts/API_CONTRACTS.csv` **两文件在位**，行数差（450 vs 377）本次未复算。 |
| **影响** | **不能宣布「唯一正本」维度收敛**。这是 R01 阻断 B-4（对象集三份表）、B-6（权威链倒挂）同族；`api_inventory.csv vs API_CONTRACTS.csv` 若真重复，会再添一对。 |
| **所需输入** | 负责人授权补派该分片；或书面接受「本维度只覆盖 6/11 对」并把余下登记为已知缺口。 |
| **建议** | **建议**补派 —— 这是本件中「投入产出比最高」的一条：单个分片就能把「唯一正本」从 6/11 推到 11/11，且 `api_inventory.csv` 是仓内现成文件、无需额外输入。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U34 · P3 §9 位置集要求 `nside = 2^23`，生产中不可达

| 字段 | 内容 |
|---|---|
| **事项** | 正本 §9 的位置集要求含「HST 真实尺度（0.04″/px、`nside=2^23`）」，而代码硬钳到 `2^22`，该位置集**形式上不可满足**。 |
| **分歧** | **位置集可满足**：`docs/science/algorithms/DRIZZLE_GEOMETRY.md:356` 要求 `nside=2^23`。**不可满足**：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:726` 的 `NSIDE_MAX = 4194304 // 2^22`、`:794` 硬钳 `if (nside > NSIDE_MAX) nside = NSIDE_MAX;`；`DRIZZLE_GEOMETRY.md:172` 自己也写 `[NSIDE_MIN, NSIDE_MAX] = [16, 4194304 = 2^22]`。`2^23 = 8388608 > 4194304` 必被钳到 `2^22`；`2^22` 下 `hp_res ≈ 0.0503″`，对 0.04″/px 已是 1.26× **欠采样**，而正本 `:173-176` 的判读规则要求 1–2× **过采样**。 |
| **出处** | 源件：`审稿-RR05-P3 守恒映射算子.md` §1 B-2（:32-40）、§4 X-2（:182）。仓内：`docs/science/algorithms/DRIZZLE_GEOMETRY.md:172,:173-176,:354,:356`、`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:724,:726,:794`、`实验/healpix-polar/docs/EXP-07-POLAR.md:477`。 |
| **核证** | ✔已核证（算术部分）：`8388608 > 4194304` ⇒ True；`211076.28514206142 / 4194304 = 0.0503″`；`0.0503 / 0.04 = 1.2575`（欠采样）。另核：`EXP-07-POLAR.md:477` 的 `A_ref = 5.876108e-16` 与 `0.005″/px` 算得 `5.876e-16` 逐位吻合，两处互证。 |
| **影响** | §9 面积守恒闭合门的形式前提不可满足 ⇒ 该门在生产中**无法按原条款通过或被判无效**；若被迫改用 `2^22`，其读数落在欠采样区，**不得与「1–2× 过采样」结论混用**。与 U35（同一 §9 的闭合门在端点恒真失效）叠加 ⇒ §9 整套位置集条款当前**两头都失效**。 |
| **所需输入** | 负责人裁定：(a) 提高 `NSIDE_MAX`（需评估内存/性能，属实现面）；还是 (b) 改 §9 位置集为生产可达值。 |
| **建议** | **建议**改 §9 位置集而不是抬高 `NSIDE_MAX` —— 理由是 `NSIDE_MAX=2^22` 是有工程理由的钳位，改它会连带影响 HEALPix 内存与性能预算，而位置集只是一个**验收覆盖面**要求。**建议**把端点改成 `2^22` 并显式标注该档是欠采样、其读数只作下限检查。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U35 · P3 `tri_inside` 分档不一致登记：是真缺陷还是误报

| 字段 | 内容 |
|---|---|
| **事项** | 登记册断言的「五处里只有 `tri_inside` 快路径不遵守分档…隐含 +5.0e-7 量级权重台阶」被红队反证**不成立**；但红队自己也未坐实「不可达」。 |
| **分歧** | **是真缺陷**：子代理 ③（`6f1b5a00`）采信登记册并称 `:1370` 是「残留真实缺陷」。**证伪失败**：审稿人构造 X-3 反证后**不采信**（`spherical_o → ...` 路径不可达该分支）；但审稿人自己也**没有**给出「不可达」的一手证明，故 `UNRESOLVED_REGISTER.md §35.1` 的登记**既不能撤也不能当缺陷修**。 |
| **出处** | 源件：`审稿-RR05-P3 守恒映射算子.md` §3.2 M-4（:128-132）、§4 X-3（:182-193）、附 U-1（:372）。仓内：`docs/engineering/UNRESOLVED_REGISTER.md:1256-1268`（§35.1）、`:1270`、`lib/algorithms/drizzle/…/spherical_overlap.cpp:1370`。 |
| **核证** | 转引。⚠ 该仓内源文件路径本次未定位（`spherical_overlap.cpp` 的确切相对路径源件已给，标转引）。 |
| **影响** | 决定 §35.1 是撤登记（误报）还是修代码；也决定 M-6 的正本措辞能否落定（「已登记、可达性待实测」vs「必须修」）。§35.1 自记的权重台阶量级直接影响生产面积口径。 |
| **所需输入** | 需**编译并插桩**统计 `tri_inside && g.max_angle < 1e-3` 的触发计数（生产配置 `nside ∈ {16,32,64,128}` × 4 种 `pixfrac` × 接缝/极点位置）—— 属前台统一执行项（`AGENTS.md` §9）。 |
| **建议** | **建议**由前台跑一次插桩统计再裁。若触发计数为 0 ⇒ 撤登记并把 §35.1 改为「已证伪」；若 > 0 ⇒ 按 M-6 措辞落「已登记、可达性已实测」。**不建议**在无统计的情况下维持现状 —— 当前是「有登记、无结论」的悬挂态。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U36 · P3 争议点②：接缝绝对亏空的指数 `p = −0.83` vs `p = −1`

| 字段 | 内容 |
|---|---|
| **事项** | 正本说接缝处相对闭合项不可达、∝`1/A_drop`（指数 −1）；实验侧单方标该句作废并判 `p ≈ −0.83`。 |
| **分歧** | **正本方**：`docs/science/algorithms/DRIZZLE_GEOMETRY.md:372-375`（句仍在 `:373`）称相对闭合项在接缝不可达、∝`1/A_drop`。**实验方**：`实验/healpix-polar/REPORT_paper.md:112,:189`、`REPORT_experiment.md:95` 单方标注该句作废，判 `p ≈ −0.83 ≠ −1`。**审稿人立场**：程序缺口成立（台账/`DISPUTES`/`DISPUTE_RESOLUTION` 三层台账都缺这一条），但**实验侧的统计效力不足** —— X-4 只推翻了「实验侧实测足以作废正本」这一主张，裁决方向应改为「**两造都须收窄**」。 |
| **出处** | 源件：`审稿-RR05-P3 守恒映射算子.md` §3.2 M-2（:109-119）、§4 X-4（:195-213）、附 U-2（:373）。仓内：`docs/science/algorithms/DRIZZLE_GEOMETRY.md:372-375`、`实验/healpix-polar/REPORT_paper.md:112,:189,:225-234`。 |
| **核证** | 转引。 |
| **影响** | 决定正本该句删、留还是改写。操作层面影响不大（审稿人给的替代口径「相对项在该域一律不适用，一律以绝对项判，阈值 `A_drop ≤ 1e-9 sr`」**与指数无关**），但**台账与 `DISPUTE_RESOLUTION` 的同步欠账会持续存在**。 |
| **所需输入** | 补统计效力（更多点 / 重复测量）与 **seed 固定复跑** —— 源件明写这是前置条件。属前台执行项。 |
| **建议** | **建议**采用源件的替代口径（该域一律用绝对项判）并把正本 `:372-375` 改为「相对项在本域不适用」—— 这样**不必先解决指数争议**就能收口，指数留作待复跑的登记项。这是成本最低的路径。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U37 · P4 门禁在冻结门表中零登记 + 算子正本载体不存在

| 字段 | 内容 |
|---|---|
| **事项** | P4 既没有机器可校验的门事实源，也没有可追溯的算子正本锚。 |
| **分歧** | **(a) 门表**：`docs/science/algorithms/GATES_AND_TOLERANCES.md:5-6,:20-23,:28-29,:54-75` 的冻结门表 §3 共 20 行**全为 `G-P1-*`、零条 P4 行**，而 `fix01` 的 14 条原子门、`fix02` 的 `tol_rel=0.05` / `dex<0.02` / `Δ*=16` 全部未登记 —— 这**违反该表自订的 R1/R2/R5**；R5 另要求门引用 §2 登记的 SNR 定义，而 P4 用的是 `SNR_true`。**(b) 算子正本**：`docs/science/CONTROL_WEIGHT_SNR.md:226` 指向的 `docs/detail/algorithms_phase1/…` 已被提交 `ac058121` 删除（`git log --all --diff-filter=D` 显示删了 9 个文件，含 `07_noise_snr.md`）；代码 `code/route3/exp05_operator_axioms.py:5` 指向同一路径。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3 阻断-3（:68-72）、§7 第 1、2 行（:231-232）。仓内：`docs/science/algorithms/GATES_AND_TOLERANCES.md:5-6,:20-23,:28-29,:54-75`、`docs/science/CONTROL_WEIGHT_SNR.md:21,:214,:226,:231,:263`、`实验/dense-snr-reconstruct/code/route3/exp05_operator_axioms.py:5`。 |
| **核证** | ✔已核证：`[ -e docs/detail/algorithms_phase1/07_noise_snr.md ]` → 不存在；`docs/detail/` 下无 `algorithms_phase1/` 子目录（见 U17 核证）。 |
| **影响** | P4 的 10 条门**没有任何权威登记面**，判据变更无约束；同时算子定义无正本锚。这直接导致 U22/U23/U24/U38/U39/U40 六条全部**「无正本强度可依」** —— 即 P4 这一整个实验单元的判据体系目前悬空。 |
| **所需输入** | 负责人二择一：把 P4 门逐条登记进冻结门表（需给门 ID / 判据式 / 量测域 / 统计量 / SNR 定义 / 阈值 / 来源 / 证据 八项）；以及算子正本是上提到 `docs/science/` 还是把指针改到现存的 `docs/detail/registry/acsd.phase1.noise-snr.md`。 |
| **建议** | **建议**先做「算子正本载体」这一小步（改动小、解除 6 条的依赖），再做门表逐条登记（工作量大但机械）。**注意** `审稿-RR06` §7 第 2 行又把算子正本问题转给跨文档一致性轮裁决 —— 两条处置线并存，须负责人指定唯一归属，否则会互相等。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U38 · P4 公理③ 对 mesh 算子要不要给例外条款；加法性要不要补测

| 字段 | 内容 |
|---|---|
| **事项** | 公理表缺两条：mesh 算子要不要例外；算子加法性（公理④）要不要补测或声明不可加。 |
| **分歧** | **mesh 无需例外**：反例 R3 证明「过 A2∧A3∧A4 仍可严格非线性」，公理③对 `natural_bicubic_spline_clip_mesh_median_v1`（冻结生产算子，`registry:273`）无例外。**需要例外**：代码 `exp05_operator_axioms.py:177` 的门判据用 `'mesh' not in k` 把 `mesh_median`（节点偏差 38.33）**静默排除** —— 即生产算子的偏差被测试口径排除掉了。**加法性**：公理④「域外 fail-closed」零测试；反例 R3 表明公理表不足以推出线性。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3.2 须修-5、须修-6（:102-109）、§4 反例 R3（:175-182）、§4 反例 R4（:182-188）。仓内：`实验/dense-snr-reconstruct/docs/registry:273,:298`、`实验/dense-snr-reconstruct/code/route3/exp05_operator_axioms.py:177`。 |
| **核证** | 转引。⚠ `exp_p4_04_brightness_forward.py` 与 `exp05_operator_axioms.py` 所在的 `实验/dense-snr-reconstruct/` 目录在本会话期间有文件被修改，见文首偏差登记。 |
| **影响** | 冻结生产算子的验证被测试口径排除，这是**判据判别力**问题（`AGENTS.md` §8「判据须在正确实现下绿、注入缺陷时红」）。公理表不完整 ⇒ 任何依赖公理表的论证都无强度。 |
| **所需输入** | 负责人裁 mesh 例外；并决定加法性是补测还是显式声明不可加。 |
| **建议** | **建议**给 mesh 算子**显式例外条款**而不是靠测试代码里的 `'mesh' not in k` 隐式排除 —— 隐式排除在代码里不可见、不可审计，正是本轮反复出现的问题形态。**建议**加法性补测（公理表缺它 ⇒ 表是不完备的，完备性比单条性质更重要）。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U39 · P4 `valid_domain_mask` 复活还是删除；`fix02` 的平面拟合改正

| 字段 | 内容 |
|---|---|
| **事项** | 一个定义了判据但全仓无人调用的函数，以及一个自称「按生产同法复现」但实际是无权 LS 的拟合。 |
| **分歧** | **保留**：`valid_domain_mask` 定义了 `[1/2,2]` 比值判据。**删除**：全仓只有定义行，`calibers()` 四次调用**均传 `None`**，单元自己在 `REPORT_experiment.md:184-189` 已认。**fix02 的拟合**：自称「按生产同法复现」，其 `plane_fit` 实为**无权 LS**，而正本 F3 要求**相对误差加权 + 控制项** ⇒ 两者的「同法」说法不成立。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3.2 须修-10（:128-129）、§2 表第 16 行（:44）。仓内：`实验/dense-snr-reconstruct/code/fix/fix02_*.py`、`实验/dense-snr-reconstruct/REPORT_experiment.md:184-189`。 |
| **核证** | 转引（`fix02` 的确切文件名源件已给，标转引）。 |
| **影响** | 「定义存在但从不被调用」是恒真/空断言的一种变体；`fix02` 若继续自称「按生产同法复现」，其全部读数都不能作为生产面证据。 |
| **所需输入** | 负责人二择一（复活并接线 / 删除）；并决定 `fix02` 是改正还是改自述。 |
| **建议** | **建议删除** `valid_domain_mask`（判据已经由 U23 的绝对尺度门覆盖），并把 `fix02` 的自述改成「无权 LS 近似」，同时在报告里标其读数**不作为生产面证据**。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U40 · P4 E4/E8 的全部证据位于 `run/` 下（被 `.gitignore` 排除）

| 字段 | 内容 |
|---|---|
| **事项** | 争议②已被正确反驳（E4/E8 确有完整读数），但这些读数的**唯一出处**在 `run/` 下 ⇒ 干净 clone 上不存在。 |
| **分歧** | **已量化**：`实验/dense-snr-reconstruct/REPORT_paper.md:151,:154` 明确「已量化，不是未量化项」，且有解析律。**无可核面**：全部数值的唯一出处 `run/FINAL-07/审核包/科研审查/P4_订正/evidence/F6_gain_sensitivity.json` 位于 `run/*`，被 `.gitignore:17` 排除。且审稿人**亲自确认 S2 子代理未回报** —— 该条是单方取证、无第二来源交叉。 |
| **出处** | 源件：`审稿-RR06-P4 重建稠密信噪比.md` §3.2 须修-8（:118-119）、§2 表第 13 行（:41）、§6 S2 行（:212）、§8 S-9（:298-300）。仓内：`run/FINAL-07/…/F6_gain_sensitivity.json`、`.gitignore:17`、`实验/dense-snr-reconstruct/REPORT_paper.md:151,:154`。 |
| **核证** | ✔已核证：`.gitignore:17` 为 `run/*`（与 U26 同一行）。 |
| **影响** | 「已量化」这个结论目前**只在作者机器上成立**。若证据不迁入，验收方无法复核，结论等同未证。 |
| **所需输入** | 负责人决定把该 JSON 迁入仓内可跟踪位置（如 `实验/dense-snr-reconstruct/results/`），还是重跑生成。 |
| **建议** | **建议迁入**（成本低、纯搬运）而不是重跑 —— 迁移后须同时补一条 `.gitignore` 例外或放到本来就被跟踪的 `实验/` 下。**这是 E4/E8 从「单方取证」变成「可复核」的最小代价动作。** |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U41 · 跨轮未回报的子代理分片（7 个）

| 字段 | 内容 |
|---|---|
| **事项** | 十轮里有 7 个子代理分片至定稿未回报或从未派发成功，相关域结论因此缺第二视角。 |
| **分歧** | **已由审稿人独立补足**：各域审稿人均声明「相应域已亲自通读完成、结论不依赖其回报」。**缺口客观存在**：① R02 的 S4/D（DOI 逐条联网核验，全任务未回，全轮唯一完全缺席的分片）；② R03 的 S2/S3/S4（3 片未回报）；③ R04 的 `795c479e`（跨帧可比性，失效域图谱未产出）与 `d79d582e`；④ R05 的 `11c2e5cd` 与 `fdb8e5ae`（2 片未交付）；⑤ R08 的 S2 与 S4；⑥ R09 的 A'/B'/C'（3 条重复分片未单独复核）。 |
| **出处** | 源件：`审稿-RR02` §6 S4 行（:323）、§10（:451）；`审稿-RR03` §6（:275,:280-282）；`审稿-RR04` §6（:289-290,:315）、§9 第 4 条（:457）；`审稿-RR05` §6（:255-258）、附 U-6（:377）；`审稿-RR08` §9 UNRESOLVED-1（:479）；`审稿-RR09` §6.1（:382,:395）、§6.3（:456）。 |
| **核证** | 转引（各轮自述）。 |
| **影响** | 后果各不相同：`审稿-RR02` 的缺位直接导致 BLK-02 的「编造」定性降级为待核（见 N-01）；`审稿-RR04` 的缺位导致「跨帧可比性失效边界清单」至今无正面清单（U42）；`审稿-RR09` 的约 25 条（实列约 43 条）不采信条目中若干「方向可信」（如 `ACS_ERR_*`/`ACS_FIO_*`/`P3_OUT_*`/`DPSF_FIT_*` 四族库返回码与 3/4/5/6/7 全撞值）。 |
| **所需输入** | 负责人决定是否补派；以及是否接受把这些域的结论按「单来源」记账。 |
| **建议** | **建议**只补派三条真正有缺口的：R02 的 DOI 联网核验（否则 §四 的 N-01～N-10 无法收敛）、R04 的失效域图谱（U42）、R09 的返回码撞值复查。R03/R05/R08 的缺口已由审稿人本人取证，**建议按「已记明缺口」结案，不再补派**。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U42 · P2「跨帧可比性」的失效边界清单至今未产出

| 字段 | 内容 |
|---|---|
| **事项** | U6 若采纳「改写立身主张」的方向，改写后的主张必须带失效域清单；该清单至今无正面产出。 |
| **分歧** | **已够**：`frame_snr-canon.md` 已加域并修好。**不够**：审稿人明写未回报分片的计划覆盖面未完成，且已列出的待补失效域含**文档未承认但物理上存在**的项 —— 滤光带、增益、PSF 形状/`A_NEA`、暗流、天光梯度、跨 tile `ZP_syn` 齐次性 **0.7%–5.2%**（出处 `docs/engineering/UNRESOLVED_REGISTER.md:416`）。 |
| **出处** | 源件：`审稿-RR04-P2 跨帧绝对信噪比.md` §9 第 4 条（:457）、§6（:290,:315）。仓内：`实验/absolute-snr/docs/frame-snr-canon.md:17,:215-217`、`docs/engineering/UNRESOLVED_REGISTER.md:416`、`docs/science/CONTROL_WEIGHT_SNR.md:20,:101`。 |
| **核证** | 转引。 |
| **影响** | 若不先补清单就改写 P2 主张，会把一个无条件断言换成另一个带未声明适用域的断言 —— 与 U14 是同一形态。 |
| **所需输入** | 需 R09/R10「反例与边界」轮补足（源件指定轮次）。 |
| **建议** | **建议**在 U6 裁决时**同时要求**失效域清单作为交付物之一，否则改写不予接受。这是把「限定必须上提到权威面」这条规律（U14）应用到 U6 上的具体做法。 |
| **阻塞级别** | 阻断下一任务 |

### G08-05-U43 · 文档域其余派门处（`DOCUMENTATION_STANDARD.md:23/:24/:35` 等六处）

| 字段 | 内容 |
|---|---|
| **事项** | 除已核的 `:46` 外，还有五到六处在向文档域派机器门，审稿人未亲读、全部未采信。 |
| **分歧** | **已采信一处**：`docs/engineering/DOCUMENTATION_STANDARD.md:46` 采信 → S-11（该处同时含「文档域不设机器门」的显式引用，见 U30）。**未采信五处**：`:23`、`:24`、`:35`，以及 `docs/engineering/CONFIG_CONTRACT.md:226`、`docs/engineering/RELEASE_STATUS.md:89`、`docs/engineering/STANDARDS_REGISTRY.md`（源件 §2.3 3-1/3-2）。审稿人的盲复算 Q9 记「违反至少 4 处」，但**那是审稿人自己找到的违反项，不是子代理这六处** —— 两者不可混。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §2.3（:72-84）、§5 Q9（:369）、§6.2 分片 C（:432）、§6.3（:456）。仓内：`docs/engineering/DOCUMENTATION_STANDARD.md:23,:24,:35,:46`、`docs/engineering/CONFIG_CONTRACT.md:226`、`docs/engineering/RELEASE_STATUS.md:89`。 |
| **核证** | ✔已核证 `:46` 现文（与 §12.5 一致，见 U30）；其余五处本次**未亲读**，标转引。 |
| **影响** | 与 U29/U30 同族：这些是 §12.5 自我否定（U29）在下游的具体犯案点。不清则每次审计重复争论。 |
| **所需输入** | 负责人先裁 U29（§7 与 §12.5 哪个为准），再据此批量处置下游派门处。 |
| **建议** | **建议**在 U29 裁完后，用一次机械扫描把全仓「向文档域派机器门」的句子全列出来再批量处置 —— 否则只清这六处，下一轮还会再冒出新的。源件的机械扫描方法可照用（§8.7「全域悬空引用的独立重测」，:552-566）。 |
| **阻塞级别** | 阻断下一任务 |

---

## 三、不阻塞但须登记（18 条）

### G08-05-U44 · `AGENTS.md:93`「SubAgent 之间无法直接通信」已失效

| 字段 | 内容 |
|---|---|
| **事项** | `AGENTS.md` 第 7 节的这条前提在本会话中**实际不成立**，而本轮十轮审稿的派发/回传机制正是建立在这条被推翻的前提上。 |
| **分歧** | **条款写着无法通信**：`AGENTS.md:93` 逐字「SubAgent 之间无法直接通信，防撞车由前台预防；后到者得知先到者已落内容，不回滚覆盖」。**实际可通信**：本轮十轮审稿全部用了「派子代理 → 子代理回传 → 审稿人复核」；R06 §6 明记 `send_message 对 subagent …` 的实际行为（`:207`）；R02 的子代理联网取证结果也是经回传通道回来的。 |
| **出处** | 源件：本次汇总**独立核证发现**（十轮审稿件均未点名此条）。仓内：`AGENTS.md:93`（第 7 节的第二条 bullet）、`AGENTS.md:19-31`（§2 权威链图）。 |
| **核证** | ✔已核证：`wc -l AGENTS.md` = **130 行**；`sed -n '90,96p' AGENTS.md` → 第 93 行正是「- SubAgent 之间无法直接通信，防撞车由前台预防；后到者得知先到者已落内容，不回滚覆盖；」。文件上下文为「## 7. 派单与并行」。 |
| **影响** | **单点，但影响面是全部派发纪律**。条款继续写着「无法通信」，则 (a) 前台按 `AGENTS.md` §7「派单前列出精确文件列表并逐一比对」做的人工防撞车在有能力通信时是**多余成本**；(b) 更要紧的是它与 `AGENTS.md` §11「发布决定只属于项目负责人」并存时，容易被读成「子代理之间不互相知会、所以我采信的那份结论是第一手」——本轮已有多次「后到者推翻先到者」（U45 全部条目都是这种形态）。 |
| **所需输入** | 负责人裁 `AGENTS.md:93` 是改写为「SubAgent 之间可经前台通道互相知会」还是保留原句并补一句限定。 |
| **建议** | **建议改写**：把该条改为「SubAgent 之间不直接通信，须经前台转达；前台转达时须注明来源与轮次」。理由是**现状是「可经前台通道互相知会」**，原句「无法直接通信」虽然字面没错，但会被读成「互相不知道对方存在」，这与本轮实际情况不符。**这是一条低成本、高澄清度的改动。** |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U45 · 十轮审稿件的派单记录自相矛盾（6 轮共 9 处）

| 字段 | 内容 |
|---|---|
| **事项** | 六轮审稿件对「派了几个子代理」的记述在件内自相矛盾，按 ID 核对会对不上。 |
| **分歧** | **件数（标题/抬头）** vs **实际行数（表格）** vs **回报状态**三者互不吻合：R01 抬头「4 个（实为 3 个不同分片）」vs `:358`「5 次派发、4 个不同分片」；R02 `:314`「5 任务 6 实例」vs §6 表格实列 **7 行** vs `:451` 回报名单「S1、S2×2、S3、S5」= **5 个**；R03 「4 个分片」vs 「实际启动 7 次」vs §6 回报列「S1 回报 / S2·S3·S4 未回报」；R04 「7 次派发 = 5 独立分片 + 2 重复」vs 7 行派发表 vs「2 个分片至定稿未返回却仍列为已派发」；R05 `:246`「派发 5 个」vs §6 表格 **7 个 ID**（5 已交付 + 2 未交付）；R07 `:324/:380/:498` 三处均写 5 vs §6 表 **6 行**（S1 被重复派为 S1-a/S1-b）；R08 `:334`「6 个实例、4 个不重复分片」vs `:479`「5 个已派发子代理中 4 个已回」；R09 `:382` 一处写「每次调用重复执行一次（同一 prompt 被派发两遍）」另一处写「共发起 8 次调用」；R10 `:282` 同时给「6 个子代理」与「5 个不同分片」。 |
| **出处** | 源件：`审稿-RR01` §6（:317,:358,:371-372）、`审稿-RR02` §6（:314）、§10（:451）、`审稿-RR03` §6（:275,:277-282）、`审稿-RR04` §6（:280,:289-290）、`审稿-RR05` §6（:246,:250-256）、`审稿-RR07` §6（:324,:380,:494,:498）、`审稿-RR08` §6（:334）、§9 UNRESOLVED-1（:479）、`审稿-RR09` §6.1（:382,:395）、`审稿-RR10` §6.1（:282）。 |
| **核证** | 转引（各轮自述，逐条并列保留）。 |
| **影响** | **不影响任何技术结论**（各轮的技术判定都由复核方背书），但影响**审稿过程的可审计性**：负责人若按 ID 清单核对派发记录会直接对不上。R07、R08 各有一处更具体的后果 —— R08 `:334` 称「重复件跑完后未采用其产出」与 `:341` 直接冲突（S2′ 的 5 条结论被逐条采信并升格为 S-7）；R07 的 S1 与 S1-b 对同一问题给出**相反定性**（须修 vs 建议级）。 |
| **所需输入** | 以**派发日志**（而非审稿件叙述）为准核定每轮的真实派发次数与分片数，然后回写对应审稿件。 |
| **建议** | **建议**以日志为准统一回写，并把两说在本件中并列保留（本件已如此）。**不建议**为对齐而改审稿的技术判定 —— 那些判定有复核背书，与派发计数无关。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U46 · `实验/裁决台账.md` §5.1 五条「依据未给待补」

| 字段 | 内容 |
|---|---|
| **事项** | 五条裁决条款的**结论在册、依据未给**，需补一手锚。 |
| **分歧** | **可采信结论**：五条均已进台账正文。**依据待补**：`A-P1-03` mag_tolerance 预过滤「边际保护 ≈0」无可核锚（无脚本名、无 `文件:行`、无 DOI）；`A-P1-05`「场形弯曲度/阶数是科学量」的非结构性论证在路线侧报告中、台账未转写；`A-P1-11`「两套星族不一致的 ZP 级影响为 μmag 级」台账未转写依据；`A-P1-12` 依据是规格文本自身冲突（「缓冲 1.2」vs「钳位下界 1.0」）但未给行号锚；`A-P4-05` Shepard 1968 正确 DOI 已在册，**Keys 1969 的订正号只记为「见路线 refs 台账」，该台账已退场**。 |
| **出处** | 源件：`实验/裁决台账.md` §5.1（:403-411）。仓内：`实验/裁决台账.md`、`docs/engineering/UNRESOLVED_REGISTER.md`。 |
| **核证** | ✔已核证：`实验/裁决台账.md` 共 **425 行**，§5.1 表在 `:405-411`，五条齐全。 |
| **影响** | 依 `AGENTS.md` §4，「我觉得」「惯例如此」不算证据；这五条目前正落在这一档。`A-P4-05` 最重 —— 它引的「路线 refs 台账」已退场，等于依据**已经丢失**。 |
| **所需输入** | 负责人提供五条的一手锚；`A-P4-05` 需决定是重跑复现该 DOI 还是撤下订正。 |
| **建议** | **建议**前四条按「路线侧报告 → 转写到台账并带 `文件:行`」补齐（成本低、文件多在仓内）；**建议** `A-P4-05` 单独处理 —— 已退场的台账不能作为依据，必须重跑或撤。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U47 · `实验/裁决台账.md` §5.2 三条「登记待补（依据齐，登记面缺）」

| 字段 | 内容 |
|---|---|
| **事项** | 三条依据已齐，但登记面缺登记义务 —— 属「做了但没记」。 |
| **分歧** | **取值已定**：REG-01 `c = 1.152` 的原始标定记录未登记入常数表（取值裁决已定，登记义务保留，归属 `docs/science/NOISE_MODEL.md §5a`）；REG-03 矢高常数 `sag0 = 8.094e-2·ρ₁` 的原始推导行未定位（单位已判为 `ρ₁`，数值与两路实测一致，归属 `docs/science/algorithms/DRIZZLE_GEOMETRY.md`）。**定义本身未定**：REG-02 VOS 非归一化注入幅度的**注入定义**未定（区间已给 3.95%–12%，**定义口径待负责人澄清**，归属 `docs/science/DISPUTE_RESOLUTION.md §10`）。 |
| **出处** | 源件：`实验/裁决台账.md` §5.2（:413-419）。仓内：`docs/science/NOISE_MODEL.md`、`docs/science/algorithms/DRIZZLE_GEOMETRY.md`、`docs/science/DISPUTE_RESOLUTION.md`。 |
| **核证** | ✔已核证：§5.2 表在 `:415-419`，REG-01/02/03 三条齐全；`:419` 的 sag0 数值 `8.094e-2` 与 `审稿-RR05` §5 盲复算的 `-0.09968368384289383` 等常数属同一族。 |
| **影响** | REG-01/REG-03 是登记义务欠账（可补）；**REG-02 是真待裁** —— 它是 U36（P3 争议点② 指数）同域的量，若定义口径不定，指数争议无法收口。 |
| **所需输入** | REG-02 需负责人澄清「注入定义口径」；REG-01/03 需补登记行。 |
| **建议** | **建议**把 REG-02 并入 U36 一并裁（两者都是 P3 接缝域的口径未定），避免同域两条裁决各走各的。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U48 · `实验/裁决台账.md` §5.3：A-P1-13 数字冲突 7.4% vs 8.03%

| 字段 | 内容 |
|---|---|
| **事项** | 同一条裁决条款在两处记了不同的偏差百分比，**两个读数并存，均未撤回**。 |
| **分歧** | **7.4%**：裁决条款记「渐近式 `1.2533/√n` 在 n = 3 高估 **7.4%**」。**8.03%**：单元内路线报告自记「高估 SE **8.03%**」。源件判为「须由前台以固定 seed 复跑裁定单一值」。 |
| **出处** | 源件：`实验/裁决台账.md` §5.3（:421-425，末行）。仓内：`实验/裁决台账.md`。 |
| **核证** | ✔已核证：`实验/裁决台账.md` 末行（:425）逐字「A-P1-13 \| 裁决条款记『渐近式 `1.2533/√n` 在 n = 3 高估 7.4%』；单元内路线报告自记『高估 SE 8.03%』\| 两个读数并存，均未撤回；须由前台以固定 seed 复跑裁定单一值」。 |
| **影响** | 单点，但它是**裁决台账自身的算术不自洽** —— 而台账是五个实验单元的裁决依据来源。一处 7.4% vs 8.03% 会让引用该条的推理都带一个未声明的误差。 |
| **所需输入** | 前台以固定 seed 复跑（属 `AGENTS.md` §9 的前台统一执行项）。 |
| **建议** | **建议**复跑后**同时**改两处（裁决条款 + 路线报告）并撤掉另一个；只改一处会让下次审计再撞一次。另建议台账增设一栏「本条的复跑命令与 seed」，让这类数值**可重放**。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U49 · 元话语「（正向约束）」87 处 / 20 个文件

| 字段 | 内容 |
|---|---|
| **事项** | 规范 02 §2 点名禁止的元话语，在仓内被当作**受控术语标记**使用，且用了 87 处。 |
| **分歧** | **违规**：规范 02 §2 逐字把「（正向约束）」列为禁例。**是术语**：`docs/engineering/DOCUMENT_GOVERNANCE.md` §7.2 的封闭词表**未收录元话语类**，仓内用它标注「本条是规范 02 §2 意义上的正向约束」。 |
| **出处** | 源件：`G08-03-书写形态体检.md` §7 U1（:465 行表内第 1 行）。仓内：`docs/engineering/DOCUMENT_GOVERNANCE.md` §7.2、20 个仓内文件。 |
| **核证** | 转引（规范 02 不在本会话文件系统内）。⚠ 注意：本次已独立核证的 `docs/engineering/DOCUMENT_GOVERNANCE.md:6` 现文**已含**「谓词形式」这类表述，说明该文件正在被改写中 —— 87 这个数是体检轮当时的读数，**处置前须重测**。 |
| **影响** | 87 处占该轮确认违规的 44%。判违规 ⇒ 20 个文件批量删括号；判术语 ⇒ 改规范 02 §2 措辞 + 给 `DOCUMENT_GOVERNANCE.md` §7.2 补第五类词表。连带须决定 `DOCUMENT_GOVERNANCE.md` §7.2「词表只有一处实现」「空即判红」的自述是否随之修正。 |
| **所需输入** | 负责人二择一裁定。 |
| **建议** | **建议**判为术语并补进 §7.2 的封闭词表 —— 理由是它在表达一个**有信息量的区分**（正向约束 vs 反向约束），删掉 87 处会丢失语义；而「封闭词表未收录」是词表的缺口，不是文本的缺陷。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U50 · 标题里的 `v1` / `V1`（4 处）

| 字段 | 内容 |
|---|---|
| **事项** | 规范 02 §2 禁「版本锚」，但 `v1` 是**文档身份名的一部分**。 |
| **分歧** | **违规**：规范 02 §2 禁版本锚。**是身份名**：`docs/engineering/CLI_PROTOCOL_V1.md:1`、`docs/engineering/MANIFEST_VERIFY_V1.md:1`、`docs/engineering/PHASE1_API_V1.md:1`、`docs/engineering/TRACEABILITY_SPEC.md:1` 的 `v1` 与**文件名 `*_V1.md` 一致**，不指向任何变更历史。体检轮**未**把 4 处计入确认违规。 |
| **出处** | 源件：`G08-03-书写形态体检.md` §7 U4（:465 行表内第 4 行）。仓内：上述 4 个文件。 |
| **核证** | ✔已核证四文件在位（`docs/engineering/` 下确实有 `CLI_PROTOCOL_V1.md`、`MANIFEST_VERIFY_V1.md`、`PHASE1_API_V1.md`、`TRACEABILITY_SPEC.md`）。 |
| **影响** | 若判违规，则须同时改 4 个**文件名**，会引发大量交叉引用改动（含 `DOCUMENT_INDEX.yaml` 的 path 列、`docs/contracts/INDEX.yaml`、代码里的路径常量）。 |
| **所需输入** | 负责人裁定。 |
| **建议** | **建议**判**保留** —— 理由与体检轮一致：删之不损语义、反增断链；且文件重命名属破坏性变更，成本与收益完全不成比例。**建议**在规范 02 §2 加一句例外：「文档身份名中的版本号（与文件名一致）不属版本锚」。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U51 · `P-159` 类「P-1xx 定一」2 处

| 字段 | 内容 |
|---|---|
| **事项** | 形态上是任务号（规范 02 §2 点名 `P-175` 为例），但所在行同时承载**可执行的 exit 7 语义**。 |
| **分歧** | **违规**：形态上是 `P-1xx` 任务号。**保留**：所在行同时承载机器可查的 exit 7 语义（`docs/detail/infrastructure/21_observability.md:47`、`docs/detail/LOG_AND_ERROR_SYSTEM.md:129`），**删号不损失语义**。体检轮按「exit 7 是机器可查对象」判保留，与 §2 的字面口径存在张力。 |
| **出处** | 源件：`G08-03-书写形态体检.md` §7 U3（:465 行表内第 3 行）。仓内：`docs/detail/infrastructure/21_observability.md:47`、`docs/detail/LOG_AND_ERROR_SYSTEM.md:129`。 |
| **核证** | ✔已核证两文件在位。 |
| **影响** | 2 处。若判删，会触及 exit 7 的可读性（不触及可执行性，因为码值本身在别处定义）。 |
| **所需输入** | 负责人确认判别规则是否细化为「编号可删且语义无损 ⇒ 删」。 |
| **建议** | **建议**采纳体检轮的判别规则并写进规范 02 §2（这条规则同时能覆盖 U50 的 `v1` 情形，是个通用出口）。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U52 · 目录 README 豁免口径二选一（13 份 README 是否入索引）

| 字段 | 内容 |
|---|---|
| **事项** | 最高设计的无条件总则与索引自设的 README 豁免正面冲突。 |
| **分歧** | **全是缺陷**：`docs/ACSD_DESIGN.md:28` 写「`DOCUMENT_INDEX.yaml` 是全文档集唯一索引地图，与文件树一致：未登记或登记不存在均为缺陷」—— 这是**无条件总则**，据此 13 份目录 README 全是缺陷。**全合规**：`docs/DOCUMENT_INDEX.yaml:17-18` 与 `:31` 自设「目录招牌件（各目录 README.md）…不进规范索引面」。**豁免无规范依据**：规范 01:61 只要求每目录有 README，未说 README 不入索引。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §3 须修 M-13（:206-210）、§2 表第 13 行（:67）。仓内：`docs/ACSD_DESIGN.md:28`、`docs/DOCUMENT_INDEX.yaml:17-18,:31`、规范 01:61。 |
| **核证** | ✔已核证：`docs/DOCUMENT_INDEX.yaml:17-18`、`:31` 确含 README 豁免；`docs/ACSD_DESIGN.md:28` 确为无条件总则。 |
| **影响** | 决定 13 份 README 是补登记还是写例外条款；口径不统一会让每次索引审计重复争论。**注意与 U56 联动**（索引闭合性未逐行核）。 |
| **所需输入** | 负责人二择一：补登记，或在 `ACSD_DESIGN.md:28` 为目录招牌件写明例外。 |
| **建议** | **建议**写明例外，并把口径**同时**写进最高设计与索引抬头 —— 只改一处两边还会再分叉。这是 R01 的作者初判「✅ 通过」后经交叉复核才翻案的一条，属高价值发现。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U53 · C4 判据是否对空白敏感

| 字段 | 内容 |
|---|---|
| **事项** | 索引与最高设计有 1 处仅空格差异的偏差，记为建议还是须修，取决于判据的空白敏感性定义。 |
| **分歧** | **须修**：`docs/DOCUMENT_INDEX.yaml:544` 写「§5（mosaic：相对定标·排异·集成）」，实际标题是「## 5. mosaic：相对定标 · 报异 · 集成」—— 若判据是逐字相符则须修。**放过**：若判据先做空白归一化则可放过，253/253 实质相符，前台「索引侧 0 红」的结论成立。**判据未定义**：这是本条的根因 —— 判据本身没有写明是否空白敏感。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §3 建议 S-1（:234-238）、§4 R01-C5（:288-289）、§5（:309）。仓内：`docs/DOCUMENT_INDEX.yaml:544`、`docs/ACSD_DESIGN.md:300`。 |
| **核证** | 转引。 |
| **影响** | 决定 1 条建议是否升格为须修，并决定**前台 435 条 C4 红统计的判定口径**（见 U55）。 |
| **所需输入** | 前台给出 C4 是否空白敏感的明确定义。 |
| **建议** | **建议**在规范 04 补一句判据口径定义，并**建议**默认空白敏感（逐字），因为索引的价值就是逐字可解析；否则一条索引错误可以靠「排版差异」永久豁免。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U54 · 同一 `SCI-PHOT-001` 在两份追溯台账锚到不同测试载体

| 字段 | 内容 |
|---|---|
| **事项** | 需求级台账与模块级台账把同一个 `SCI-PHOT-001` 锚到两个不同的测试载体（两者都真实存在）。 |
| **分歧** | **需统一**：`docs/TRACEABILITY.csv` 锚 `lib/algorithms/photometry/cpp/test/test_spectrum_integrator.cpp`；`docs/traceability/TRACEABILITY_MATRIX.csv` 锚 `tests/p1phot`。**不必统一**：审稿人已核实两个载体都真实存在，故不构成悬空，也不足以推翻 `TRACEABILITY_SPEC.md:117-118` 的「二者互补不冲突」—— 两份台账粒度不同（16 列 67 行以 `requirement_id` 为键 vs 24 列 30 行以 `module_id` 为键，交集为 0）。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §3 M-14 残余观察（:216-218）、§4 R01-C3（:275）。仓内：`docs/TRACEABILITY.csv`、`docs/traceability/TRACEABILITY_MATRIX.csv`、`docs/engineering/TRACEABILITY_SPEC.md:93,:117-118`、`lib/algorithms/photometry/cpp/test/test_spectrum_integrator.cpp`、`tests/p1phot`。 |
| **核证** | ✔已核证：`lib/algorithms/photometry/cpp/test/test_spectrum_integrator.cpp` 存在（源件实测 9355 字节）；`docs/engineering/TRACEABILITY_SPEC.md:93` 确把 docs 内旧目录当作合法在位者（与 U1 的规范 01:46 冲突）。 |
| **影响** | 只影响追溯台账的锚点口径一致性，不影响 R01-C3 的「非双正本」结论。**注意** `TRACEABILITY_SPEC.md:93` 的旧目录认定并入 U1。 |
| **所需输入** | 负责人裁定锚点粒度口径（一个需求是否允许多个测试载体）。 |
| **建议** | **建议**允许不同载体并在两份台账各加一列「本层的锚点粒度」，明确二者互补而非冲突 —— 这样既不必强行统一，也把口径写明。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U55 · 435 条 C4 红的分布未核证

| 字段 | 内容 |
|---|---|
| **事项** | 前台给出 435 条 C4 红并作了四类分布，其中只有一项被复核，其余全部未核。 |
| **分歧** | **已核（1/4）**：审稿人只复核「索引侧 0」这一项 —— 258 处逐字比对，253 处实质相符、0 处实质不符、1 处空白级（U53），结论一致。**未核（3/4）**：435 总数与「抬头 137 / 同源 88 / 正文散引 210」三项分布，审稿人明确写「未复核——分布统计超出本轮结构与索引域，且需机器扫描」，§7 亦声明未越界处理。 |
| **出处** | 源件：`审稿-RR01-结构与索引.md` §5 盲复算表末行（:311）、§4 R01-C5（:284-289）、§7（:393）。 |
| **核证** | 转引。 |
| **影响** | 若 435 的分布不准，前台按「抬头/正文」类别分派轮次的依据会错位 —— 即**工作量分配依赖一个未核的数字**。 |
| **所需输入** | 需机器扫描；规范 04 §1 禁止以脚本判定替代阅读，故须由前台在允许机器扫描的轮次执行。 |
| **建议** | **建议**由前台统一机器扫描一次并出分布清单。这与 U56 是同一类问题（索引面的机器覆盖缺口），宜合并做。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U56 · `DOCUMENT_INDEX.yaml`（1085 行）未逐行核对文件树闭合性

| 字段 | 内容 |
|---|---|
| **事项** | 权威顶点声明的「未登记或登记不存在均为缺陷」这条硬条件，本轮**没有验证**，而该面按标准「完全由审查兜住」—— 本轮自陈没兜住。 |
| **分歧** | **应当闭合**：`docs/ACSD_DESIGN.md:28` 是无条件硬条件。**本轮未兜住**：`审稿-RR09` §5 Q11 把「文档索引与文件树闭合是否被判红」列为未验证项；`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:424` 明列该面「没有机器执行面，正确性完全由审查兜住」——而 R01/R09 两轮都没有逐行兜住。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §9（:615）、§5 Q11（:371）、§2.5（:101-115）、§8.7（:552-566）。仓内：`docs/DOCUMENT_INDEX.yaml`、`docs/ACSD_DESIGN.md:28`、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:424`。 |
| **核证** | ✔已核证：`docs/DOCUMENT_INDEX.yaml` 实测 **73,901 字节**；`docs/ACSD_DESIGN.md:28` 确含该硬条件；`VALIDATION_EVIDENCE_STANDARD.md:424` 确含「没有机器执行面」的原话。 |
| **影响** | **文档索引面的正确性目前没有任何保障** —— 既无机器门（U29/U30），本轮审查也没逐行兜。这是 R01 与 R09 两轮共同暴露的**体系性缺口**，也是 U52（README 豁免口径）与 U55（435 条分布）的前置问题。 |
| **所需输入** | 负责人决定：是补一次逐行核对（本轮未做）、还是给索引面建机器门（与 U29 的裁结果绑定）。 |
| **建议** | **建议**优先建结构门（与 U28 同一方向：只校验 id/path/upstream 键的唯一性与存在性，不判内容）。理由是：73KB 的索引靠人眼逐行核对在下一轮必然再漏，而结构面完全可以机械化。**这三条（U28/U29/U56）应打包成一个「文档索引机器门」决策一起裁。** |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U57 · 本审核包统一使用 `文件:行`，与 `DOCUMENT_GOVERNANCE.md:47` 冲突

| 字段 | 内容 |
|---|---|
| **事项** | 仓内现行条款禁止文档内留指向仓内文档的行锚，而本审核包（含本件）全部使用 `文件:行`。 |
| **分歧** | **禁止**：`docs/engineering/DOCUMENT_GOVERNANCE.md:47`（§3「锚纪律与机器门」）规定「文档内不留指向仓内文档的行锚」，引用本仓其他文档一律用内容锚或 §条。**本包的做法**：按前台明确交办使用 `文件:行`；审稿人自解为「本件落在 `run/` 下（非正式文档层），不违反该条款」，但加了条件句「若本件内容将来被搬进 `docs/` 则违反」。 |
| **出处** | 源件：`审稿-RR09-跨文档一致性.md` §9（:616）。仓内：`docs/engineering/DOCUMENT_GOVERNANCE.md:47`。 |
| **核证** | ✔已核证：`docs/engineering/DOCUMENT_GOVERNANCE.md` 在位，`:6` 已见（前引 U30）。`:47` 本次未逐字读，标转引。 |
| **影响** | **本件沿用同一风格**，故该口径问题同样适用于本件。若负责人决定把审核包内容搬进 `docs/`，本台账的全部 `文件:行` 都要重写。**提前登记可避免届时返工。** |
| **所需输入** | 负责人裁：`run/` 下的审核包是否豁免该条款；若不豁免，是否允许审核包专用一种「带基线 commit 的行锚」形式。 |
| **建议** | **建议**允许审核包使用**带基线 commit 的行锚**（如 `ACSD_DESIGN.md:124@1208d5a0`）—— 这样既保留可核性，又不与「文档内不留行锚」的初衷冲突（该条款的初衷是防行号漂移，带 commit 正好解决漂移）。本件已记录文首偏差登记，动机与该建议一致。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U58 · 墙钟字段豁免口径是否被接受为「可复现」判据

| 字段 | 内容 |
|---|---|
| **事项** | `实验/README.md:12` 自定的可复现判据把墙钟字段列入白名单，与规范 03 §6 的字面冲突。 |
| **分歧** | **可接受**：实验侧白名单 5 个墙钟名、其余确定性字段逐位相同。**字面冲突**：规范 03 §6 的字面要求不含该豁免。**且 R08 反例 R3 证伪了可执行性**：白名单用精确匹配，**可被一个字段名击穿**（多一个或少一个字段名即失效，而字段名有 7 处漏项）。 |
| **出处** | 源件：`G08-04-实验单元侦察.md` §12 U-2（:746 行表内第 2 行）；`审稿-RR08-实验与复现.md` §4 反例 R3（:308-315）、§3 S-2（:431-436）。仓内：`实验/README.md:12`、`实验/absolute-snr/README.md`。 |
| **核证** | 转引。 |
| **影响** | 判据若是「非偏松而是**不可执行**」，则所有据此宣称可复现的单元结论都要重判。 |
| **所需输入** | 负责人裁决并写入验收记录。 |
| **建议** | **建议**把白名单改成**前缀匹配 + 通配**（如 `*.wall_*`），并把「白名单本身是判据输入面」这件事写进实验 README —— 字段名硬编码是典型的「判据与被测对象耦合」。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U59 · GA-05「各自 Poisson」是否为硬性字面要求

| 字段 | 内容 |
|---|---|
| **事项** | 裁决条款要求「源、天光、暗电流**各自** Poisson」，实现是「求和后单次抽样」，统计等价但字面不符。 |
| **分歧** | **字面合规要求**：GA-05（`实验/裁决台账.md:308-315`）逐字「必须模拟噪声物理过程…源、天光、暗电流**各自** Poisson（电子域）」。**统计等价**：实现为「求和后单次抽样」，两者统计上等价。 |
| **出处** | 源件：`G08-04-实验单元侦察.md` §12 U-3（:746 行表内第 3 行）。仓内：`实验/裁决台账.md:308-315`（GA-05）、`docs/science/NOISE_MODEL.md`。 |
| **核证** | ✔已核证：GA-05 原文在 `实验/裁决台账.md:308-315`（本件已完整读过该节）。 |
| **影响** | 判字面合规 ⇒ 所有合成实验都要改实现；判统计等价 ⇒ 改文档一个字。影响面覆盖全部合成/注入类实验。 |
| **所需输入** | 负责人裁决：字面合规 or 订正文档。 |
| **建议** | **建议**订正文档为「等价口径：电子域合并抽样与各分量独立抽样统计等价，但合成必须给出各分量的期望率」—— 理由是 GA-05 的**意图**（不能算术地加常数代替加天光）已被满足，逐 Poisson 只是实现细节；而强制改实现会触及所有合成实验且不增加任何判别力。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U60 · `m16_sampling` 默认场景天空梯度为零是否为缺陷

| 字段 | 内容 |
|---|---|
| **事项** | 默认档的天空梯度为零，只有 11 个场景档才启用。 |
| **分歧** | **不是缺陷**：环节 6 已实现、11 个场景已启用，默认档为零只是默认值选择。**是缺陷**：默认档正是绝大多数复现会跑的那一档，若它的天空梯度为零，则 U14（apex 无条件单调性）的失效域恰好是默认档。 |
| **出处** | 源件：`G08-04-实验单元侦察.md` §12 U-4（:746 行表内第 4 行）。仓内：`实验/additive-sky-seamless/code/reverse_verify/m16_sampling/`、`实验/absolute-snr/docs/frame-snr-canon.md:17,:215-217`。 |
| **核证** | 转引。 |
| **影响** | 与 U14 联动：若 apex 的单调性失效域是「结构主导」，而默认场景的天空梯度为零，则默认档**不覆盖**失效域 —— 即默认复现跑出来的绿灯不能验证 apex 断言。 |
| **所需输入** | 负责人裁决默认档是否应带非零梯度。 |
| **建议** | **建议**默认档改为带一个**温和但非零**的梯度（并在场景表里标出），理由是零梯度默认会让所有默认复现都落在「最容易通过」的区间 —— 这是「默认取最易过」这一反模式的典型形态。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-05-U61 · 掩膜缺失时静默降级为全像素有效，是否为可接受行为

| 字段 | 内容 |
|---|---|
| **事项** | 掩膜缺失时当前只 WARNING、不 fail-closed，且掩膜目录在当前树里不存在。 |
| **分歧** | **可接受**：现状是 WARNING + 降级为全像素有效。**不可接受**：依 `AGENTS.md` §8「不以 waiver 静默覆盖红灯；判据须在正确实现下绿、注入缺陷时红」，静默降级会让缺掩膜的运行**读数变好但真实性变差**；且 `实验/*/run/reverse_verify/m16_scene/masks/` 在当前树不存在。 |
| **出处** | 源件：`G08-04-实验单元侦察.md` §12 U-5（:746 行表内第 5 行）。仓内：`实验/additive-sky-seamless/code/reverse_verify/m16_scene/masks/`（不存在）、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md`。 |
| **核证** | 转引。 |
| **影响** | 与 U27（`acs-stage2` 的 `is_process_exit_code` 闸不产生 final 事件）是**同一形态**：**判据存在但不覆盖真实路径**。降级路径一旦被误用，判据读数无意义。 |
| **所需输入** | 负责人确认该目录是否应入库，或确认降级为既定口径（若确认，须在 README 显式写明降级读数不可作为证据）。 |
| **建议** | **建议**改 fail-closed（与 `run_all.sh:56-57` 对 V6/V10 的处理保持一致 —— 那条已经是「记红、不静默跳过、不设 waiver」，是正确的范式）。同一仓内两种相反做法，统一到后者。 |
| **阻塞级别** | 不阻塞但须登记 |

---

## 四、其余待裁决事项（合并登记，均需口径裁定）

以下条目来自各执行件/复核件的「移交车道 / 待裁决清单」表格，性质明确、来源清楚，但**不单独立条**（它们多数是「按清单执行」而非「双方对立待裁」）。**本件不代裁**，登记在此供负责人一次性知悉与派单。

| # | 事项 | 分歧/缺口 | 出处 | 所需输入 | 级别 |
|---|---|---|---|---|---|
| a | shared 域归属：4 份后继正本是否计入 | 属合同域范围决策 | `G08-03-整改B-正本唯一性.md` §6 第 6 项（:241） | 负责人裁归属 | 不阻塞但须登记 |
| b | `acsd-stage2` 裸整数退出码与 `acsd::ExitCode` 重叠 | 根因在代码；本单不改代码不编译 | 同上 §6 第 7 项 | 并入 U27 | 阻断提交 |
| c | `CHK-VERSION-CONSISTENCY-VER001` 声明存在但无实现 | `eng/ci/` 已退场，无执行面 | 同上 §6 第 8 项 | 并入 U20 | 阻断提交 |
| d | `verify_toolchain.py` 声明存在但无实现 | 禁 MinGW 约束现无机器执行 | 同上 §6 第 9 项 | 并入 U20 | 阻断提交 |
| e | 溯源台账数量 31/18/13 vs 30/27/3 两套并存且均不可复现 | 原件已随 `run/` 永久丢失 | `审稿-RR04` §6 复核表第 2 行（:299）、§3 S3（:174）；`UNRESOLVED_REGISTER.md:104` | 负责人决定重跑 inventory 或整组标「不可复现、待重测」 | 不阻塞但须登记 |
| f | statsmodels commit 未落地（实验侧用 `/main/` 无版本） | 正本已钉 tag + 2 commit 并明令「必须带 commit」，实验侧未执行 | `审稿-RR02` §2（:132）、§7（:349）；`docs/science/algorithms/PHOTOMETRIC_FIT.md:301`、`实验/photometric-magnitude/refs.md:17` | 实验侧补 commit | 不阻塞但须登记 |
| g | P5 阻断-5/阻断-6 的修法二择一 | 补正本适用域（文档动作）vs 改判决量为斜坡不敏感量并沿边提高采样（代码动作） | `审稿-RR07` §9.3（:491） | 随 U9 一并裁 | 阻断下一任务 |
| h | P5 残余接缝过渡宽度 `w` 的新夹具未做 | 待办；本轮按纪律未跑实验 | `审稿-RR07` §9.4（:492）；`实验/additive-sky-seamless/README.md:386-390` | 前台执行 | 不阻塞但须登记 |
| i | R09 其余常数/口径未闭：NSIDE 口径 `2^18` vs order 9（差 10.837 dex）、drizzle 方差分母口径（`Σv·w_jp²/D_p²` vs `pixfrac⁻⁴` 修正，自陈偏低 2.44×）、§5.3「含拟合参数协方差的残差方差」在生产分支变成纯 `1/Σivar`、精度默认值三处互斥（单精度/FP64/FP32） | 源件标「未复核」，方向可信但本轮无一手证据 | `审稿-RR09` §9（:607-610）、§2.4（:99）；`docs/engineering/NUMERIC_STANDARD.md:10,:59,:87`、`docs/science/DATA_SEMANTICS.md:43`、`docs/ACSD_DESIGN.md:191,:330`、`docs/detail/common.md:26` | 补一次逐条复核后交裁 | 阻断下一任务 |
| j | R09 `S-13`/`R4'` 的**科学侧未闭**：`CONTROL_WEIGHT_SNR.md`、`INTEGRATION.md`、`UNCERTAINTY_AND_COVARIANCE.md` 未读 | 源件自陈「本轮最需要补做的一条」，只能确认文档层存在口径分叉，不能断言科学正本选了哪个 | `审稿-RR09` §4 R4'（:338）、§9（:612）、§3.2 S-13（:238） | 补读三份科学正本 | 阻断下一任务（与 U5/U8/U9 同批） |
| k | 十轮结论**零实测** | 全部为静态取证（`read`/`grep`/`sed`/`test -e`/纯算术），没有一条经运行确认；`B2` 的「`main.cpp:391` 返回 27」是代码路径推断而非运行观测 | `审稿-RR09` §9（:614）、§8（:498-598） | 前台统一执行编译/运行验证 | 不阻塞但须登记（**但它限制了本台账全部结论的强度**） |
| l | `docs/science/experiments/`（规范 01 §1 列出）不存在 | 建 vs 改规范；仓根 `实验/` 有 9 个单元，索引 `:26-27` 的扫描口径显式排除 `实验/`，全索引指向 `实验/` 的仅 `:37` 一条 | `G08-03-结构侦察.md` §10 U4；`审稿-RR01` §3 M-8（:177-181）；`docs/DOCUMENT_INDEX.yaml:26-27,:37` | 负责人裁建/改 | 阻断提交（并入 U1） |

---

## 五、登记面本身的待办（不是裁决项）

以下不是「待裁决」，而是**台账维护面**的待办，登记以免遗失：

1. **`docs/engineering/UNRESOLVED_REGISTER.md` 的归属与滞后**：该文件 **4106 行、80 个二级章节**，是仓内最大的治理过程台账。它**不在** `DOCUMENT_GOVERNANCE.md` §7.1 的封闭豁免面内（豁免面只列 `docs/research/**`、`docs/references/**`、`artifacts/**`、`实验/**/results/**`、退役件失效标注区、§7.2 围栏块），而形态体检判它应属过程层、原命中 361 行（20%）+ 13 处确认违规，**建议迁出**。同时它已出现**滞后**（裁-24 已被改过而登记未更新，见 U30）。**裁-24 与本项合并处理。**
2. **`审稿-RR01` §3 的编号体系冲突**：§3 的 `M-1…M-16`（须修）与 `S-1…S-4`（建议），同件 §6 的 `S1…S4` 是**子代理编号**，同形不同物；§6 S4 回报用的 `B1/B2/B4/B5/B7/M3` 与 §3 的 `B-4/B-6/B-5/M-14/M-15/M-16` 是映射关系。**建台账时必须加前缀区分**，否则按编号对齐会全错。
3. **已撤回/改判条目须保留痕迹**：R01 `M-14`【已撤回】、R07 §4 B4 撤回段（争议③原判定作废）、R03 `CE-7`/`CE-8` 对子代理判词的撤回改判、R05 `X-3` 构造失败、R06 `S1` 的 2 条否决、R09 `Q12` 审稿人自撤、R10 两条自纠。**这些是「已否证」而非「未审」，不应在下一轮被重新提出。**

---

**本件编制纪律声明**：全程只读仓内；仅新建 `run/GOVERN-08/审核包-R2/UNRESOLVED.md` 与 `run/GOVERN-08/审核包-R2/审稿-总账.md` 两个文件（`run/` 在 `.gitignore` 内，不出现在 `git status`）；无 git 写（未 add / commit / checkout / reset / stash）；未编译、未跑测试、未跑构建、未联网；未替负责人做任何裁决，「建议」列全部标明为建议。

---
---

# 六、G08-01–G08-04 车道补充台账（本车道，编制于 2026-10-02 05:5x）

## 6.0 本节说明（**先读**）

### 6.0.1 为什么是「追加」而不是「重写」

本车道开工时（2026-10-01 21:39）本文件**不存在**；编制途中（2026-10-02 01:21:36）另一车道已先行落盘 889 行、61 条正文的 **G08-05 待裁决台账**。该台账基于**更晚**的源材料（`审稿-RR01`–`RR10`、`G08-05-整改-*`、`00-索引.md` 等，mtime 00:16–05:45），证据更全。

按 `AGENTS.md §7`「后到者得知先到者已落内容，**不回滚覆盖**」，本节**只追加，不改动第一至五节一字**。第一至五节的 `G08-05-U*` 编号继续有效；本节用 `G08-01-U*` / `G08-04-U*` / `6.x` 编号，不与前者冲突。

### 6.0.2 本节收录什么、不收录什么

| 收录 | 不收录 |
|---|---|
| ① 第一至五节**完全未收录**的 G08-01 删门禁线（8 条）；<br>② 后续车道**已推翻或已关闭**、但第一至五节仍当开放项的条目；<br>③ 两个**整节缺失**的部分：「前台已代为裁决的事项」与「建议裁决顺序」 | 已由第一至五节立条且证据更强的条目（逐条比对后，本车道起草的 60 条中约 35 条已被 U1–U61 覆盖或超越，故不重复立条，只在 §6.1 标注对应关系与差异） |

### 6.0.3 复核时刻与纪律

- 复核时刻 **2026-10-02 05:46–05:50**，仓内 HEAD **`ef3dc516`**（**已不是**本车道开工时的 `10aa5490`——G08-05 各车道已多次提交）。
- 本节所有 `文件:行` 均在该 HEAD 下**亲自实测**。凡本车道在 `10aa5490` 下测得、且在 `ef3dc516` 下已变的，一律在 §6.1 显式标注，不沿用旧值。
- 本车道未改仓内任何文件；无 git 写；未编译、未跑测试。仅追加本文件（原 889 行完整保留，已备份 `/tmp/UNRESOLVED.bak`）。

---

## 6.1 复核时刻的状态变化（**负责人请先看这张表**）

本车道开工时排出的前 5 优先项，到编制本节时**已有变动**：

| 原排序 | 事项 | 编制本节时的实测状态 | 结论 |
|---|---|---|---|
| **1** | `closure_metric.py` 身份拆分 | **仍成立且仍阻断**：`p1wcs_closure_metric_gate` 全仓仍 **3 处**引用（`docs/science/ASTROMETRY.md:281`、`docs/science/algorithms/GATES_AND_TOLERANCES.md:67` **末列仍为 `Y`**、`lib/algorithms/platesolve/memory.md:111`）；`add_test` 指向该件仍为 **0**（仅 `eng/tests/unit/p1wcs/CMakeLists.txt:187` 一条注释）。**且第一至五节完全没有收录此条** | ⬆ **升至第 1 优先**，本节立为 `G08-01-U1` |
| **2** | 最高设计 §3.1 只列 11 个 / 下游声明 13 个（权威顶点断链） | ✅ **已被后续车道关闭**：`docs/ACSD_DESIGN.md:177` 现逐字为「现行统一数据对象集共 **13** 个：signal、variance、ivar、**source_snr**、depth_m5、frame_snr、**point_information**、sparse_snr_layer、support、coverage、validity、rejection、provenance」——两个缺失对象已补入顶点 | ⬇ **降级**，但第一至五节的 `G08-05-U3` 标题仍写「顶点追溯断链」，**建议负责人按本节订正该条状态**（见 §6.1 注） |
| **3** | 门禁占位符 140 处 / 门禁登记面落地时点 | 第一至五节已立 `G08-05-U20`，本车道不重复立条 | ➡ 维持 |
| **4** | V6/V10 依赖 3×268 MB 模板 ⇒ 干净 clone 必红 | 第一至五节已立 `G08-05-U25`，本车道不重复立条 | ➡ 维持 |
| **5** | 三条实验单元与正本科学冲突 | 第一至五节已立 `G08-05-U12/U13/U14`（apex 侧）与 §4 合并项，证据更全 | ➡ 维持，但注意 `G08-04-U2`（`k_gauss`）在 `审稿-RR07 §5 B2` 已有 16 位机器精度闭合复算，**可能可直接关闭** |

> **§6.1 注（关于 `G08-05-U3`）**：本车道只核到「§3.1 已补为 13 个」这一步。`G08-05-U3` 标题所称「13 / 14 / 附录A 10 三数并存」中的**「14」与「附录A 10」两说本车道未核**——这两说若仍成立，U3 仍需负责人裁，但**断链**这一具体缺陷已消解。请勿据本节把 U3 整条视为关闭。

> **§6.1 附（G08-05-U18 模块 ID）**：该条已做 `MODULE_MAP.yaml` **逐块解析**（26 个 `- id:` 条目、23 个具 `module_id`、`registry/` 25 张卡），**严格优于**本车道用正则得到的 23/23。本车道据此**撤回**自己的一组计数，不与 U18 竞争。

---

## 6.2 新增待裁决条目（G08-01 删门禁线，第一至五节未收录）

### G08-01-U1 · `closure_metric.py` 的身份拆分 —— 发布门 = Y 却零执行面 🔴 **第一至五节未收录**

| 字段 | 内容 |
|---|---|
| **事项** | 该件同时是 SCI-WCS-001 的「唯一可执行实现」和一条**发布门 = Y** 的门。删其 ctest 注册、留实现，是否成立？ |
| **分歧** | **执行件**（`G08-01-整改执行.md:170`）：它是「唯一可执行实现」，删会打断 SCI-WCS-001 ⇒ 保留实现、删 ctest 注册；并主动登记「拆分口径需负责人确认」（`:334`）。<br>**复核件**（`复核-G08-01-整改.md §7.5`）：**拆口径不成立**，三条理由——① 正本定性被截断：`ASTROMETRY.md:209` 同一引文块紧接着写「`check` 判门」，一级科学正本里的身份是「实现 + 门」；② `GATES_AND_TOLERANCES.md:67` 把它登记为 `ctest:p1wcs_closure_metric_gate`、**发布门 = Y**（同表 `:66` 的基础门反写「不可复现，不作门」、发布门 = N）；③ 拆分后两头落空。复核明写「**现状是三者中唯一自相矛盾的选项**」（`:344`）。 |
| **出处** | 执行 `G08-01-整改执行.md:170`、`:334`；复核 `复核-G08-01-整改.md:289-344`。**本车道实测（HEAD `ef3dc516`）**：3 处悬空指针**仍在**，发布门**仍为 `Y``，执行面**仍为 0**。 |
| **影响** | 3 个文件持有指向 0 执行面的门号指针，其中 1 处明确标「发布门 = Y」。若照现状发布，**发布门 Y 无任何机器验证**，违反规范 08 §4「恒真、空断言、不读真实对象的判据无效」。 |
| **所需输入** | 负责人二选一：**(甲)** 恢复 `p1wcs_closure_metric_gate` 注册（复核称其 selftest 含 7 类负例注入必红 + 缺输入 fail-closed），并同步订正 `ASTROMETRY.md:281` / `GATES_AND_TOLERANCES.md:67`；**(乙)** 连实现一并删并在 `docs/` 车道废止 SCI-WCS-001 的可执行面声明；**(丙)** 维持现状、显式登记「发布门 Y 当前无执行面，G08-10 承接」。 |
| **建议** | **建议采 (甲)**。理由：前台原裁（§6.4 P-2）所引的正本依据已被复核当场证伪（引文截断），而「登记 G08-10 承接」并未消除「发布门 Y 零验证」这一硬伤——G08-10 未落地前它是一条**假绿发布门**。若坚持 (丙)，至少须把 `GATES_AND_TOLERANCES.md:67` 的发布门由 `Y` 改 `N` 并注明未实现。 |
| **阻塞级别** | **阻断提交** |

### G08-01-U2 · `runtime_oracle.py` 是否部分回退 ＋ **本车道新发现的文档侧悬空**

| 字段 | 内容 |
|---|---|
| **事项** | 整件删除是否正确？以及它删完后，文档侧留了什么？ |
| **分歧** | **执行件 R2**（`G08-01-整改执行R2.md:482`）：该件有 **R1–R8 共 8 条规则，仅 R7 判据对象消失** ⇒「**正确拆法应如此**（删 R7、留 R1–R6/R8）」，但任务书明令本轮不得回退前一轮改动，故登记待裁。<br>**复核件**（`复核-G08-01-整改.md:356`）：整件删除「结论可接受，**跳跃过快**」——连带抹掉另外 7 条独立结构规则的唯一 oracle 覆盖，判「**需登记**」。 |
| **出处** | `G08-01-整改执行R2.md §5`（`:380-389` 逐条列 R1–R8 存活性）、`:425`、`:482`；`复核-G08-01-整改.md:356`。规则实现行号：`git show <基线>:eng/tools/quality/runtime_oracle.py:85,151,206,254,293,326,409`。 |
| **🔴 本车道新发现（HEAD `ef3dc516`）** | 文件确已删除（`ls eng/tools/quality/runtime_oracle.py` → 不存在），**但 `docs/engineering/EXECUTION_MODEL.md:140` 仍逐字引用它，并断言「⇒ 在役 CI 面」**：<br>`` - `lib/infrastructure/scheduler/budget.py`：由运行闭包判据调用其 `selftest`、由 `eng/tools/quality/runtime_oracle.py` 锚定（运行闭包判据载体见 门禁注册面（G08-10 重建））⇒ **在役 CI 面**。 ``<br>⇒ **删件动作在文档侧留了一处新悬空**，且该悬空**带「在役」断言**，比普通死名更危险。此项在 `G08-01-整改执行R2.md` 的移交清单中未见登记。 |
| **影响** | 7 条规则判据能力归零（判据对象**全部存活**，故属误删在役覆盖而非退役门）+ **新增 1 处文档侧悬空**。 |
| **所需输入** | 负责人决定：(甲) 在 G08-10 重建这 7 条覆盖；(乙) 接受覆盖归零并登记为已知缺口。**另需决定** `EXECUTION_MODEL.md:140` 的订正归属（该件 mtime 17:12:56，属 G08-03 车道写面，非 G08-01）。 |
| **建议** | **建议 (甲)**，并把 `EXECUTION_MODEL.md:140` 的订正**立即并入 G08-03 车道下一次写入**——它带「在役」断言，是典型的会误导后续判断的死名。建议不要留到 G08-10：文档侧的死名不会因为门禁重建而自动变真。 |
| **阻塞级别** | 阻断下一任务 |

### G08-01-U3 · `eng/tools/fixtures/README.md` 去留 —— **已实际落地，待负责人追认**

| 字段 | 内容 |
|---|---|
| **事项** | 任务书只给「改指或加注」两选项，执行件判「删件」，登记待裁。 |
| **分歧** | **执行件 R2**（`G08-01-整改执行R2.md:481`）：该 JSON 本轮删除后目录只剩这一个 README，且 `grep -rn "tools/fixtures"` 在 `CMakeLists.txt`/`eng/`/`lib/`/`docs/` **零命中** ⇒ 判删件；另一选项是恢复并写「本目录暂无内容」。 |
| **出处** | `G08-01-整改执行R2.md:481`、`:204`、`:230`；`复核-G08-01-整改.md:245`（该 README 属 `eng/**` 在范围内，复核未反对）。 |
| **本车道实测（HEAD `ef3dc516`）** | `ls -d eng/tools/fixtures` → **目录已不存在**，删件已生效且未见残留悬空。 |
| **影响** | 1 个文件 + 1 个空目录，已实际消失。 |
| **所需输入** | 负责人**追认**该处置（已成事实），或指示恢复。 |
| **建议** | **建议追认**——零引用已实测，且保留空目录 README 本身违反 `AGENTS.md §5`「每个文件夹内放一个极简 README，说明该文件夹的内容」。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-01-U4 · `sha256_utf8.py` 的归属 —— **仍未处置**

| 字段 | 内容 |
|---|---|
| **事项** | 位于 `lib/infrastructure/acr/ci/`，随 `eng/ci` 退役还是长期保留？ |
| **分歧** | **执行件**（`G08-01-整改执行.md §6.2 #5`）：读原文后**判定为实现** ⇒ 保留并顺手修了它的两处恒真；但若负责人认为该目录整体应随 `eng/ci` 一并退役，需另行裁决。<br>**复核件**（`复核-G08-01-整改.md:365`）：**实跑复现**改前 `generate` 读盘失败 `rc=0`、`generate-and-verify` 打印 `ALL PASSED` 而 `rc=0`；改后两者 `rc=1`；无错树 `rc=0` ⇒ **修复真实有效**，认可。 |
| **出处** | `G08-01-整改执行.md:233`；`复核-G08-01-整改.md:365`、`:416`（无活代码引用，仅文档提及）。 |
| **本车道实测（HEAD `ef3dc516`）** | 文件**仍在**：`./lib/infrastructure/acr/ci/sha256_utf8.py`。 |
| **影响** | 0 个代码消费者，删不删都不影响构建。 |
| **所需输入** | 负责人一句「保留」或「随目录退役」。 |
| **建议** | **建议保留**——两侧证据一致显示它是真实现且修复有效，删除会丢掉一个已验证的真判据。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-01-U5 · `eng/ci/` 缺席 ⇒ 全仓无门禁、无 CI 基线

| 字段 | 内容 |
|---|---|
| **事项** | 是否接受「零机器执行面」作为本轮交付形态？ |
| **分歧** | **执行件**（`G08-01-整改执行.md:234`）：引仓内自陈「`eng/ci/` 不存在 ⇒ 所有判据即使修好也不会被执行 ⇒ `eng/tools/` 下 **17 个工具全部无自动执行面**」；据此**不保留任何在跑门禁**，交付件意义上即「无门禁、无 CI 基线」，重建归 G08-10。<br>**复核件**：未推翻（`复核-G08-01-整改.md §8.1` 认可「无 ctest 目标指向已删文件」成立）。 |
| **出处** | `G08-01-整改执行.md:234`。⚠ 其转引的 `docs/engineering/UNRESOLVED_REGISTER.md:3117-3129` 本车道**未逐行读**，列为待核实（§6.6 V-1）。 |
| **影响** | 规范 08 §6 完成判据要求「CI/门禁依据定稿文档重建并 GitHub 全绿」——该判据**当前不成立**。与 `G08-01-U1` 互为因果：`closure_metric` 的发布门之所以零验证，根源即在此。 |
| **所需输入** | 负责人确认：本轮是否接受以「零门禁」进入后续任务，还是必须先落 G08-10。 |
| **建议** | **建议接受**作为两段式时序的中间态，但**须在交付件与发布材料中显式写明「当前零机器执行面」**，避免下游误读为「已全绿」。**这一条与 §6.4 P-2 直接相关**：正因本条未定，`closure_metric` 的「登记 G08-10 承接」才是个空头承诺。 |
| **阻塞级别** | **阻断提交**（对发布而言） |

### G08-01-U6 · 文档宣称已删门禁在役，且复核指出登记不全

| 字段 | 内容 |
|---|---|
| **事项** | 约 20 处文档仍宣称已删门禁在役；复核另查出 11 处未登记。 |
| **分歧** | **执行件**（`G08-01-整改执行.md:232`）：登记约 20 处，含 `docs/science/*.md` 10 处 `science_contract_lint.py PASS`、10 处 `docs_machine_consistency.py PASS`（含 `build_v19r3_package.py:212` 的 `8/8 PASS` 与 `build_v19r4_package.py:129` 的 `9/9` **硬编码假绿文本**）；`docs/` 非本车道，只登记不修改。<br>**复核件**（`复核-G08-01-整改.md:359`）：该登记「**全部限于 `docs/`**」；另有 **8 处落在本单自己可写范围**（`eng/**`、`lib/infrastructure/acr/**`）内被**静默略过**，另有 **3 处**在范围外也未登记；判「订正（登记不全）」。 |
| **出处** | `G08-01-整改执行.md:232`；`复核-G08-01-整改.md:359`（其 §6.3(i)/(ii)）。 |
| **本车道实测（HEAD `ef3dc516`）** | `EXECUTION_MODEL.md:140` 的「**在役 CI 面**」断言**仍在**（见 `G08-01-U2` 的新发现），可作该条未结的一个现证。 |
| **影响** | 假绿文本与「在役」断言进入仓内，任何人照文档跑命令都会拿到「PASS」而无门可跑。 |
| **所需输入** | 负责人确认清理范围与车道，特别是那 8 处 `eng/**` 是否由 G08-01 车道返工。 |
| **建议** | **建议**把 8 处 `eng/**` 补入 G08-01 车道返工（同车道同写域，返工成本最低），3 处范围外另登记。 |
| **阻塞级别** | **阻断提交** |

### G08-01-U7 · 悬空引用终点数字分歧（8 条 vs 19 条/10 文件）

| 字段 | 内容 |
|---|---|
| **事项** | G08-01 整改后残留悬空，是 8 条还是 19 条？ |
| **分歧** | **执行件**：「悬空 53 → **8**（剩 8 条全在 2 个文件）」。<br>**复核件**（`复核-G08-01-整改.md:352`）：同方法学独立复算 = **19 条 / 10 文件**，多出 11 条逐条列于其 §3.3；根因是执行件扫描器把整串 `ast.Constant` 拿去 `normpath`，看不见「命令前缀 + 路径」内嵌串（如 `"py -3.12 eng/tools/x.py"`）。判「**推翻**（终点数字错；不构成 >53 的阻断）」。 |
| **出处** | `复核-G08-01-整改.md:352`、`:98`（漏报根因）；执行件 R2 `:144-146`。 |
| **影响** | 两者都认为方向已收敛（53+ → 两位数），不构成 >53 级阻断；但「已清干净」与「还剩 19 条」差一个量级，直接影响是否收口。 |
| **所需输入** | 负责人采信哪一方数字；若采信 19，确认新增 11 条的归属车道。 |
| **建议** | **建议采信复核的 19 条**——它给出了可复现的漏报根因与逐条清单，而执行件未对其扫描器盲区作正面回应。注：复核同时认可「0 个 ctest 目标指向已删文件」（`:367`）。 |
| **阻塞级别** | 不阻塞但须登记 |

### G08-01-U8 · 版本一致性判据缺口（T04/T05）无机器承接 ＋ `verify_toolchain.py` 无实现

| 字段 | 内容 |
|---|---|
| **事项** | 三条声明存在、无实现面的判据，谁承接？ |
| **分歧** | **执行件**（`G08-01-整改执行.md:230`）：删掉的 T04/T05 与 5 个类原本覆盖「版本口径在文档/生命周期表/外部工具版本上的区分」，随门禁删除后**该口径暂无机器判据**。<br>**整改 B**（`:252`/`:253`）：`CHK-VERSION-CONSISTENCY-VER001` 声明存在但无实现（`VERSIONING.md` 只读、`eng/ci/` 已退场）；`verify_toolchain.py` 声明存在但无实现 ⇒「禁 MSYS2/MinGW 依赖面」这条硬约束现无机器执行。<br>**复核**（`复核-G08-03-整改B.md:155`）：版本一致性门的事实为真，但**已由基线层自己登记为「待 G08-10 重建」**，执行件当新缺陷上报属「重复登记」。<br>**整改 B 另给订正**（`:259-263`）：前台情报「`DEPENDENCY_RULES.md:31` 与 MinGW 无关」**不成立**——该行**逐字包含**「MSYS2/MinGW 依赖面禁止」，可作辅证。 |
| **出处** | `G08-01-整改执行.md:230`；`G08-03-整改B-正本唯一性.md:252-253`、`:259-263`、`:265-266`；`复核-G08-03-整改B.md:155`。 |
| **注** | 第一至五节已把这两条**并入 `G08-05-U20`**（见 §四 c、d 两行）。本条补充的是**整改 B 的订正情报**（`DEPENDENCY_RULES.md:31` 可作辅证、`§5.3` 实际范围是 9 份而非派单列的 4 份），这两点第一至五节未收录。 |
| **影响** | 版本口径与工具链口径两条硬约束均无执行面。 |
| **所需输入** | 负责人确认 G08-10 重建时点与车道。 |
| **建议** | **建议**把三条（版本一致性、`verify_toolchain`、`runtime_oracle` R1–R6/R8）与 `G08-01-U2` 合并成**一份**「G08-10 门禁重建输入清单」，避免逐条分派导致漏项。同时把 `DEPENDENCY_RULES.md:31` 写进 `verify_toolchain` 的判据条款锚。 |
| **阻塞级别** | 阻断下一任务 |

---

## 6.3 已被后续车道推翻或关闭、但第一至五节尚未吸收的条目

### 6.3.1 `UNRESOLVED_REGISTER.md` 的处置方向：**「整体迁出」这一推荐已被推翻** 🔴 **第一至五节仍沿用未推翻的推荐**

第一至五节 §五第 1 项写「形态体检判它应属过程层……**建议迁出**」。但：

| 字段 | 内容 |
|---|---|
| **事项** | 该文件是「迁出 `docs/`」还是「按 §8.3 章程清洗」？ |
| **分歧** | **形态体检 §7 U2**（`G08-03-书写形态体检.md:470`）：`DOCUMENT_GOVERNANCE §7.1` 豁免面是**封闭枚举**且不含本文件；本件是 4010 行治理过程台账，本质属过程层 ⇒ **建议迁出**（批次 A）。<br>**复核**（`复核-G08-03-形态.md:56`、`:227`、`:309`）：**「角色错放」定性过强，判定不成立**。该文件 `:3` 上游 = `DOCUMENT_GOVERNANCE §8.3（登记面交付形状）`、`:16`「本面不承担裁决，不写过程流水、责任归属与变更轨迹」——**正是 §8.3 明文认可的登记面交付形状**。正确诊断是「违反自身 §8.3 章程（个别条目写了 commit/日期）」，处置应是**按条目清洗或补 §7.1 豁免面**，**不是迁出**——「迁出会把一个合规交付形状删掉」。 |
| **出处** | 执行 `G08-03-书写形态体检.md:198-202`、`:229`、`:386`、`:470`；复核 `复核-G08-03-形态.md:56`、`:227`、`:309`。 |
| **本车道实测（HEAD `ef3dc516`）** | `docs/engineering/UNRESOLVED_REGISTER.md` 现 **4178 行**（本车道开工时 4106 行，后续车道有增补）；`:3` 上游行仍含 `DOCUMENT_GOVERNANCE.md §8.3（登记面交付形状）` ⇒ §8.3 登记面定性**仍成立**。 |
| **影响** | 迁出 vs 清洗的决策影响 `docs/engineering/` 的目录形态与索引条目数。若照「建议迁出」施工，将删掉一个合规交付形状——这是**净损害**。 |
| **所需输入** | 负责人三选一：迁出 / 补 §7.1 豁免面 / 按条目清洗。 |
| **建议** | **建议采复核口径：既不迁出，也不整份清洗**——按 §8.3 章程做**按条目清洗**（有限集），并采纳 `审稿-RR10:203` 的「拆『现行未决项登记册』与独立复盘层；标题计数机检化」以解决流水账形态。 |
| **阻塞级别** | 阻断下一任务 |

### 6.3.2 形态体检的 `UNRESOLVED_REGISTER.md:5` 引文是**伪造**——本车道已独立证实

| 字段 | 内容 |
|---|---|
| **事项** | 一条确认违规判定建立在一条**不存在的引文**上，且该条目是「整体迁出」推荐的**唯一引用依据**。 |
| **分歧** | **形态体检 §3.1**（`:198`）：列 `UNRESOLVED_REGISTER.md:5` = `（本文件为治理过程登记册，非正式文档层正本）`，据以判「角色错放」。<br>**复核**（`复核-G08-03-形态.md:222`、`:328`）：判「**推翻（伪造引用，阻断级）**」——`sed -n '5p'` 输出**为空**；`grep '本文件为治理过程登记册'` **无命中**；真实第 3–4 行是「上游：…§8.3」与「地位：…唯一登记面」。复核明写「**该条目是批次 A 与 U2 的唯一引用依据**」。 |
| **出处** | 执行 `G08-03-书写形态体检.md:198`；复核 `复核-G08-03-形态.md:211`、`:222`、`:328`、`:351`。 |
| **本车道独立实测（HEAD `ef3dc516`）** | `sed -n '5p'` → **空行**；`grep -c '本文件为治理过程登记册'` → **0**；`:3` 含 `§8.3（登记面交付形状）`。⇒ **复核的证伪成立。** |
| **影响** | 该确认违规条目与批次 A「整体迁出」推荐**一并失效**（即 §6.3.1）。另牵连该件的计数问题：该件称「68 份含确认违规」，复核只从其自表析出 **51 份**（`复核-G08-03-形态.md:38`、`:230`），且其 §1.3 明文认定 `G08-10` 占位符违规却在 §6 全部 8 个批次中**一处未列**（`:283`）。 |
| **所需输入** | 负责人确认：形态体检 §3.1 的 `:5` 条目与批次 A 推荐项作废；是否要求该件**补正后重新提交**（复核 `:351` 列出至少 4 项须补正：§2.0 可执行词表定义、§3.1 该条目、§4.2/§4.3 行号与编号重复结论、§6 批次 A/D/G/H 范围）。 |
| **建议** | **建议要求补正后重新提交**，并**与其计数问题（68 vs 51）合并为一次打回**，不要分两次。一个含伪造引用的件不宜作为后续车道的施工依据。 |
| **阻塞级别** | **阻断提交** |

### 6.3.3 `DOCUMENT_INDEX.yaml`「14 条 `§8.4`→`§8.5`」移交项是**阻断级移交缺陷**——照单施工会破坏 9 条正确引用

| 字段 | 内容 |
|---|---|
| **事项** | 移交清单的数量与定性双错，下发车道 A 前必须撤回重写。 |
| **分歧** | **整改 B §6 #3**（`:247`）：`docs/DOCUMENT_INDEX.yaml` **14 条** `upstream` 的 `§8.4` → `§8.5`，列为须修，归属车道 A。<br>**复核**（`复核-G08-03-整改B.md §5.3`，`:158-164`）：**推翻**，判为「**阻断级移交缺陷**」——该移交项在**数量上错**（9 ≠ 14）**且在定性上错**（那 9 条不是缺陷，是正确引用）。复核明写「**移交 §6 第 3 项必须先撤回重写再下发车道 A，否则会破坏 9 条正确引用**」（`:26`）。 |
| **出处** | `G08-03-整改B-正本唯一性.md:247`、`:265-266`（§5.3 实际范围是 **9 份 + 索引 + 生成器**，派单列的 4 份中 `DOCUMENTATION_STANDARD.md` 越界）；复核 `复核-G08-03-整改B.md:23`、`:26`、`:158-164`。 |
| **影响** | 若按原移交单施工，**会破坏 9 条正确引用**。这是台账中少见的「**移交件本身错了**」情形。 |
| **所需输入** | 负责人/前台确认撤回该移交项并重新点数；同时订正派单范围（4 份 → 9 份 + 索引 + 生成器）。 |
| **建议** | **建议先撤回再下发**。生成器模板残留（`gen_module_readmes.py:79`、`gen_build_graph_doc.py:48,49,60,62`）**建议与之合并处理**——先把正确节号裁死，再一次性改生成器并重跑，避免重跑两次。 |
| **阻塞级别** | **阻断提交** |

---

## 6.4 前台已代为裁决的事项（供负责人**复核**，非重裁）

第一至五节**没有这一节**。以下三项已在前台层面裁定并落地，请负责人复核；如不同意，请退回重裁。

| # | 事项 | 前台裁定 | 依据 | 本车道复核结论 |
|---|---|---|---|---|
| **P-1** | **C4 口径取 435**（不砍真检出） | 采用 **435** | `G08-03-整改A-R3.md:156-173`：算式 `431 + 9 − 5 = 435`；三种候选修法中 435 是唯一「不新增假红、不丢真检出」的读数（9 条新增全真阳性、5 条消掉全假红），且方向对己方不利 | ⚠️ **仍有分歧**：`复核-G08-03-整改A-R2.md` 给的预期是 432/433。**建议负责人只需回答一个问题——指定哪个候选修法口径**；口径定死，数字自然唯一。第一至五节的 `G08-05-U55` 已就 435 的分布另立一条，建议两条合看 |
| **P-2** | **`closure_metric.py` 按规范 08 §2 保持删除执行面 + 登记 G08-10 承接** | 删 ctest 注册、留实现、G08-10 承接 | 规范 08 §2「删除旧门禁」；`G08-01-整改执行.md:170`、`:334`（执行件主动登记待确认） | 🔴 **独立复核已推翻其依据**：`复核-G08-01-整改.md §7.5` 判「拆口径不成立」，并指出 `GATES_AND_TOLERANCES.md:67` 的**发布门 = Y 当前零执行面**。本车道在 HEAD `ef3dc516` 实测**三处悬空指针仍在、发布门仍为 Y、执行面仍为 0**。**这是全表唯一的「已裁决但依据被推翻」条目，建议负责人优先复核**（详见 §6.2 `G08-01-U1`） |
| **P-3** | **脏树 36 文件立检查点** | 已提交 `10aa5490` | `git log` 实测：`10aa5490 治理开工前既有的文档改动立为检查点` | ✅ **已生效**。⚠️ 但仓内 HEAD 已推进到 `ef3dc516`，后续 G08-05 各车道又多次提交；当前工作树另有多车道在途改动。**负责人若要复核该检查点的完整性，须按 `ef3dc516` 而非 `10aa5490` 重新对账** |

---

## 6.5 建议裁决顺序（第一至五节**没有这一节**）

按「**阻断下游任务的程度**」排。同级内按「先裁它能让更多条目解锁」排。**只列需要跨条协调的决策点**，单条自明的不重复列。

### 第 0 批 · 先裁这 3 条（每条解锁一批下游）

| 序 | 编号 | 为什么必须先裁 |
|---|---|---|
| **1** | **§6.2 `G08-01-U1`**（`closure_metric`） | 全表唯一「**已裁决但依据被推翻**」的条目；`发布门 = Y` 却零执行面，是唯一**直接影响发布正确性**的一条。它同时牵动 §6.2 `G08-01-U5`（零门禁基线）——**正因 G08-10 未定，「登记 G08-10 承接」才是空头承诺** |
| **2** | **`G08-05-U20` + `G08-01-U8`**（门禁登记面落地时点 + 三条无实现判据） | **总闸**：`G08-01-U2`（runtime_oracle R1–R6/R8）、`G08-01-U5`（零门禁基线）、`G08-01-U8`（版本一致性 / verify_toolchain）、`G08-05-U20` 及 §四 c/d 两项共 **6 条**都以「G08-10 重建」为出口。建议**一次裁定**，产出「G08-10 门禁重建输入清单」，避免逐条分派漏项 |
| **3** | **`G08-05-U12/U13/U14` + §6.3.3**（三条 science 冲突 + 撤回错误移交） | 三条 science 冲突按 `AGENTS.md §3/§4` 属必须由负责人裁的正本内容事项，**都阻断发布**；§6.3.3 若不先撤回，会在施工时**破坏 9 条正确引用**——属净损害，须在下发前拦住 |

### 第 1 批 · 阻断提交、但不卡下游施工

`§6.3.2`（形态体检含伪造引文，要求补正后重提；与其 68 vs 51 计数问题**合并一次打回**）→ `§6.3.1`（`UNRESOLVED_REGISTER` 处置方向：采复核口径，不迁出）→ `§6.2 G08-01-U6`（假绿文本与「在役」断言，补 8 处 `eng/**`）→ `G08-05-U55`（435 分布未核证，与 §6.4 P-1 合看）。

### 第 2 批 · 可批量签字

`§6.2 G08-01-U3`（追认已落地的删件）、`§6.2 G08-01-U4`（建议保留 `sha256_utf8.py`）、`§6.2 G08-01-U7`（建议采信复核的 19 条）、`G08-05-U44`（`AGENTS.md:93` 环境中立改写）、`G08-05-U49/U50/U51`（元话语 / `v1` / `P-1xx`，形态体检与复核已同向，只缺签字）。

> **建议负责人先做一件零成本的事**：把 §6.1 的三行状态变化（顶点断链**已关闭**、`closure_metric` **仍阻断**、`模块 ID` 以 U18 为准）直接并入第一至五节对应条目。**本车道起草的 60 条中约 35 条已被 U1–U61 覆盖或超越，不应重复进入裁决流程**——重复立条会让负责人对同一问题裁两次、且两边数字不一致。

---

## 6.6 待核实（本车道无法在源材料中找到充分依据，**不进裁决清单**）

| # | 事项 | 为什么待核实 |
|---|---|---|
| V-1 | `docs/engineering/UNRESOLVED_REGISTER.md:3117-3129` 的内容（`G08-01-U5` 引其为「`eng/ci/` 不存在」自陈） | 本车道**未逐行读该段**，仅采信 `G08-01-整改执行.md:234` 的转述。而该文件行数已从 4106 增至 4178，**原行号可能已漂移** |
| V-2 | G08-01 各项「悬空引用 53 条」的**起点**数字 | 三方不一（任务书 40 / 执行件 53 / 复核判「不构成 >53 的阻断」），本车道未独立复算 |
| V-3 | `G08-05-U3` 标题所称「14 / 附录A 10 三数并存」中的**「14」与「附录A 10」两说** | 本车道只核到「§3.1 已补为 13 个」这一步（§6.1），未核另两个数。若仍成立，U3 仍需裁，但**断链**已消解 |
| V-4 | `G08-01-U3` 取正本掩膜式后的**重算工作量** | `G08-04-整改G2` 只说「半径扫描与 `r_local` 闭式须按 `clip(·, r_min, 60)` 重算」，**未给工作量估计**，本车道无法评估 |
| V-5 | 本车道 21:39–00:41 期间的全部实测值 | 当时 HEAD 为 `10aa5490`；此后 G08-05 各车道多次提交，**仓内已大幅变动**。§6.2/§6.3 中标「本车道实测」的行均为 `ef3dc516` 下复测；**未标注者沿用源材料原值，其现状需负责人按 `ef3dc516` 复核** |

---

**本节编制纪律声明**：全程只读仓内；仅追加 `run/GOVERN-08/审核包-R2/UNRESOLVED.md` 一个文件（第一至五节 889 行**原文保留、一字未改**，副本存 `/tmp/UNRESOLVED.bak`；`run/` 在 `.gitignore` 内，不出现在 `git status`）；无 git 写（未 add / commit / checkout / reset / stash）；未编译、未跑测试、未跑构建、未联网；**未替负责人做任何裁决，「建议」栏全部标明为建议**。本节所有 `文件:行` 均为编制时刻（HEAD `ef3dc516`）亲自实测。
