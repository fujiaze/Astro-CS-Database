# 检查报告 · 全仓-01 切片 S12「docs/api + interfaces + traceability + validation + audit + references + owner + performance + browser」

> 检查方式：纯静态（read/grep/python 只读解析），未编译、未运行测试、未写入本报告之外的任何文件。
> 四面标注：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚。
> 「在册」= 该问题此前已登记于 KNOWN_LIMITATIONS / 文档现状审计台账 / AUD-101-DBxx；在册仍未修者照实列出并在编号后标注，不作为新发现重复计数。

## 计数

- 红 4 条（其中 2 条为在册未修）
- 黄 14 条（其中 4 条为在册未修/在册同族）
- 绿 3 条

---

## 红

### 红-1（在册 KNOWN_LIMITATIONS:102 #38 / AUD-101-DB01）`docs/api/PHASE3_API_V1.md:44`
- **所属面**：①③④
- **问题**：拒绝表写「variance / weight / ivar / flux-per-pixel 输入模式 → ACS_ERR_UNSUPPORTED」，与科学冻结、数据合同、实现三方全部相反——API 冻结面宣称拒绝的输入，正是 SCI 冻结要求受理、实现已在消费的输入。
- **证据**：① `docs/science/PHASE3_HIPS_TO_FITS.md:10` 与 `:147`（§9a-10）明列「variance/ivar …… 不属拒绝项」；③ `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:306-310`「Phase3 承载方差类输入；输出 VARIANCE/IVAR 扩展 HDU」「两者皆无时显式 unavailable（不静默）」；④ 反证实现消费并传播——`lib/phase3_session/p3_session.cpp:202+` 接收/传递 variance、ivar，`lib/phase3_session/p3_v6_export.cpp:624` 写 VARIANCE HDU。测试 `eng/tests/api/test_p3_api.py` 只核拒绝表字符串（11 字段断言=表内 10 行），不覆盖 variance/ivar 行 ⇒ 机器门对该行盲区。
- **在册状态**：`docs/KNOWN_LIMITATIONS.md:102` 第 38 条已登记，**至今未修**。
- **建议改法**：按 SCI-P3 §9a-10 终裁 + 实现实况修正该行；若主张保留拒绝语义，属科学面变更，须走变更流程裁决，不得在 API 冻结文档单方定案。

### 红-2（在册 DB-01 家族；§5 锚与测试主张为本批新证）`docs/api/MANIFEST_VERIFY_V1.md:70,74,78-80`
- **所属面**：③④
- **问题**：本文把**已删除的命令面**当 FROZEN 契约书写，且 §5 落点映射与测试主张全部失实。
- **证据**：③ §3 标题 `acsd verify --run-manifest --json`（:70）、§4 `config show-effective`（:74）均为已删别名（rc=2）——`lib/infrastructure/cli/command_tree.h:67-69` 注释「verify* 属已删别名 → rc=2（parser.cpp:36、129）」、`:109` 命令树仅 `doctor {--json,--run-manifest}`；`eng/tests/cli/test_cli_protocol.py:431-435` 断言删除面 rc=2。④ §5（:80）称「实现: lib/infrastructure/cli/main.cpp(cmd_config_validate/cmd_show_effective/cmd_verify/cmd_run)」——`cmd_config_validate`/`cmd_show_effective`/`cmd_verify` 全仓**零命中**，写 manifest 的实际实现为 `commands.cpp::write_run_manifest`（dispatch :2293、cmd_verify 语义落点 :2136）；§5 称「test_cli_protocol.py 追加 mutation 组（schema_version 篡改/…）」——该文件 `TestManifestVerify.test_01..09` 已 RETIRED，现行为 `TestManifestDoctor`，所列 mutation 组不存在。
- **在册状态**：DB-01 已登记「CLI_PROTOCOL/MANIFEST_VERIFY 与已删 verify/show-effective 冲突」——**仍未修**；§5 四锚与 mutation 测试主张为本批新证。
- **建议改法**：§3/§4 按 doctor --run-manifest 现命令面重写，§5 实现锚改指 commands.cpp 实锚，测试行改指现存 test 用例名。

