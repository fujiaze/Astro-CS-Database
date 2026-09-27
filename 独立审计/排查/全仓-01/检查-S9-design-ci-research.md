# 检查报告：S9（docs/design + docs/ci + docs/research + 根级零散）

- 切片：S9「docs/design＋ci＋research＋根级零散」；检查方式：**纯静态只读**（未编译、未测试、未运行任何检查器/门脚本、零 git 写）。
- 覆盖文件：docs/design/ 6 篇（LOG_AND_ERROR_SYSTEM、PHASE1/2/3_DETAILED_DESIGN、PRODUCT_STORAGE_FORM、UNIFIED_MODEL）；docs/ci/ 5 篇（01_CHECKS、02_PIPELINE、03_GATES、04_ARTIFACTS、CI_SPEC）；docs/research/ 6 篇（CCD_LINEAR_DEFECT_LITERATURE、COMPRESSION_CODEC_RESEARCH_PACK＋_TABLES、IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK、PHOTOMETRY_RESEARCH_PACK、SNR_WEIGHT_RESEARCH_PACK）；根级 docs/DOCUMENT_INDEX.yaml、GLOSSARY.md、DEVELOPER_GUIDE.md、KNOWN_LIMITATIONS.md、TROUBLESHOOTING.md、VERSIONING.md。
- 预读已排除项：`独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（红 8 条中 7 PASS + 1 partial FAIL 的 k_shape 残留、黄 16 条中 15 PASS、行漂移 6 条，均不复报）；`分歧台账.md` D-01…D-11 与 §2 A-* 终裁不重开。
- 统计：**红 6 / 黄 12 / 绿 3**。

---

## 一、红级（必须改）

### S9-01 ｜红｜③④
**文件:行**：`docs/DOCUMENT_INDEX.yaml`（规则自述于头部注释「规则 2」段；docs/research 条目位于 :1256-1283）↔ `docs/research/CCD_LINEAR_DEFECT_LITERATURE.md`

**问题**：索引自述「每份下级文档（docs/**/*.md）必须在 active 区段登记……覆盖率 100%，机器判红」，但 `docs/research/CCD_LINEAR_DEFECT_LITERATURE.md`（309 行、git-tracked、2026-09-24 经 a6f602fe 入库）**在索引中完全不存在**——它是 233 份 docs/*.md 中唯一未登记者（其余全部登记、265 条条目路径全部存在）。

**证据（静态复算，按检查器判据逐条对照）**：
- `grep -n 'CCD_LINEAR' docs/DOCUMENT_INDEX.yaml` ⇒ rc=1（active/archived 两段均无，也无不带引号的变体）；
- `git ls-files docs/research/` 含该文件；索引最后改于 c4af4136（09-25 01:16），**晚于**该文件入库时间（09-24 23:49），非时序差；
- 索引无任何目录级条目（`- path: ".../"` 0 命中）⇒ 不存在前缀覆盖；
- 复算 `eng/tools/doccheck/check_doc_index.py` 判据：`docs_fully_covered`（:470 `uncovered=[p for p in docfiles if not _covered(...)]`，docfiles=git 跟踪的 docs/** md）与 `subordinate_docs_registered`（:500-503，`status_of()` 缺省返回 ACTIVE_INFORMATIVE ⇒ 该文件进入扫描面）**两面都应判红**；coverage 判据无台账豁免（`dangling_ledger.json` 只作用于 docs_path_refs 面）。
- **反方核验**：① 若该门在 CI 实为绿，则必有我未见的豁免面——已排查：无目录条目、无 archived 条目、无 ledger 豁免、无状态过滤旁路；② 本地 `artifacts/ci/**/CI_RESULT.json` 最近几份 profile 只跑 1~33 项，**不能**作为该门为绿的证据；③ `docs/KNOWN_LIMITATIONS.md:107` 记载的「doccheck --strict rc=0」实测早于/独立于本状态，且其对象是三处悬空引用，不是本条覆盖缺口。

**建议改法**：在索引 active 区段补登该文件（path/status/duty/upstream/downstream 五字段，抬头补「上游」条款区若缺）；或若判定其应归档则按 archived 规则登记并改 status。

### S9-02 ｜红｜③④
**文件:行**：`docs/ci/01_CHECKS.md:19-165`（§2 表）、`:239` ↔ `eng/ci/checks.json`（150 个注册项）

**问题**：§1 声明「与 checks.json 双向一致（CHK-REGISTRY-DOC-SYNC 门）」，实际 §2 表只有 142 个 ID，**8 个已注册检查项未登记**：`CHK-BUDGET-SINGLE-SOURCE、CHK-DOCS-MACHINE-CONSISTENCY、CHK-DOCS-MACHINE-CONSISTENCY-SELFTEST、CHK-DRZ-PF-AREA-S1、CHK-ISA-SAME-SOURCE、CHK-PROD-WIRING、CHK-TRUTHFUL-CONCLUSION、SPEC-POLARITY-CONSIST-01`。且 :239 仍写「**注册项待写入**：……登记 ID SPEC-POLARITY-CONSIST-01……」，而该项**已经在 checks.json 注册**（文档说未登记 vs 注册表已登记，正面矛盾）。

**证据**：静态解析 checks.json 150 id vs §2 表 142 id 差集 = 上列 8 项；反向差集为空（无「登记了没注册」项）；表内无重复 ID；`exemptions.json` 不含这 8 项；8 项均为普通注册项（waivable:false，含 selftest 步骤）。历史佐证：`run/CLAUSE-WIRING-01/registry_doc_sync.json`（09-25）verdict=FAIL、registered_not_documented=[CHK-PROD-WIRING]——其后差集已扩大到 8。
**反方核验**：8 项不是豁免项、不是 RESERVED 项（id_migration_map.reserved_targets 只有 CHK-FMT/CHK-DUAL-TOL/CHK-AGENT-HARD-RULES）、不是新增未提交（均已在库）。

**建议改法**：按 §4 流程把 8 项补进 §2 表（ID/等级/命令/门），删除或改写 :239 的「待写入」陈述。

### S9-03 ｜红｜④
**文件:行**：`docs/ci/CI_SPEC.md:213-214`

**问题**：L2 四条**冻结判据**的第 4 条表格行内泄漏模型分隔符字面量 `<｜｜begin▁of▁sentence｜｜>`，把「无就绪积压同样违规」截断并撑破 markdown 表格单元（行 213 未闭合、214 才接 `违规**：…`）。冻结判据的人读条文当前不可读。
- 原文（:213）：`| 4 | 无连续 ≥10s 且利用率 <60% 的低利用窗（**无就绪积压同样<｜｜begin▁of▁sentence｜｜>…`
- 反方核验：`grep -c 'begin▁of▁sentence' docs/ci/CI_SPEC.md` = 1（仅此一处）；机器侧冻结源完好——`eng/contracts/resource_gate_v1.json::compute` 含 queue_low_utilization_percent=60、queue_low_window_seconds_min=10，判据语义无损，**但** :217「任一判据违规 ⇒ verdict=red」的判据文本以本表为唯一散文出处。
**建议改法**：删除分隔符字面量，恢复为「无就绪积压同样违规」，并把该单元收回单行。

### S9-04 ｜红｜③④
**文件:行**：`docs/design/PRODUCT_STORAGE_FORM.md:19` ↔ `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md:35`（N7）；实现侧 `lib/**`

**问题**：三面互不相容——
1. 设计文档：「**两形态都由产品级索引** `<name>.hips.index.json` 伴随（§5）。索引是产品的组成部分，**不是可选的调试附属物**」（:19，绝对陈述，无「目标态/未实现」标注；该文抬头只有上位/下游，不像 PHASE1/2/3 有「目标态详细设计」自述）；
2. 机器合同 N7：「裸形态**允许**无产品级索引；此时读端按目录枚举重建覆盖，并在 provenance 记降级」——同一属性一边「必备」一边「可缺省」；
3. 实现侧：`lib/**` 对 `storage_form` / `index_path` / `*.hips.index.json` / `coverage.index` **零写出**（写者只有 CLI 解析键表与注释）；真实产物 `run/**/p1_products.json`（8 份）与 aio `manifest.json` 均无 storage/index 段，全树唯一的 `coverage.index.json` 是 `run/HIPS-PACK-01/coverage_lab/` 实验 fixture；`eng/ci/ledgers/dead_config_keys.json#dead_config_key:storage_form` 明记「生产写出侧零读取」、`coverage_index`「生产消费点为零」。
**反方核验**：`docs/contracts/CONFIG_CONTRACT.md:85` 确实披露 storage_form 属「分阶段实现计划阶段 2」——但披露对象是**配置键**，并未豁免设计文档对「索引必备」的断言，也未调和与 N7 的矛盾；`dead_config_keys.json` 属 eng/ci 台账，docs/ 侧仅 KNOWN_LIMITATIONS 交叉引用了 algorithm_rejection_method/reject 两键（:153），未覆盖 storage_form/coverage_index/snr_path。
**建议改法**：二选一并全局对齐——(a) 设计文档改为与 N7 一致（裸形态可无索引 + 降级语义），或 (b) 合同 N7 收紧为必备；无论方向，补「当前生产未写出」的显式披露（沿 DISP-STAR-002 惯例）。

### S9-05 ｜红｜③④
**文件:行**：`eng/ci/check_registry_doc_sync.py`（按 `### 2.3` 解析 RESERVED；R3 从 §2.1 摘退役项）↔ `docs/ci/01_CHECKS.md:166/:180`（只有 §2.1、§2.1.1，**无 §2.2/§2.3**）↔ `docs/ci/03_GATES.md:39`

**问题**：同步门的两个判据面与文档结构错位——
- **R4（RESERVED 误注册）恒空**：解析器只认 `### 2.3` 段，而 01_CHECKS 不存在 §2.2/§2.3 ⇒ 该面永远解析出 0 空集，**永不触发**（判据退化为恒真/死面）；
- **R3（退役回归）看错对象**：从 §2.1 摘出的 token 是 `['CHK-FMT','CHK-DUAL-TOL','CHK-AGENT-HARD-RULES','changed_paths']`——前三个是 RESERVED（03_GATES:39 明说「RESERVED 不进本表」），`changed_paths` 是垃圾 token；而正文真正描述的退役项（TASK-RESULT-SCHEMA、WORKSPACE-ADOPTION、RECONCILE-STATE、TRACEABILITY-CODE，01_CHECKS:170 一带）**从不被解析**；
- **文档锚同样悬空**：03_GATES:39「逐条依据……见 `01_CHECKS.md §2.3`」指向不存在的章节。
**证据**：`grep -n '^#' docs/ci/01_CHECKS.md` ⇒ 19/166/180/249/257/264，无 §2.2/§2.3；`run/CLAUSE-WIRING-01/registry_doc_sync.json`（09-25）verdict=FAIL 且 doc_reserved_count=0（与「R4 永远读不到 RESERVED」互证）。
**反方核验**：机器面 `eng/ci/id_migration_map.json::reserved_targets` 存在且含 CHK-FMT(P0)/CHK-DUAL-TOL(P1)/CHK-AGENT-HARD-RULES(P0)，**不是**「RESERVED 整体没登记」，问题只在解析面与文档锚；R2 面（登记未注册）解析路径正常。
**建议改法**：01_CHECKS 落一个真实的 §2.3 RESERVED 表（或把解析器改指 §2.1/§2.1.1 并按 flag 区分 RESERVED 与 RETIRED），R3 改解析正文退役清单并去掉 changed_paths；同步修正 03_GATES:39 的章节引用。