### 红-3（本批新发现）`docs/api/MANIFEST_VERIFY_V1.md:21`
- **所属面**：④（附带 ③）
- **问题**：cpu profile 契约段描述的**字段与错误码映射在实现中不存在**——文档写了一个不存在的 profile 契约。
- **证据**：文档 :21 写 `{"schema_version":"1","kind":"astrocs_cpu_profile","cpu_signature":"<hw hash>","kernels":{...}}`、validate「仅做 kind/schema_version/结构检查(3)」、stale = `profile.cpu_signature ≠ 本机 signature → 5`。反证（逐项）：
  1. `kind` 与 `cpu_signature` 两个键**全仓零命中**（无任何 schema/源码消费）；
  2. 实际契约是 **v2**：schema 串 `astrocs.cpu-profile/v2`（`lib/infrastructure/benchmark/cpu/…/profile_gen_v2.cpp:562`），stale 三元组 = arch / quota_signature / logical_available（`cpu_routing.cpp:101 validate_profile_v2_for_machine`）；
  3. 错误码映射不符：profile **结构失败 → BACKEND 5**，仅「文件缺失/JSON 语法错 → 3」；
  4. `parser.cpp:681 validate_cpu_profile`（旧 v1 解析）**零调用点**，为死代码——文档描述的检查路径在实现里已无接线。
- **建议改法**：该段按 v2 实况重写（或迁入历史档案），错误码映射以 parser.cpp/cpu_routing.cpp 实现为正本；改动属工程面，不涉科学公式。

### 红-4（本批新发现）`docs/owner/RELEASE_STATUS.md:95,140`
- **所属面**：③④
- **问题**：发布权威文档把**已删除的 CLI 命令面**登记为 `INSTALLED`（“当前提交实测绿”），与命令树实况、与它自己引用的测试直接相反。
- **证据**：:95「CLI 薄命令面（validate/plan/inspect） | INSTALLED | `cli/parser.cpp` kRules「9 条新命令」+ `eng/tests/cli/test_cli001_vpi.py` 15/15 PASS」；:140「CLI 命令面: INSTALLED（phase1/2/3 × validate|plan|inspect；run --phases 已删除）」。反证：① `command_tree.h:8-9`「命令树只登记 normalize/mosaic/export + help + --version + doctor + benchmark；旧 phase1/phase2/phase3 及其别名」已删；② 所引测试 `test_cli001_vpi.py:91` 现为 `test_01_legacy_vpi_subcommands_exit_2`——断言 validate/plan/inspect **rc=2**，15/15 PASS 证明的是“已删除”而非“已安装”；③ 同批 `docs/owner/PIPELINE_OVERVIEW.md:36` 自己写「旧 phase1/2/3 用户命令与 validate/plan/inspect 全部删除且 rc=2」。
- **在册状态**：同族已登记于 DB-01（docs/api 侧）与 DB-11:512（PIPELINE_OVERVIEW:155）；**RELEASE_STATUS 这两行未被登记**。
- **建议改法**：:95 按 §0 词表降档并把证据锚改指删除面测试；:140 汇总行同步改为 `normalize/mosaic/export` 现命令面。

---

## 黄

### 黄-1 `docs/api/CLI_PROTOCOL_V1.md:44`
- **面**：④。锚 `lib/infrastructure/cli/src/commands.cpp:2115` 失效：该符号在文件内唯一出现于 **:2259**；:2115 所在区块为分发代码（不是所指调用）。**建议**：锚改 :2259。

### 黄-2 `docs/api/CLI_PROTOCOL_V1.md:108`
- **面**：④（DB-05 同族漂移，本行未登记）。锚 `resource_recorder.h:260-266` 已漂：CSV 表头实际在 **:289-293**；正文列的 20 列与实表**逐字一致**（内容无误，仅行号旧）。**建议**：锚改 :289-293。

### 黄-3 `docs/api/PHASE2_API_V1.md:55`
- **面**：②。行文自称机器门核对「所有权图完整(**七对象**)」，而同文件 §1 所有权表只有 **6 行**、`eng/tests/api/test_p2_api.py:34-36` 恰检查 **6** 个对象 ⇒ 同一份冻结文档内 7 vs 6 两个数。**建议**：统一为 6（或补第 7 对象并同步测试）。