### S9-06 ｜红｜③④
**文件:行**：`docs/ci/02_PIPELINE.md:12、:23-47、:53、:55、:77-79` ↔ `.github/workflows/ci-linux.yml` / `ci-windows.yml`

**问题**：流水线文档描述的是一条**当前不存在的** CI 架构——
1. :12 触发表含 `pull_request → changed`，两 workflow 的 `on:` 实际只有 push[main] + workflow_dispatch + schedule（**无任何 PR 触发**）；
2. §2 Job 图（:27-32）与 Job 表（:38-44）列出 build-linux / build-windows / static+doc+contract / unit+module / package-candidates 等**多 job 依赖图**，实际每平台**单 job**（ci-linux.yml:41-44 `jobs: linux`，内部一步 `python3 eng/ci/run.py --profile …`），且 runner 是 ubuntu-24.04 / windows-2022（02_PIPELINE 与 04/CI_SPEC 的 ubuntu-latest 口径也不同）；
3. :53「每 job 设 timeout：build 30 min、测试 45 min、打包 15 min」vs 实际 `timeout-minutes: 330` 单 job；
4. :55 证据锚 `run/CI-INCREMENTAL/EVIDENCE.md` **不存在**（`ls run/CI-INCREMENTAL` ⇒ No such file）。
**反方核验**：:54 的 step 级预算公式与 :77「产物见 04_ARTIFACTS」的引用本身有效；增量预算 120s 属实（`eng/ci/run_checks.py:152 DEFAULT_INCREMENTAL_BUDGET_SECONDS=120`）；schedule（cron 17 19 * * *）与 workflow_dispatch 属实——即触发面只错了 PR 一行，但 job 图/超时/证据锚三处都是结构性描述错误。
**建议改法**：按现役 workflow 重写 §1 触发与 §2 job 图（或显式标注「目标态编排，当前为单 job 实现」），:53 改为实测 330min 单 job 口径，:55 改为现存证据锚或标注已回收。