### 黄-4（在册 KNOWN_LIMITATIONS:104 #39）`docs/api/PHASE3_API_V1.md:18`
- **面**：④。文档宣称的 `schemas/phase3_request_v1.schema.json` 不存在（仓根与 eng/contracts 下均无此名）。**在册仍未修**。**建议**：补 schema 或改标 GAP。

### 黄-5 `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:94,103,211`
- **面**：④（含②）。两处「机器形态同步落在 schema」声明均不成立：
  - :103「`plane_id` 枚举同步落在 `eng/contracts/data/phase_product_exchange.schema.json`」——schema 内 enum = `["signal","support","variance","ivar","mask"]`，**无 sparse_snr**；:94 的 6 元集合与 :102 的 `sparse_snr_layer` 键在该 schema 中亦不存在（对该 schema `grep sparse_snr` 零命中）；
  - :211「`invalid_handling` 键同步落在 phase_product_exchange.schema.json」——该文件 `invalid_handling` **零命中**（该词仅见于 p3_resample.cpp/.h、DATA_SEMANTICS、NUMERIC_STANDARD 等别处）。
  **建议**：把键/枚举真正落进 schema，或把「同步落在」改为如实的计划落位/以本块为准表述。

### 黄-6 `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:285,286,288`
- **面**：④。验收映射引用三个不存在的测试符号：`test_missing_hash_rejected`、`test_missing_schema_rejected`（精确名）、`test_no_implicit_name_binding`——在 `eng/tests/artifact/test_phase_product_exchange.py` 中零命中（仅该文件 docstring :9/:12 提过旧名）。语义实际由 `test_manifest_missing_digest_rejected:164`、`test_manifest_bad_hash_rejected:172`、`test_artifact_id_arbitrary…:121`、`test_storage_uri_does_not_drive…:129` 覆盖。**建议**：改指真实用例名。

### 黄-7 `docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md:80`
- **面**：④。宏名 `ACS_FIO_MAX_CARDS` 不存在：实头为 `ACS_FIO_HEADER_MAX_CARDS`（`lib/infrastructure/aio/io/include/astrocs/io/fits_stream_v1.h:28`，值 1024 一致）；`grep ACS_FIO_MAX_CARDS` 全仓只命中本文档。**建议**：宏名订正。

### 黄-8 `docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md:174-186`
- **面**：④。§12 验收映射表把 9 行属性全部归到 `eng/tests/io/` 并给出不存在的模式名，且 trace 行**声称的断言强于实测**：
  - 模式 `fits_bad_header_* / fits_truncated_* / fits_mismatch_*` 在 `eng/tests/io/` 零命中；python 侧合同测试真名为 `test_bad_header_no_simple:315 / test_truncated:333 / test_mismatch:345`；
  - 取消/磁盘满/trace 覆盖**只存在于 C 自检** `lib/infrastructure/aio/io/tests/fits_core_selftest.c`（test_cancel:528、diskfull:591、缺 SIMPLE:217 / 缺 END:234、trunc:305、mismatch:338-346）；
  - :185「hook 计数累计与文件实际长度一致断言」——实测断言仅 `CHECK(g_cb_read > 0 && g_cb_write > 0)`（fits_core_selftest.c:134），是“被调用过”而非“与长度一致” ⇒ 判据强度宣称失实（判据弱化风险）。
  **建议**：测试名/落位改指真实文件与函数；trace 行按实际断言改写，或补强断言后另行裁决。

### 黄-9 `docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md:267,268`
- **面**：④。§8 引用两个不存在的测试符号：`test_tile_status_present_missing_invalid`（真名 `test_tile_status_present_missing`，test_hips_input_contract.py:328）、`test_moc_hint_optional_enum`（真名 `test_moc_optional_hint_absent`，:368）。覆盖核查：INVALID 状态实际在 layout 三测断言（:203/:222/:242），“有 MOC 枚举”在 status 测试段覆盖 ⇒ 语义有、符号错。**建议**：改真名。

### 黄-10 `docs/audit/risk_verification_T012.csv:2-8`
- **面**：④。全表 file:line 锚系统性失效（结论本身经复核仍成立）：
  - R1：`sampler.cpp:30` 实为 `#include <atomic>`（声称 `#if P2_ENABLE_OPENMP`）；`:620` 实为 SNR veto 注释（声称“串行默认”）；`coverage/CMakeLists.txt:18` 实为 `endif()`（声称 option OFF）、`:54` 实为 `kernel_registry.cpp`（声称 OpenMP link）；
  - R2：`stage2_common.cpp:391` 实为 `if (rj.contains("linear_fit"))`（声称 `acr_route auto`，真锚 :468）；`eng/tools/stage2.cpp` **不存在**（真件 `lib/algorithms/coverage/tools/stage2.cpp`）；
  - 反向复核（结论面）：P2_ENABLE_OPENMP 条件编译真实存在于 `acr_kernels.cpp:25/75/212`、`tools/stage2.cpp:25/1329/1543`；ACR-IVAR-001 禁用语义真实在 `stage2_common.cpp:468/516` ⇒ **结论无碍、锚全错**。
  **建议**：逐行重指真锚（审计证据 CSV 的锚必须可复开）。

### 黄-11 `docs/audit/inventory.csv:3,7`（行级判定错；整份角色问题在册 DB-11 G1）
- **面**：④。行 3（PSF）`tst_exists=False`，实测 `lib/algorithms/psf/tests` **存在**（含 p1psf/）——判定与现树相反；行 7（Drizzle/HiPS）`public_headers=lib/algorithms/drizzle/healpix_drizzle/include` 不存在（真件 `lib/algorithms/drizzle/include`，行内 `hdr_exists=False` 系按错误路径得出）。另：`src_files=` 计数与现树顶层数全部不符（声称 7/6/13/9/2/46/11/38 → 实测 11/7/14/16/4/32/23/6）。其余列（science_doc/algorithm_doc/tests/memory 路径）逐条存在性核验仅上述 1 处缺路径。**建议**：按现树重跑判定位并修正路径列。

### 黄-12 `docs/validation/SCIENCE_FREEZE.md:46-49`
- **面**：③。冻结声明 `PERFORMANCE_BASELINE = FINAL（真实 16 帧 Phase1：cold median 145.4s / warm 142.4s；Phase2 24.0-25.1s；Browser pan p50 34.7ms）`——这些数字**全仓仅此一处出现**（对 docs/artifacts/reports/独立审计 grep 145.4/142.4/34.7ms 零命中），而性能权威 `docs/performance/BASELINE.md` 登记的是另外两轮（V14: Phase1 65.0s、Phase2 GC 234.6s、pan 0.22s/f；V18R2: Phase1 ~67.35s/frame），二者互不对账；BASELINE.md 自带证据指针 `evidence/performance/*.json` 已悬空（在册 DB-12）。即：**被冻结为 FINAL 的性能基线数值无出处、与权威基线文档口径分叉**。
- **在册状态**：SCIENCE_FREEZE 其它问题在册 DB-13、BASELINE.md 悬空指针在册 DB-12；**本条“冻结数值与权威基线不对账”未登记**。
- **建议**：由负责人面裁决唯一基线数值并把原始测量出处落 `artifacts/evidence/`；涉冻结定义，本报告只登记不代裁。

### 黄-13 `docs/references/SCIENTIFIC_REFERENCES.md:92,93,94,107,108,123,124,164-172`
- **面**：②。条目编号 **61–67 被两套不同文献各用一次**，编号引用产生歧义：§J `61=IRAF combine.par / 62=ccdproc / 63=ZOGY`、§K `64=Holland&Welsch / 65=Kendall&Stuart`、§L `66=VanOosterom / 67=Starck&Murtagh` ↔ §N.2 `61=Zackay I / 62=Zackay II / 63=ZOGY / 64=Starck / 65=Rousseeuw / 66=Moffat / 67=Stetson`（67 与 §L:124 的 Starck 还重复登记）。**文献本身抽验无误**（见「已查无问题面·④」）。**建议**：§N.2 段改用 70+ 编号或字母后缀，一次性消除重号。