---

## 二、黄级（建议改）

### S9-07 ｜黄｜③
**文件:行**：`docs/ci/03_GATES.md:26` ↔ `docs/ci/01_CHECKS.md:25、:249-253`
**问题**：同一检查项等级冲突——03_GATES 简表 `CHK-WARN / STATIC | P0 | 阻塞=是 | 豁免=否`，01_CHECKS §2 表 `CHK-STATIC … | P1`；按 01 §3 定义 P0=无 waiver、P1=可负责人豁免 ⇒ 两文对「CHK-STATIC 红灯能否被豁免」给出相反答案。
**证据**：静态展开 03 §3 简表的斜杠组逐 ID 对表，全表仅 CHK-STATIC 一处级别冲突（`CHK-PSFSW-RETIRED-STATIC` 是同一斜杠组 `STATIC` 的另一种展开，同源问题）；其余 ID 级别一致。
**反方核验**：03 自称「简表，完整见 01_CHECKS.md」（:21），即权威可判定，冲突不至于无解——故定黄不定红；但简表本身的行是错的。
**建议改法**：03_GATES:26 改 P1（豁免=负责人登记），或说明 STATIC 拆分了 P0/P1 两个面。

### S9-08 ｜黄｜③
**文件:行**：`docs/ci/03_GATES.md:96-105`（§6.2/§6.3/§6.4）↔ `eng/ci/checks.json`、`run/ci/failclosed-survey/survey.json`
**问题**：三处「生成自注册表」的数字与现状漂移——
1. §6.2 表称「8 检查 / 11 执行单元」，step 级 requires_monitor 实际 11 个但集合不同：注册表有 `CTEST-LINUX-FULL`、`TEST-INDEX-LIVE` 两个 step 未入表，表里反而列了两个**非 step** 的顶层项（RESOURCE-GATE-REAL、CHK-REALDATA-E2E）；
2. §6.3「7 个顶层声明 mutates_workspace」实际顶层 **9** 个（多 `CHK-MASTER-UNIT-GUARD`、`CHK-MASTER-UNIT-GUARD-SELFTEST`）；step 级 6 个与表一致；
3. §6.4 与 01_CHECKS CHK-FAILCLOSED-SURVEY 行「对 230 个执行单元（78 个注册项）……226 适用」，最新机器普查 `run/ci/failclosed-survey/survey.json`（09-25）为 units=**364**、有适用面=**360**、无适用=4（注册表现为 150 项）。
**反方核验**：§6.1「artifacts/acceptance/l2_performance/gates/*_gate.json 9 份 verdict=pass」**经复算正确**（11 文件中 9 个 verdict=pass、2 个 not_applicable），不列为问题；「4 个显式无适用面」与 survey.json 的 units_without_applicable_face=4 一致。
**建议改法**：由 `analyze` 类脚本重生成 §6.2/§6.3/§6.4 数字，或标注生成时点与复跑命令。

### S9-09 ｜黄｜④
**文件:行**：`docs/ci/01_CHECKS.md:21-164`（§2 表体）
**问题**：表结构损坏，机器/人读都会错位：表头 :21 行尾多一个 `|`；**16 行不是 5 单元格**——CON-SYMBOL-DIM-UNIQUE(+SELFTEST)、CHK-FROZEN-STRING-DISAMBIG(+SELFTEST) 缺 command/gate 两列，CHK-BUILD-PROVENANCE 两行只有 4 单元；≥8 行单元内含**未转义 `|`** 把列挤断（CHK-PHOTOMETRY-APPLY-SELFTEST、CHK-SPARSE-PUNCH、AHPX-WEIGHT-RETIRED、CHK-DRZ-DISP009、CHK-P3-PROJ-DECL、CHK-HIPS-STORAGE-FORM、CHK-REGISTRATION-ANCHORS、CHK-L4-SEAM-FOOTPRINT）。
**证据**：按 `|` 切分逐行计数（静态复算）；同文件 §2 只有 §2.1，无 §2.2/§2.3（见 S9-05）。
**反方核验**：ID 列本身完整无重复（142 唯一 ID），损坏集中在尾列，未造成 ID 丢失——故黄不红。
**建议改法**：单元内 `|` 全部写成 \| 或改用「、」，补齐缺列，去表头尾游离 `|`。

### S9-10 ｜黄｜②
**文件:行**：`docs/design/PHASE2_DETAILED_DESIGN.md:54`
**问题**：小节标题写「**最终五档表**」，其下表格只有 **3 档**（1–3 none / 4–5 percentile / ≥6 winsorized）；正本 `docs/plugins/algorithms_phase1/12_rejection.md §9` 明写「由四档收成三档」。标题是上一版口径残留（新旧并存的订正注记冲突）。
**反方核验**：表内容与正本三档一致，仅标题错；不涉及 D-01…D-11 任何终裁数值。
**建议改法**：标题改「最终三档表」，或加一行「五→四→三档演进注记」。

### S9-11 ｜黄｜②④
**文件:行**：`docs/design/PHASE1_DETAILED_DESIGN.md:55、:63`
**问题**：(a) :55「细化见 §3.6」——该文 §3 没有任何子节（标题清单：§1…§3…§4.1 起才有子节），**§3.6 不存在**；(b) :63 证据锚 `run/RULING-DOC-01/REPORT.md`（裁决 B）**不存在**（`grep` 全树 exit=1），同一死锚还被 `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md:145` 引用。
**反方核验**：run/ 为可回收临时区（AGENTS §3/§7），锚可因 run_gc 消失；但裁决类证据落 run/ 本身就违反「证据落 artifacts/evidence」的仓库惯例，且 §3.6 是纯文档内锚，无回收借口。
**建议改法**：§3.6 改为实际小节号（或补写该节）；RULING-DOC-01 证据改引入库件或补 artifacts 落点。

### S9-12 ｜黄｜②
**文件:行**：`docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md` §2.2.1 负档 bullet（「负档（−1~−7）压缩率明显更差（0.365 vs 0.307），**且比正档 1 更慢（见 2.3）**，没有使用价值」）
**问题**：所引 §2.3 表恰与断言相反：zstd −1 压缩 0.312 ms / 3,203 MB/s、解压 0.096 ms，zstd 1 压缩 0.429 ms / 2,334 MB/s、解压 0.210 ms ⇒ 负档**更快**；`COMPRESSION_CODEC_RESEARCH_PACK_TABLES.md` 各分组「c −1」列同样全部快于「c 1」。负档「压缩率更差」成立，但「更慢⇒没有使用价值」的论据不成立。
**反方核验**：结论层（推荐 level 1~3、不用负档做默认）与 §5.1 建议不受影响——受影响的是该 bullet 的论据链。
**建议改法**：把该句改为「负档压缩率明显更差（0.365 vs 0.307），速度虽快但 ratio 不划算，故不采用」，或删除「更慢」。

### S9-13 ｜黄｜②
**文件:行**：`docs/research/CCD_LINEAR_DEFECT_LITERATURE.md:160-172`（§2.4 之后）
**问题**：§2.1 的三条编号清单（「1. 把坏像素的噪声设为无穷大…/2. 插值像素被单独打标…/3. 坏列与宇宙线…」）在 §2.4 之后**原样重复一遍**，无标题、无承接；且第 2 条与前文有轻微差异（多出「→ 这是"修复会污染测量可靠性"的一手判据…」），属订正/搬运残留的重复段。
**反方核验**：两段语义不矛盾（仅详略差），不构成结论冲突——故黄不红。
**建议改法**：删除重复块，或改写为明确的「§2.4 小结」并只保留一份。

### S9-14 ｜黄｜④
**文件:行**：`docs/research/PHOTOMETRY_RESEARCH_PACK.md:90` ↔ 同文件 `:50`
**问题**：文献题录错页——:90 引「Gaia Collaboration, Montegriffo et al. 2023, **A&A 674, A33**, §1」，同包 :50 与 Crossref 均为 **A3**。外部核验：`api.crossref.org/works/10.1051/0004-6361/202243880` ⇒ vol 674 / page **A3** / 2023 / a0 Montegriffo。同一文献在同一文件内两种页码 = ②④双面。
**反方核验**：DOI/arXiv 本身正确（`arXiv:2206.06205` = Montegriffo 外定标，arXiv API 标题逐字匹配），错的只是页码。
**建议改法**：:90 改 A3。

### S9-15 ｜黄｜④
**文件:行**：`docs/research/PHOTOMETRY_RESEARCH_PACK.md:18、:191-197`；`docs/research/SNR_WEIGHT_RESEARCH_PACK.md:214-229`
**问题**：两包把全部核验留痕与复跑命令指向 `run/DOC-404/**`（verify_refs.py、check_pack_refs.py、check_pack_oss_anchors.py、evidence/*.html、crossref/*.json、verified_refs_table.txt），当前树 **`run/DOC-404` 整目录不存在** ⇒ §2「核验方式（本包执行）」与 §7/§9 复跑块不可执行，「doi=45 ok=45」等计数无落盘证据可查。
**反方核验**：run/ 属可回收区（AGENTS §3），且 SNR 包头部已声明「历史/冻结层」；但 PHOTOMETRY 包是现行活文档，其可复核性主张依赖这些文件——对照组：`run/SCI-PHOT-FORMULA-01/evidence/*.json` 在位，说明并非整区被回收，是该目录从未入库或被定点清理。
**建议改法**：把关键证据（verified_refs_table、crossref 摘要、锚命中表）归档到 `artifacts/evidence/doc-404/` 并改引，或在两包标注「证据已回收，复跑需重新抓取」。

### S9-16 ｜黄｜④
**文件:行**：`docs/VERSIONING.md:63` ↔ `eng/tools/check_version_consistency.py:39-43`
**问题**：扫描范围声明与实现不符——文档写「扫描 `docs/ schemas/ eng/tools/ launch/ eng/tests/` 与根级 README/CHANGELOG/build.sh/toolchain.ps1」；实现 `SCAN_ROOTS = [docs, eng/contracts/schemas, eng/tests]`、`SCAN_FILES = [eng/build/build.sh, eng/build/toolchain.ps1, README.md, VERSION, eng/tools/gen_version.py]`。差异：`launch/` 与 `CHANGELOG` **在仓库中根本不存在**（声明扫描不存在的面）；`schemas/` 实际是 `eng/contracts/schemas/`；`eng/tools/` 声明为面但只扫 1 个文件；扫描但未声明的 `VERSION`。
**反方核验**：mutation 合同（:64）与豁免清单（§5）与实现一致；:18 所引 `schemas/version.schema.json` 实为 `eng/contracts/schemas/version.schema.json`（同根相对写法，可容忍）。
**建议改法**：按 SCAN_ROOTS/SCAN_FILES 重写 :63 的范围句，删 launch/CHANGELOG 或标注「已不存在」。

### S9-17 ｜黄｜④
**文件:行**：`docs/TROUBLESHOOTING.md:29-35`（常用命令）
**问题**：5 条命令中 **4 条路径不存在**：`lib\snr_estimator\cpp\test\noise_model_science_test.exe`、`lib\healpix_db\healpix_drizzle\tests\variance_propagation_test.exe`、`lib\phase2\build\phase2_synthetic_gate.exe`、`py -3.12 tools\astrocs_diagnose.py`。现址分别是 `lib/algorithms/noise_snr/cpp/test/`、`lib/algorithms/drizzle/healpix_drizzle/tests/`、（phase2 gate 见 eng/tests 对应 CMake 目标）、`eng/tools/astrocs_diagnose.py`——三处 `lib/<旧模块名>` 是 2026-09-24 布局重整前的旧树。
**反方核验**：表内诊断语义本身在位——E100/E980 出现在 `eng/tools/astrocs_diagnose.py`，`ZERO_VALID_WEIGHT`/`INVALID_CONFIGURATION`/`NOISE_MODEL_STATUS` 均可在 lib 命中；`eng/build/toolchain.ps1` 存在 ⇒ 不是全篇失效，只是命令路径过期。
**建议改法**：四条路径改到现址（或改为 `ctest -R <名>` 形式，避免再次随布局漂移）。

### S9-18 ｜黄｜④
**文件:行**：`docs/design/PHASE2_DETAILED_DESIGN.md:21、:114`；`docs/design/UNIFIED_MODEL.md:46`
**问题**：以现在时陈述「实际生效口径记录在 `snr_path_effective`」（:21/:114）与 snr_path「三条路径产出同一物理量…」（UNIFIED_MODEL:46），而 `snr_path_effective` 在 `lib/**` **零出现**、`snr_path` 生产读取点为零——`eng/ci/ledgers/dead_config_keys.json#dead_config_key:snr_path` 明记「生产科学消费点仍为零……防止把『CLI 认识』误当成『已消费』」，其 exit_condition 恰是「snr_path_effective 出现生产读取点后删除本条」。两文均无未实现披露；KNOWN_LIMITATIONS §47 只交叉引用 `algorithm_rejection_method`/`reject` 两键（:153），未覆盖本键。
**反方核验**：PHASE1/2/3 抬头自称「目标态详细设计」、PHASE1:112 有 DISP-STAR-002 式披露先例、CONFIG_CONTRACT:85 披露 storage_form——披露机制存在而这两处未用；UNIFIED_MODEL 无目标态标注。故定黄（有缓解、非无据断言）。
**建议改法**：两处补「当前未实现（dead_config_keys#snr_path）」式披露，或在 KNOWN_LIMITATIONS §47 增列该键交叉引用。

---

## 三、绿级（可不改）

### S9-19 ｜绿｜②
`docs/design/LOG_AND_ERROR_SYSTEM.md:206-207`：「下游节点必须按逐帧表选择输入面」在逗号与换行处**原样重复两遍**。机械重复，语义无冲突，读感瑕疵。

### S9-20 ｜绿｜②
`docs/KNOWN_LIMITATIONS.md` §B 条目编号 **13 → 15 跳号**（:27→:28），`artifacts/evidence/known-limitations-ledger/LEDGER.md` 的编号注册表也无 14 号记录（grep 0 命中）。台账明言「保留原条目号作为稳定 ID 锚点」，跳号不造成任何现存引用断裂（外部只引 §15/条目 30/35 等），仅建议补一行「14 号已撤，号不复用」的 tombstone。

### S9-21 ｜绿｜②
`docs/design/PRODUCT_STORAGE_FORM.md:246`：反引号不配对（`"`<output_dir>/manifest.json`` 写法破损），渲染时会吞掉后续行内代码标记；纯显示缺陷。

---

## 四、已查无问题面（逐面覆盖声明）

**①科学性面：已查，未发现需报问题。** 抽验口径：
- 数值/公式与 `分歧台账` D-01…D-11 终裁对表——本切片 6 篇 design 与 6 篇 research 未出现与 D-01（1.0415+20%）、D-02（depth=8/+15%/max_depth≥9）、D-04（k=1.152）、D-05（idw_power=1.0）、D-08、D-10（natural_bicubic_spline_clip_v1）相抵触的常数或判据；
- 研究包公式抽检通过：PHOTOMETRY 包 m_AB=−2.5log₁₀F_ν−48.60（F_ν 单位 erg s⁻¹ cm⁻² Hz⁻¹）、⟨f_λ⟩ 加权式、F_syn 量纲 W·m⁻²·nm 与正文声明自洽；SNR 包 A_NEA=1/ΣP²、W_psf=a²ΣP²/σ_pix² 与 PHASE1 §7.1 逐字一致；COMPRESSION 包端到端判据 Δ=(1−ρ)S(1/BW_w+R/BW_r)−(t_c+t_d) 与临界带宽 BW*=(1+R)(1−ρ)S/(t_c+t_d) 数值复算自洽（zstd3+shuffle：省 3.117 ms − codec 1.077 = +2.040；BW*=1,119≈表中 1,120 MiB/s）；
- 门判据抽验：COMPRESSION §6 负例表全部有红有绿（全零块、纯随机 f32、NaN 往返 nulval、Rice 413 失败），**未发现恒真门充当证据**；
- 研究包题录外核验 7 条全对（见④抽样），仅 1 处页码错（S9-14）。

**②行文逻辑面：已查**（S9-10、11、12、13、14、19、20、21 共 8 条在案）；其余章节链、订正注记、UNRESOLVED 结论化排查未见新增问题。

**③跨文档冲突面：已查**（S9-01、02、04、05、06、07、08、18 共 8 条在案）。五方权威链抽查：docs/ASTROCS_DESIGN ↔ docs/ ↔ 实验/ ↔ 独立审计 ↔ 源码，本切片冲突集中在 docs 内部（design↔contracts、docs/ci↔checks.json、docs/ci↔workflows），未见与 docs/ASTROCS_DESIGN 条款或分歧终裁正面相抵者。

**④幻觉与锚面：已查**（S9-01、02、03、04、05、06、09、11、15、16、17、18 共 12 条在案）。
- **文献题录抽样**（方法：arxiv_search 按 id 查 + Crossref works API 逐 DOI 拉元数据，共 7 条）：arXiv 2206.06205（Montegriffo 外定标）✓、arXiv 2602.02844（Ryon & Grogin 2026, Serial CTE in ACS/WFC）✓、arXiv 2312.03064（van Dokkum & Pasha 2023）✓；DOI 10.1051/0004-6361/202243880（674/A3）✓ 但暴露 :90 页码错、10.3847/1538-4357/836/2/188（836/188, Zackay）✓、10.1086/131801（98/609, Horne）✓、10.1046/j.1365-8711.1998.01314.x（296/339, Naylor）✓、10.1086/316197（110/863, Pickles）✓；
- **标准原文核验**：抓取 IVOA HiPS 1.0 REC PDF 全文，比对 PRODUCT_STORAGE_FORM:23 与 COMPRESSION 包 [S5] 的两段引文——REC 中该句**出现两次、措辞各不同**（§目录结构例「data base, or any other appropriate packaging…directory structure」与 §5.1「Behind the HTTP server…method for packaging it…directory」），两处引文各自对应原文，**均非伪造**（此项原判为疑似错引，反方核验后撤销）；
- **文件:行锚抽样**：exit_codes（LOG 篇）、aio_sparse_punch.h 四个函数、hpps_writer write_fits_atomic、export_crop schema、id_migration_map.reserved_targets、dead_config_keys 12 条、run/SCI-PHOT-FORMULA-01 证据、artifacts/evidence/compress-01/**（final_numbers.json/fill_scan.json/MANIFEST.sha256 均在）、run/RUN-PROVENANCE-01/REPORT.md、docs/architecture/MODULE_MAP.md、docs/standards/、traceability 矩阵——均在位；失效锚见 S9-03、06、11、15、17；
- **「文档说有、代码没接」**：dead_config_keys 12 键逐条对 docs 披露面（披露不全见 S9-04/18）、CI 产物与 job 描述对 workflow（S9-06）、VERSIONING/TROUBLESHOOTING 命令面对现址（S9-16/17）、索引覆盖率（S9-01）。

**抽样方法（整体）**：6+5+6 篇正文**全量通读**（design/CI 全篇，research 按章节全读、TABLES 篇按头尾+结构抽样 810 行中的关键表），根级 6 件全读；机器对表用 Python 静态复算（不运行仓库检查器）：checks.json↔01_CHECKS 表、DOCUMENT_INDEX↔文件树、check_doc_index/check_registry_doc_sync/check_version_consistency 三个检查器的判据函数逐行复算、workflow YAML 结构、L2 gate 文件 verdict 分布、failclosed survey 数字、mutates/requires_monitor 集合。第三方代码（nanoflann/cfitsio/nlohmann）仅做接口层存在性确认，未审内容。

**纪律**：本报告只写入 `独立审计/排查/全仓-01/检查-S9-design-ci-research.md` 一处；未运行 `run_checks.py`、任何 check_*.py、ctest、构建或写盘命令；未执行 git 写操作；未对任何公式/默认容差/冻结定义提出改动（发现即登记）。