### 黄-14 `docs/owner/SCIENCE_OVERVIEW.md:133` + `docs/owner/PIPELINE_OVERVIEW.md:162`（与在册 G-1 同族，行未登记）
- **面**：②③。两份 owner 概览的**状态汇总块**仍写「冻结四投影 TAN/SIN/CAR/AIT registry IMPLEMENTED」：
  - SCIENCE_OVERVIEW 同文件 :103 已按 DOC-202 R26 订正「当前登记仅 TAN 已实现，v1 四行 registry 已 RETIRED」，§6 汇总 :133 未同步 ⇒ 同文互斥；
  - PIPELINE_OVERVIEW 同文件 :109 已注明「BASE 快照；现行 = 八投影、registry v3」，§7 汇总 :162 仍按四投影记 IMPLEMENTED ⇒ 同文互斥；
  - 与上位 `docs/ASTROCS_DESIGN.md` §6.3（设计 8 种、当前仅 TAN 可用）及 registry v3 实况（`lib/algorithms/projection/p3_proj_v6.cpp` 在役、`p3_projection_registry.h` 存在）不符。
- **在册状态**：四投影互斥家族已登记（DB-12 G-1 覆盖 `RELEASE_STATUS:93 ↔ SCIENCE_OVERVIEW:103`）；**这两处汇总行未登记、未随 R26 订正更新**。
- **建议**：两处汇总行同步 R26 口径（涉状态定义表述，交负责人面统一改）。

---

## 绿

### 绿-1 `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:306-310`
- **面**：②。第 11、12 两条中「**两者皆无时**显式 unavailable（不静默）」子句逐字重复一次（合并痕迹），方差条款前后两遍。内容无冲突，仅冗余。**建议**：合并为一句。

### 绿-2 `docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md:88`
- **面**：④。该行把 provenance 摘要重算的执行者写成 `validate_doc`，实际为 `validate_provenance`（`provenance.py:594`，由 `production_store.py:769` 调用）；`validate_doc` 属 ManifestValidator（校 manifest，不校 provenance）。符号均存在，仅调用方张冠李戴。**建议**：改名。

### 绿-3 `docs/references/SCIENTIFIC_REFERENCES.md:49,59`
- **面**：②。章节顺序为 F → **H(:49)** → **G(:59)** → I，字母序倒挂（H 在 G 前）。**建议**：交换两节或重排编号。

---

## 在册未修核对（本切片复核仍存在，不重复计数）

| 来源 | 锚 | 本轮复核 |
|---|---|---|
| KNOWN_LIMITATIONS #38 | `docs/api/PHASE3_API_V1.md:44` | 仍冲突（=红-1） |
| KNOWN_LIMITATIONS #39 | `docs/api/PHASE3_API_V1.md:18` | schema 仍缺（=黄-4） |
| AUD-101-DB01 G1 / DB-01 | `MANIFEST_VERIFY_V1.md` §3/§4 verify、show-effective | 仍为已删命令面（=红-2 之一部） |
| DB-05 同族 | CLI_PROTOCOL `resource_recorder.h:260-266` | 行号仍旧（=黄-2，本行未单列登记） |
| DB-11:613 | `ARCHITECTURE_OVERVIEW.md:64-65`、`PIPELINE_OVERVIEW` 的 `runtime/…` 缩写路径 | 真件在 `lib/infrastructure/{cli,pipeline}/`，文档未改（本批复核：`cli/runtime_client.cpp`→`lib/infrastructure/cli/runtime_client.cpp`、`runtime/pipeline/typed_dag.py`→`lib/infrastructure/pipeline/typed_dag.py`） |
| DB-11:512 | `PIPELINE_OVERVIEW.md:155` 自相矛盾（已删命令面 vs §1） | 仍在 |
| DB-12 | `SCIENCE_OVERVIEW.md:79 lib/phase1/`、`:106 runtime/io/fits_core.c`、悬空锚 :81/:103、:64-65 返回包证据 | 仍在（`ls lib/phase1` → 无；`find -name fits_core.c` 只回 `lib/infrastructure/aio/io/`） |
| DB-12 G-1 / 复核-负责人面 | `RELEASE_STATUS.md:93` ↔ `SCIENCE_OVERVIEW.md:103` 四投影互斥 | 仍在（同族新增行见黄-14） |
| DB-13 | `SCIENCE_FREEZE.md:5` `ACCEPTANCE_GATES.md` 悬空（全仓 find 无此文件，实为 `ACCEPTANCE_SPEC.md`）、:11-52 预写状态 | 仍在 |
| DB-13 | `TRACEABILITY_SPEC.md:21` schema 路径、`:94` §5 指针、`:110` returns/、`:134` 22 模块计数、`:70/:78` 正则 vs 小写示例、`:31` 8 层 vs 表 9 行 | 均仍在 |
| DB-11 G1 | `doc_classification.csv` 20 行指向已删文件仍写 present（docs/history/**、docs/backlog/**、4 个根级 md 均不存在；覆盖 95/233） | 仍在 |
| DB-12 | `BASELINE.md:14 evidence/performance/*` 悬空 | 仍在 |
| DB-11 1.9 | `risk_verification_T012.csv` 自造 7 种状态词 | 仍在（行锚问题为本批新证，见黄-10） |
| RULING-DOC-01 | run_manifest 双契约（CLI-003 vs CFG-001 schema） | 已裁决，本批仅对照、未重开 |
| DB-14/DB-15 | DATA-003/DATA-004 上游 §9→§10 条款号族 | 仍在，未重复计数 |

---

## 已查无问题面

### ① 科学性（对照分歧台账 D-01…D-11 终裁与非退化判据）
- **查法**：把本切片出现的每个数值/口径回查 D 系终裁表（分歧台账.md:238-248）与 docs/science 引文；涉公式/容差的表述只登记冲突、不改公式。
- **查过**：DATA-002 §2a invalid_handling 全规则块（合格样本定义、方差可用性独立通道、D_p/W_p 逐分支分母表、覆盖级 NaN、强制计数、六行可判定性）与 DATA_SEMANTICS §4a/§31.1a、DRIZZLE.md:83/:202 引用一致；`:141` 的 `Σ v w²/D_p²` 属「检查-修复验证.md:71」已裁**合法等价参数化**的 5 处之一，不重复上报；v6 六件冻结口径与机器源逐项复算（44 门=11/8/8/6/5/6、56=39/14/3，与 `artifacts/evidence/v6/qa-design/qa_matrix.json` gates 族计数及 `data/mutations.json` 前缀计数完全一致）；SCIENCE_FREEZE 排除面三条与 parser、`migrate_stage2_config.py`（存在）、`no_legacy_production_reference.py` 方向一致；PROJECT_SPEC §5-§8（w=SNR²/F_ref²、point information 对拍、权重=现场派生=A44、八投影当前冻结 TAN）与 D-系列终裁无冲突；三个 IO 契约均声明 scientific_change=false 且不改公式。
- **结论**：未见以 UNRESOLVED/待定充当科学结论；未见任何文档单方改动 sup、depth、idw_power、k_corr、样条裁剪等已裁量；非退化判据在 v6 mutation（56 条 rc!=0）与 traceability 空串判红规则中登记完整。

### ② 行文逻辑（断链、单文自相矛盾、订正注新旧冲突、UNRESOLVED 当结论）
- **查法**：docs/api 6 篇、docs/interfaces 6 篇逐行通读；traceability/validation/audit/references/owner/performance/browser 全量读或按节采读；每处“正文↔状态汇总↔表格”交叉对照；专查订正注记（DOC-001 溯源注、DOC-202 R26、A44 作废头）是否新旧并存。
- **查过**：全部 6 份 v6 档案的 DOC-001/A44 作废头与历史档案定位自洽（按历史标准判读，旧控制包路径已标注不作现状引用）；TRACEABILITY_SPEC/LAYERS/warn_baseline 的规则闭环（空串判红、baseline 只减不升、fixtures 六件在位）自洽；DATA-003/DATA-004/IO-003 的章节-符号-测试互指逐条可解析；SCIENTIFIC_REFERENCES 各节“勘误”自洽。
- **结论**：除已列 黄-3（7 vs 6）、黄-13（编号重号）、绿-1（重复子句）、绿-3（章节倒挂）、黄-14（汇总未随订正同步）、黄-12（数值无对账）外，**未见**把“未决”写成“已定”的第二例，**未见**订正注与正文直接互相否定的第二例；DB-13 的 8/9 层与正则矛盾属在册，未另计。

### ③ 跨文档冲突（五方权威链：docs/ASTROCS_DESIGN ↔ docs/ ↔ 实验/ ↔ 独立审计·08_修复包 ↔ 源码 lib|eng）
- **查法**：对每条“数量/状态/命令面/基线数值/枚举”做跨文件对照——CLI 命令面（CLI_PROTOCOL ↔ MANIFEST_VERIFY ↔ RELEASE_STATUS ↔ PIPELINE_OVERVIEW ↔ command_tree.h/测试）、run_manifest 双契约（RULING-DOC-01 已裁，仅对照）、cpu profile（MANIFEST_VERIFY ↔ parser/cpu_routing/profile_gen_v2）、四投影（design §6.3 ↔ owner 概览 ↔ projection 源码）、性能基线（SCIENCE_FREEZE ↔ performance/BASELINE ↔ evidence）、平面/枚举（DATA-002 ↔ exchange schema ↔ P3 实现）、traceability 层定义（SPEC ↔ LAYERS.csv ↔ schema ↔ checker）。
- **结论**：新冲突即红-1/红-2/红-4/黄-5/黄-12/黄-14；**已裁不再裁**——run_manifest 双契约（裁决 D）、DATA-002 上游 §9→§10 条款号族（DB-14）、VERSIONING（DB-11）、doc_classification 角色（T10 判历史快照）、D_p² 参数化（检查-修复验证:71）均未重开；DDL 级正面一致：run_manifest §2 字段序列与 commands.cpp 实现、manifest 校验序（语法3/status→8/version→5/config hash→3/artifact→3/sha256→8）与 §2 表逐项一致。

### ④ 幻觉与锚（题录 DOI/arXiv 抽验、file:line 实开、“文档说有、代码没接”）
- **题录抽验 6 组（全部通过）**：Newberry 1991 PASP 103,122 DOI `10.1086/132801`；Horne 1986 PASP 98,609 DOI `10.1086/131801`；Zackay&Ofek I ApJ 836,187 DOI `10.3847/1538-4357/836/2/187`（期刊题名 "How to COAAD Images…"，与 [arXiv:1512.06872](https://arxiv.org/abs/1512.06872) v1 题名不同属真实双版本，非错误）；[arXiv:1512.06879](https://arxiv.org/abs/1512.06879) 题名作者一致；Fernique 2015 A&A 578 A114（HiPS）；Rousseeuw&Croux 1993 JASA 88,1273 DOI `10.1080/01621459.1993.10476408`、Maples 2018 ApJS 238,2 DOI `10.3847/1538-4365/aad23d`、Van Oosterom 1983。出处：OpenAlex works API（DOI 解析）+ arXiv 检索 + 交叉源（卷页逐项比对）。
- **锚实开**：TRACEABILITY_MATRIX 30 行 ×（science_doc/algorithm_doc/src_path::symbol 符号逐个命中/test_path）**去注解后 0 缺失**；CSV 30 行 ↔ JSON modules 集合完全同构；base_main_sha `0d32c07d…` `git cat-file -e` 通过；schema/checker/fixtures(6)/test 全在。v6 负例：56 条 mutation、44 门、P0-01..P0-06、10 个 driver（含 gone_artifacts 登记）逐个核验。docs/audit inventory 全路径（仅黄-11 两处）、risk_verification 行锚重开（黄-10）、doc_classification 95 行存在性。docs/interfaces 6 篇全部代码符号与测试类（IO_003 九 Test 类 9/9、DATA-003 30 测试、DATA-004 61 测试/8 类、IO_002 十七个 test 函数、PHASE1/2/3 符号）逐一 grep。owner 5 篇、performance 2 篇、browser 1 篇代码锚（stf_engine.h、DisplayTransformState、--stf-mode/--lock-stf/--stf-bench/--stf-lock-probe、p3_projection_registry.h、check_module_map.py、resource_gate_v1.json 等）核验通过。
- **“文档说有、代码没接”专项**：确认 5 例（红-2 §5 main.cpp 三符号、红-3 cpu profile 旧字段+死解析器、黄-5 两处 schema 同步声明、黄-8 trace 断言强于实现、黄-9 两测试符号）；**未发现**新的死配置键（ENG-002 文档键与 parser.cpp:708-715 白名单一致）、新的 schema-文档漂移（CLI_PROTOCOL 11 退出码=exit_codes.h、kEventFieldsV1/10 类 kind=protocol.h、CSV 20 列=resource_recorder.h:289-293 内容一致）、新的第三方接口虚接（fits_stream_v1.h 错误码 0-13、hips_input_v1.h 0-11 与文档逐位一致）。
