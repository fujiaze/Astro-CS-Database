# 全仓排查 S30 ｜ eng/（合同 schema ＋ CI 注册表 ＋ 工具）

**切片**：S30　**四面**：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚（含「文档说有、代码没接」专项）
**性质**：只读对抗静态审查。本文件是本切片唯一写盘产物；零构建、零测试、零 git 写入。
**快照**：HEAD = dfe44ac2（docs: 根 README 参考面按 R3 一手核验增补）。审查期间仓库由并发轮次持续推进（e42ca51e → 01c3666a → dfe44ac2，并有 5 份未提交改动：docs/DOCUMENT_GOVERNANCE.md、docs/science/{PHASE2_UPM,PHOTOMETRY,REJECTION,UNCERTAINTY_AND_COVARIANCE}.md、eng/tools/run_keep.txt），凡随 HEAD 漂移的计数均注明取数时点。
**前置阅读**：独立审计/实验重做/总编对账/检查-修复验证.md（PASS 表，104 行）、独立审计/实验重做/总编对账/分歧台账.md（D-01…D-11 终裁）——表内已判 PASS/FAIL 的项一律不重复登记；科学口径争议一律从 D 系终裁，不在本片重开。
**兄弟切片去重**：S9-01/02/03/05/07/08/09、S12、S23 §四、独立审计/证据/复核-合同层.md 已覆盖者只做交叉引用，不重复计数（见 §五）。

**计数：红 7 ／ 黄 14 ／ 绿 16**

---

## 一、红级（7）

### 红-01 ｜红｜④
**文件:行**：eng/contracts/config/module_dll_contract.schema.json:180
**问题描述**：合同 schema 含非法转义 \. ，整个文件不可被 JSON 解析器读入；直接把注册表里 fast/linux-main/windows-main、waivable:false 的 P0 锚门打成基础设施红（rc=2），该门的全部判据一个都跑不到。
**证据**：
- python3 -c "import json; json.load(open('eng/contracts/config/module_dll_contract.schema.json'))" ⇒ JSONDecodeError: Invalid \escape: line 180 column 85 (char 6796)；
- python3 eng/ci/check_registration_anchors.py ⇒ rc=2，输出唯一一行 CHK-REGISTRATION-ANCHORS_FAIL: ANCHOR_UNPARSABLE: eng/contracts/config/module_dll_contract.schema.json: Invalid \escape: line 180 column 85 (char 6796)；
- 把该文件临时排除后同一条命令 ⇒ contract_files=83, ci_files=35, refs_checked=126, exempt_doc_index=1、原始 31 findings / 5 组，全部被 eng/ci/ledgers/registration_anchor_ledger.json（5 条）收编、残差 0 ⇒ 证明红灯完全由这个解析错误引起，不是判据本身有真阳性；
- python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict ⇒ rc=0 / error_count:0 / verdict:PASS ⇒ 解析器只校验 checks.json，没有任何规则读 eng/contracts/**/*.json 的可解析性，故 rc=2 是运行期才暴露的盲区。
- 反方核验：① 该问题在 独立审计/证据/复核-合同层.md:549 已以 W3-c P1 记过，但不在 PASS 表内、也无任何兄弟切片在 全仓-01/ 报过（grep -rn 'module_dll_contract' 独立审计/排查/全仓-01/*.md 零命中），属未闭合项；② 与「仓内改一处即可修」的表象相反——AGENTS.md §6 明写 eng/contracts/ 冻结 schema 属禁改清单（docs/DOCUMENT_GOVERNANCE.md:63 同），故只登记不擅改，与本切片纪律一致。
**建议改法**：走变更流程把 :180 的 \. 改为 \\ 或改用合法转义；同时给 validate_registry 补一条 R 规则：eng/contracts/**/*.json 与 eng/ci/*.json 必须可解析，否则 rc=2 且点名文件——把「读不进来」从运行期蓝屏提前到注册期红灯。
**所属面**：④

---

### 红-02 ｜红｜③④
**文件:行**：docs/owner/RELEASE_STATUS.md:83 ↔ eng/tools/check_agents_gov.py:63（DESIGN）、:84（SELF_AUTHORITY_RE）、:338（行级豁免）↔ docs/DOCUMENT_GOVERNANCE.md:61（§4.2 规则 2）
**问题描述**：P0 主门 AGENTS-GOV 在当前 HEAD 实跑 rc=1 GOV_CHECK_FAIL；同一工具的 --self-test 也 rc=1（16 例中 N5 失败），而这个自测根本没注册进 checks.json，CI 永远看不到它的红。根因是 DOC-REORG 批次1（e42ca51e）把行级豁免的判定字面量从裸名改成 docs/ASTROCS_DESIGN.md，却（按其自己写的迁移规则也不该）没有改写 docs 内部的裸名引用 ⇒ 检查器判据与迁移规则互斥。
**证据**：
- python3 eng/tools/check_agents_gov.py ⇒ rc=1，verdict=GOV_CHECK_FAIL，唯一违规 single_authority_entry | docs/owner/RELEASE_STATUS.md:83 以非最高权威文档自称唯一最高: | 最高设计 | CONTRACT_READY | ASTROCS_DESIGN.md（§0 权威链，唯一最高权威） | ；unique_authority_scanned=276 / scan_face=276（扫描面非空，fail-closed 不是诱因）；
- 该行语义是点名 ASTROCS_DESIGN.md 为最高权威，属豁免本意；豁免在 :338 写作 if "docs/ASTROCS_DESIGN.md" in line: continue，而该行只有裸名 ASTROCS_DESIGN.md ⇒ 不豁免；
- 历史翻转点（反方核验 ①）：git show e42ca51e^:eng/tools/check_agents_gov.py | sed -n '337,339p' ⇒ 原文「# 唯一行内豁免：逐字点名现行最高设计 ASTROCS_DESIGN.md。」+ if "ASTROCS_DESIGN.md" in line: ⇒ 改前该行豁免、门绿；git show e42ca51e --stat | grep -c 'docs/owner/' ⇒ 0，即该 commit 未触碰此文件；grep -c 'docs/ASTROCS_DESIGN.md' docs/owner/RELEASE_STATUS.md ⇒ 0；
- 两侧规则互斥（③核心）：git show HEAD:docs/DOCUMENT_GOVERNANCE.md 第 61 行逐字为「引用重写规则表（脚本化）：docs 外引用 docs/X/ → 新路径；docs 内相对引用按新拓扑改写；ASTROCS_DESIGN.md 裸名引用 → docs 外加 docs/ 前缀、docs 内保持裸名」⇒ docs/owner/RELEASE_STATUS.md 在 docs 内，写裸名是合规的；check_agents_gov.py:338 却要求 docs/ 前缀才豁免 ⇒ 检查器与它要保护的迁移规则互相打脸；
- 自测红（④）：python3 eng/tools/check_agents_gov.py --self-test ⇒ rc=1、all_pass:false、positive_cases=6 / negative_cases=10，失败用例恰一条「N5 唯一权威扫描面为空 ⇒ fail-closed | want= unique_authority_scan_empty | scanned=1」；
  机理：夹具 _mini_repo（:542-551）现在写 docs/ASTROCS_DESIGN.md，而 N5（:631-637）只删根目录 4 份 ⇒ 扫描面（:304-315 含 docs/** 仍剩 1 份）永不为空，scanned==0 ⇒ fail-closed（:345-348）分支不可达 ⇒ fail-closed 正例变成永远不可触发的死例；
- 自测未接线（④）：AGENTS-GOV 无同名前缀兄弟 ID、注册步骤里无 --self-test ⇒ 红在本地暴露，CI 面不可见；
- 反方核验 ②：CI 产物 artifacts/ci/*/CI_RESULT.json 09-17 的运行记录 AGENTS-GOV: PASS ⇒ 不是「一直红、一直被忽略」，是 e42ca51e（09-27 15:14）新引入的回归；全仓 471 处裸 ASTROCS_DESIGN.md 中只有这一处与 SELF_AUTHORITY_RE = r"唯一\s*最高(权威|约束|规范|文档)"（:84）共现，故违规只有 1 条而非 471 条。
**建议改法**：check_agents_gov.py:338 的豁免改为「该行点名裸名 ASTROCS_DESIGN.md 或 docs/ASTROCS_DESIGN.md 均豁免」（与 DOCUMENT_GOVERNANCE.md:61 对齐）；N5 夹具补删 docs/ASTROCS_DESIGN.md 使 scanned==0 可达；给 AGENTS-GOV 补一条 -SELFTEST 兄弟注册项，让 16 例自测进 CI。本轮只登记不擅改。
**所属面**：③④

---

### 红-03 ｜红｜③④
**文件:行**：docs/DOCUMENT_INDEX.yaml ↔ eng/tools/doccheck/check_doc_index.py:356-358（has_upstream_header）、:470、:500-503、:285-293（scan_dangling）↔ docs/DOCUMENT_GOVERNANCE.md:61-62
**问题描述**：P0 门 DOC-INDEX --strict rc=1 DOC_INDEX_FAIL，其负例面 DOC-INDEX-SELFTEST 同样 rc=3（24 例中 4 例正例失败）。四面里有两面是兄弟切片没报过的新面：subordinate_docs_upstream_header（缺抬头 258 条）与 docs_path_refs_resolve（未登记悬空 11 条）；另有第二份未登记文档 docs/DOCUMENT_GOVERNANCE.md（S9-01 只点了 CCD 那一份）。
**证据**：
- python3 eng/tools/doccheck/check_doc_index.py --strict ⇒ rc=1 / verdict: DOC_INDEX_FAIL，四面全红（取数 HEAD dfe44ac2）：
  - docs_fully_covered：未覆盖 38 份（docs/DOCUMENT_GOVERNANCE.md + 37 份新落库 docs/**/README.md）；
  - subordinate_docs_registered：未登记 38 份（同上）；
  - subordinate_docs_upstream_header：缺抬头 258 份（首 10 条 docs/ASTROCS_DESIGN.md / DEVELOPER_GUIDE / DOCUMENT_GOVERNANCE / GLOSSARY / KNOWN_LIMITATIONS / README / TROUBLESHOOTING / VERSIONING / algorithms/ACR_EQUIVALENCE / …）；
  - docs_path_refs_resolve：未登记悬空 11 条（docs/DOCUMENT_GOVERNANCE.md:61 docs/X/、eng/ci/check_naming_surface.py:410/:419/:451/:453/:454 docs/a.md、:491 docs/nowhere/*、check_budget_single_source.py:277 docs/other.md 等）；
- 新面机理（③核心）：e42ca51e 把 return any(("上游" in ln and "ASTROCS_DESIGN.md" in ln) 改成 return any(("上游" in ln and "docs/ASTROCS_DESIGN.md" in ln)；静态复算当前 271 份 tracked docs/**.md 抬头形态：旧规则（裸名）通过 232 份 / 新规则（docs/ 前缀）通过 0 份，两者都失败 39 份（README 类与 docs/ASTROCS_DESIGN.md 自身）⇒ 该面由「基本全绿」翻成「全红」；而 DOCUMENT_GOVERNANCE.md:61 写的是「docs 内保持裸名」⇒ 检查器要求的形态恰好是迁移规则禁止产生的形态；
- 夹具侧同因（④）：python3 eng/tools/doccheck/check_doc_index.py --self-test ⇒ rc=3 / SELFTEST_FAIL: 4/24 例不符预期，失败 4 例全部是「期望 rc=0」的正例（S0-positive-legal-index、S4-retired-declared-archived、S14b-current-control-pack-ok、S16-ledger-resolved-still-green），失败面恒为 ['docs_fully_covered','subordinate_docs_registered','subordinate_docs_upstream_header']；
  复算夹具本体：ROOT_DOC_FILES（:829-838）的键已被改成 docs/ASTROCS_DESIGN.md，而 BASE_INDEX（:839-853）仍只登记 docs/owner、docs/ci/01_CHECKS.md、docs/DOC-L0.md ⇒ 夹具自己造出一份未登记、无抬头的文档，正例永不可能绿 ⇒ 该 P0 自测门同样红；
  旁证：S17/S18/S19 三条 PASS 行的 red=[...] 里恒含这三面 ⇒ 夹具与真仓同构带病；
- 索引缺口新增（③）：git show HEAD:docs/DOCUMENT_INDEX.yaml | grep -c DOCUMENT_GOVERNANCE ⇒ 0；该文件由 e42ca51e（git log --diff-filter=A -- docs/DOCUMENT_GOVERNANCE.md ⇒ 唯一命中该 commit）新增入库，其 :62 自己写着「DOCUMENT_INDEX.yaml 与 TRACEABILITY.csv 同批重生成；--strict 校验通过才算批次完成」⇒ 批次1 违反了它自己写下的完成判据；
- 反方核验：① 兄弟覆盖核查——grep -rn '缺抬头|upstream_header|path_refs_resolve' 独立审计/排查/全仓-01/*.md ⇒ 零命中；检查-修复验证.md 对 DOC-INDEX/抬头 ⇒ 零命中 ⇒ 两新面确属本片首报；S9-01 只覆盖 docs_fully_covered + subordinate_docs_registered 且只点名 CCD 一份，本条不重复计其数，只接续 38/38 的现值与两新面；② 本会话早期（e42ca51e 刚落库时）同一命令读数为 222/8，dfe44ac2 时为 258/11，增量来自并发轮次 dc986c82 / 01c3666a 落库的全目录 README.md 未同步进索引——红是结构性的，不随并发修复消解；③ 该门 waivable=false、profiles=fast/linux-main/windows-main ⇒ 不可能被 SKIP 绕过，红是真红。
**建议改法**：二选一并全局对齐——(a) has_upstream_header 改回接受裸名（与 DOCUMENT_GOVERNANCE.md:61 一致），或 (b) 按「docs 外加前缀」的镜像规则把 232 份 docs 内抬头统一改成 docs/ASTROCS_DESIGN.md 并同步改 :61；无论方向，BASE_INDEX 补登 docs/ASTROCS_DESIGN.md、DOCUMENT_INDEX.yaml 补登 DOCUMENT_GOVERNANCE.md 与 37 份 README；11 条代码侧 docs/a.md、docs/X/ 悬空多为检查器自身夹具字符串，改判据（如 TEMPLATE_MARK）或进台账。
**所属面**：③④

---

### 红-04 ｜红｜④
**文件:行**：docs/validation/v6/{QA_MATRIX,ORACLE_AND_ZERO_CASE_POLICY,BASELINE_COMPARISON_MATRIX,NEGATIVE_MUTATION_CATALOG,P0_GATE_FAMILY,README}.md ↔ artifacts/evidence/v6/**
**问题描述**：v6 验收文档里 26 处 reports/v6/** 引用全部悬空（16 个不同 token），其中含「机器唯一事实源」「复跑入口」这类可执行指针；搬迁早已发生，没有任何机器门能看见这些引用。
**证据**：
- reports/ 下只有 v19r2，ls reports/v6 ⇒ 不存在；git log --oneline --diff-filter=D -- 'reports/v6' ⇒ 9a2b5d11 refactor: 根目录整合（负责人 2026-09-21 直接指令）…，同 commit 的 git log --diff-filter=A -- 'artifacts/evidence/v6/…' ⇒ 同一 9a2b5d11 把 v6 证据树搬进 artifacts/evidence/v6/**；
- 逐 token 复算：26 次出现 / 16 个不同 token，其中 13 个能在 artifacts/evidence/v6/** 找到对应件（review-audit/01..06、qa-design/qa_matrix.json、report/REPORT.md、report/EXECUTIVE_SUMMARY.md 等）；3 个找不到——reports/v6/review-audit/03（QA_MATRIX.md:834/:842，残缺文件名，真名是 03_缺陷账本_重开清单.md）与 reports/v6/qa-design/evidence/{oracle_baseline.json、reports/v6/qa-design/oracle/{qa_oracle（花括号组被 token 正则切开，展开后其实都存在）；
- 文件与行：QA_MATRIX.md:9/:10/:97-102/:104/:834/:842、ORACLE_AND_ZERO_CASE_POLICY.md:7-8、BASELINE_COMPARISON_MATRIX.md:7、NEGATIVE_MUTATION_CATALOG.md:7-8、P0_GATE_FAMILY.md:9、README.md:16/:27；其中 QA_MATRIX.md 写着「复跑入口 python3 reports/v6/qa-design/oracle/run_all.py」、README.md 写着「机器唯一事实源」——按字面执行必然 No such file；
- 为什么机器看不见（④核心）：check_doc_index.py 的 TOKEN_RE = (?<![A-Za-z0-9_./-])docs/[A-Za-z0-9_./-]* （:155）只匹配 docs/…，reports/v6/** 不在扫描面；SKIP_DIRS（:99）又显式跳过 reports/、artifacts/ ⇒ 双重盲区；全仓 grep -rn 'reports/v6' docs/ 除 docs/validation/v6/ 自身外 0 命中 ⇒ 也没有别的文档交叉引用兜底；
- 反方核验：① 非「文件真丢」——13/16 的对应件在 artifacts/evidence/v6/** 活着，属搬迁未改引用，不是证据灭失；② 不在 PASS 表（grep -n 'reports/v6|DOC-INDEX' 检查-修复验证.md ⇒ 零命中）、不在兄弟切片（grep -rn 'reports/v6' 独立审计/排查/全仓-01/*.md ⇒ 零命中）；③ docs/validation/v6/*.md 全部是 ACTIVE_NORMATIVE（DOCUMENT_INDEX.yaml 登记），不是归档件，按规则必须保持锚存活。
**建议改法**：按 9a2b5d11 的映射把 26 处 reports/v6/ 批量改写为 artifacts/evidence/v6/，QA_MATRIX.md:834/:842 的 …/review-audit/03 补全为 03_缺陷账本_重开清单.md；并给 check_doc_index 的 TOKEN_RE 增补非 docs/ 前缀的仓内路径族（至少 reports/、artifacts/、run/），或另立一条「仓内路径锚存活」判据，否则同类搬迁仍会静默复发。
**所属面**：④

---

### 红-05 ｜红｜③④
**文件:行**：eng/contracts/data/v6_clause_registry_v1.json:911-917、:2043-2050、:2786-2793 ↔ docs/contracts/DATA_SEMANTICS.md:1739、docs/validation/v6/QA_MATRIX.md:452、docs/plugins/algorithms_phase2/10_sampling.md:48
**问题描述**：交接件 §10 点名的遗留项核实成立（只登记不擅改）：条款登记 JSON 仍写旧 k_corr 单因子口径，与 D-08 终裁和全仓现行文档口径冲突；同一条款的 source_binding.locator 指向已不存在的文本；放大看，53 条带 source_binding 的条款里 43 条锚失效，而全仓没有任何机器校验。
**证据**：
- 交接件原文：独立审计/交接/下一阶段-海量排查与修复-交接.md:99：「1. eng/contracts 条款登记 JSON 两处旧 k_corr 语义文本（:912/:2786 附近）——走变更流程的候选，本轮只核实现状并在台账登记，不擅自改。」
- 两处旧语义（行号在 dfe44ac2 复核仍准）：
  - :912（条款 FZ-PROV-KCORR 的 value）："定义 k_corr=Var(median)/[pi sigma_bg^2/(2 N_retained)]; … 未复跑前仅域内用 1.4; 按尺度查找表保留"；
  - :2792（SUP-10.suggested_wording）："…必须补冻结定义 k_corr=Var(median)/[π σ_bg²/(2 N_retained)]…"（:2786 是同条 statement）；
  - 现行口径（D-08 终裁：k_corr = k_gauss(N_retained) × k_geo 两因子查表，1.4 = 代码默认/域外回退，MC 实测 1.3883）：docs/contracts/DATA_SEMANTICS.md:1739、docs/plugins/algorithms_phase2/10_sampling.md:48、docs/validation/v6/QA_MATRIX.md:452 三处全部已是两因子式，且 grep -rln 'k_gauss' docs/ 命中 8 份文档 ⇒ 文档层已统一，JSON 层是唯一残留的旧口径；
- 死 locator：:2043-2050（FZ-PROV-KCORR-VALUE）source_binding.locator = "未复跑标定前"、file = docs/contracts/DATA_SEMANTICS.md ⇒ grep -c '未复跑标定前' docs/contracts/DATA_SEMANTICS.md ⇒ 0；
- 系统性 43/53（放大面）：逐条复算 v6_clause_registry_v1.json 的 96 条 clauses ⇒ 带 source_binding 的 53 条（kind: substring 35 / md_table_row 18）、file 缺失 0、locator 或 contains 在目标文件中不成立 43 条（例：PSFSW-T-DEPTH 等 16 条 PSF 门锚 docs/science/PSF_SIGNAL_WEIGHT.md 的 ID 字面量在该文件中根本不存在；FZ-AP2S-RANK-RTOL 等 4 条锚 docs/science/algorithms/UPM_SOLVER.md 同样缺失；FZ-CAL-FLOOR 等锚 docs/contracts/DATA_SEMANTICS.md 的 max(f_p, 0.1)、default 1 ADU 缺失）；
- 零机器校验（④核心）：grep -rn 'source_binding' eng/ --include=*.py ⇒ 仅 2 个文件，且唯一消费点是 eng/tests/contracts/product_family/test_field_constraints_integration.py:165-174：「"""49 条待签条款的锚不得悬空：source_binding.file 必须存在。"""」——只查 file 存在性，locator/contains 从不校验；
  grep -rn 'source_binding' docs/ ⇒ 仅 1 份命中且是 CONFIG_CONTRACT.md 里无关的 resource_binding ⇒ source_binding 字段语义在文档层零定义；
- 反方核验：① 不是「条款不存在」——96 条 ID 计数自洽（FROZEN/… 分档 39/49/8），file 存在率 53/53；② 不在 PASS 表：检查-修复验证.md 对 k_corr 的 5 条（红-4/红-8/黄-2/黄-5/黄-12）全部点名 .md 论文/规范文件，无一条点 v6_clause_registry；③ 兄弟切片只把该 JSON 当引用源提及（S27:331 weight_vocabulary、S28:151-152 ALG-P1-001/CAL-UNIT、S7:160、S29:53），没有一条报过 k_corr 或 source_binding；④ 同时在飞的并发轮次正在把 .md 里的「订正/D-08」注释按 DOCUMENT_GOVERNANCE.md §5 正向设计纪律清除（git diff docs/science/ 可见），但没有任何提交触碰 v6_clause_registry_v1.json ⇒ 残留不因并发改动消解。
**建议改法**：按交接 §10 只登记——:912/:2792 的 k_corr 文本、:2043 的死 locator 提变更单，口径以 D-08 终裁 + DATA_SEMANTICS.md:1739 为准；source_binding 补一条机器校验（locator 字面量存在 + contains 字面量存在，md_table_row 按行锚而非全文件子串），先把 43 条红出来再逐条收口；字段语义写进 docs/contracts/ 说明层。
**所属面**：③④

---

### 红-06 ｜红｜④
**文件:行**：docs/ci/01_CHECKS.md:10-11、ENGINEERING_SPEC.md:181 ↔ eng/ci/checks.json（150 项）
**问题描述**：规范两处用「必须」写死负例入口要求，且明写「检查器的注册步骤中必须能看到该入口」；实测 150 项中 72 项的注册步骤里看不到 --self-test/--fault-inject，其中 27 项的检查器本身实现了 --self-test 却没有任何注册步骤调用它（典型「文档说有、代码没接」），而 validate_registry.py 的 R1–R15 没有一条规则校验这个面。
**证据**：
- 规则原文：docs/ci/01_CHECKS.md:10-11「可执行负例面（ENGINEERING_SPEC.md §8）：每项检查必须提供机器可执行的负例入口（--self-test 或 --fault-inject）；仅有人工说明不算。检查器的注册步骤中必须能看到该入口」；ENGINEERING_SPEC.md:181 同义；
- 按该判据逐项复算（dfe44ac2，以「本项 command + steps[].command」全体为注册步骤）：
  - 注册表总数 150；
  - 自身步骤含 --self-test/--fault-inject（DIRECT）58；
  - 无、但有同前缀兄弟 ID <ID>-* 提供（SIBLING）20；
  - 两者皆无（违反 :11 字面要求）72；
  - 其中 ID 自带 SELFTEST/NEG 字样（形式满足）5；由 unittest/ctest 载体承载负例 24；检查器实现了 --self-test 但零注册步骤调用且无 unittest/ctest 载体 27；
- 27 项全名（均 waivable:false）：CHK-WARN、ALG-LINE-ANCHORS、CHK-STALE-DOC、API-DOCS、AGENTS-GOV、ENG-CONSTRAINTS、VERSION-CONSISTENCY、VERSION-NAMESPACES、DOC-L0、GLOSSARY-DOCS、CHK-PATH-DOMAIN-ANCHORS、CHK-E2E-REPRO、CHK-ALGO-WIRING、CHK-REGISTRY-IR-PARITY、CHK-CONFIG-CONSUMED、CHK-CONFIG-DEFAULTS、CHK-PROD-SCALE、CHK-PROVENANCE-CONSISTENCY、CHK-AIO-IO-BOUNDARY、L2-FROZEN-GATE-REPLAY、WORKER-BALANCE-METRIC-REPLAY、CHK-TEST-DISCRIMINATIVE、CHK-SPARSE-PUNCH-PROBE、CHK-MUTATION-GATES、CHK-BASELINE-OPCODES、CHK-LINK-SCAN、CHK-PROD-WIRING；
- 调用点复核（反方核验 ①）：grep -rn 'check_agents_gov.py --self-test|check_config_consumed.py --self-test|check_mutation_gates.py --self-test' --include=*.py --include=*.md . ⇒ 仅命中工具自身 docstring 与 eng/ci/ID_MIGRATION_MAP.md:475 的说明文字 ⇒ 确无任何执行步骤调用；且这些 --self-test 是活的（本片实跑 check_config_consumed/check_mutation_gates/check_prod_wiring/check_frozen_gate 的 --self-test 全 PASS）⇒ 不是「没实现」，是实现了没接线；
- 无强制规则（④核心）：grep -n 'self-test|fault-inject|negative' eng/ci/validate_registry.py ⇒ 零命中；R1–R15（:10 起）覆盖字段类型、ID 治理、迁移映射、豁免登记，唯独不覆盖规范自己点名的这个面 ⇒ 72 项违规对机器不可见；
- 反方核验 ②（对严重级的校准）：① 58+20=78 项确已满足，规范不是全体落空；② 24 项由 unittest/ctest 承载，属等价负例面（如 CHK-REGISTRY-VALIDATE 由 eng/ci/tests/test_negative_guards.py 覆盖、CHK-CONTRACT-REF 由 test_contract_graph_negative.py 覆盖），故只有 27 项是「既无注册步骤入口、又无测试载体」的真空白；③ 16 项完全无自测面者多为聚合/构建包装步（CHK-BUILD-LINUX/WIN、CHK-SANITIZER、CHK-COVERAGE、LINUX-MAIN-BUILD-TREE 等），风险低于那 27 项。
**建议改法**：给 validate_registry 补 R 规则：每项必须满足「自身步骤含负例入口 ∨ 同前缀兄弟项含 ∨ 注册的 unittest/ctest 步骤点名其检查器」，否则 rc≠0 并点名 ID；对 27 项逐个补一条 --self-test 步骤（工具侧都已就绪，只需在 checks.json 加 step）。
**所属面**：④

---

### 红-07 ｜红｜④
**文件:行**：eng/ci/polarity_evidence.json:11/:1273/:1277、eng/ci/polarity_probe.py ↔ docs/ci/01_CHECKS.md:10-11 / ENGINEERING_SPEC.md:181
**问题描述**：为「负例面」这个规范要求专门建的机器台账自相矛盾：头部声明已覆盖 150 项，实际只有 45 条记录、105 项 missing_records；其自带的 --check 模式实跑 rc=1 POLARITY_LEDGER_FAIL；而产生这个红灯的探测器根本没注册进 checks.json ⇒ 红灯不进 CI、也不进任何报告。
**证据**：
- python3 eng/ci/polarity_probe.py --check ⇒ rc=1，首行 POLARITY_LEDGER_FAIL；
- eng/ci/polarity_evidence.json：:11 "registry_entry_count": 150、:1273 "gates_total": 150，而 gates 实际只有 45 条（PROVEN-EXECUTABLE 22 / RE-JUDGED 4 / FACE-DEFINED-NOT-RUN 19），:1277 "missing_records" 有 105 条；extra_records: []（无多登项）；
- 未接线（④核心）：grep -rn 'polarity_probe|polarity_evidence' eng/ci/checks.json docs/ci/*.md ⇒ checks.json 零命中（01_CHECKS.md:233 提到的是另一个工具 check_spec_polarity_consistency.py）⇒ 该门与该红灯都在 CI 之外；
- 反方核验：① gates_total=150 与 registry_entry_count=150 数字本身与 checks.json 真实 150 项对得上，说明台账是按全量建的、只是只填了 45 条——不是统计口径错，是覆盖缺口；② --check 的 rc=1 是它自己的契约（「每个注册项都有极性记录」），不是误报；③ P0_GATE_FAMILY.md:33 也写着「建议 RUNTIME-CI-001（W9）把 P0-01..P0-06 接入 CI」⇒ 文档层已承认这批门尚未接线，与实测互证；④ 不在 PASS 表、不在兄弟切片（grep -rn 'polarity' 独立审计/排查/全仓-01/*.md 零命中）。
**建议改法**：补登 105 条 missing_records（或把 gates_total 改为实际口径并说明差额）；把 python3 eng/ci/polarity_probe.py --check 注册为独立检查项（建议 waivable:false），使该红灯能被 CI 看见。
**所属面**：④

---

## 二、黄级（14）

### 黄-01 ｜黄｜④
**文件:行**：docs/ci/01_CHECKS.md:229
**问题描述**：判据落点表写 CHK-CTEST-REGISTRATION 且状态「在册」，但该 ID 在 checks.json 中不存在——是幻觉 ID；真实落点是 step CTEST-REGISTRATION（属 CHK-MODULE-MANIFEST）。
**证据**：遍历 150 id ⇒ CHK-CTEST-REGISTRATION False；真实命令 python3 eng/tools/quality/check_ctest_registration.py --output run/ci/ctest-registration/ctest_registration.json（step CTEST-REGISTRATION，under CHK-MODULE-MANIFEST），另有 CI-CONTRACT-CTEST-REGISTRATION under CHK-CI-CONTRACT-SELFTESTS；grep -rn 'CHK-CTEST-REGISTRATION' . ⇒ 全仓仅 01_CHECKS.md:229 一处。
**为什么机器抓不到（反方核验）**：eng/ci/check_registry_doc_sync.py:70-73 的 parse_doc 只切「## 2. 检查项清单 → ### 2.1」「### 2.1 → ### 2.2/## 3.」「### 2.3」三段；:229 属 §2.1.1 落点表，不在任何解析片内；实跑 python3 eng/ci/check_registry_doc_sync.py ⇒ rc=1 但输出只有 registered_not_documented（8 项 = S9-02），没有任何 documented_not_registered 命中 CHK-CTEST-REGISTRATION ⇒ 幻觉 ID 对双向同步门不可见。
**建议改法**：:229 落点改写为 CHK-MODULE-MANIFEST / step CTEST-REGISTRATION（维护面 eng/tools/quality/check_ctest_registration.py --write-baseline）；同步门补一条「判据落点表 ID 列必须存在于 checks.json」。
**所属面**：④

---

### 黄-02 ｜黄｜③
**文件:行**：docs/ci/01_CHECKS.md:230 ↔ :139-140 ↔ eng/ci/checks.json
**问题描述**：同一文件内自相矛盾——:230 说 CHK-REGISTRATION-ANCHORS「实现已落地，注册项待写入」，而该 ID 早已注册且同文件 :139-140 已按 §2 正常登记。
**证据**：CHK-REGISTRATION-ANCHORS 在 checks.json ⇒ True；CHK-REGISTRATION-ANCHORS-SELFTEST ⇒ True；grep -n 'CHK-REGISTRATION-ANCHORS' docs/ci/01_CHECKS.md ⇒ :139/:140 两行正常登记 + :230 一处「待写入」。
**反方核验**：与 S9-02 的 :239（SPEC-POLARITY-CONSIST-01 的「注册项待写入」）是不同行、不同 ID、不同问题，S9-02 明确只点名 :239 与 8 个未登记 ID，不含 :230 ⇒ 本条为新发现，不重复计数。
**建议改法**：:230 状态列改「已注册（fast/linux-main/windows-main，waivable=false）」。
**所属面**：③

---

### 黄-03 ｜黄｜③④
**文件:行**：docs/ci/CI_SPEC.md:83、docs/ci/02_PIPELINE.md:54 ↔ eng/ci/checks.json ↔ eng/ci/checks.schema.json:63-66/:204-207 ↔ eng/ci/validate_registry.py:286-288/:342-344
**问题描述**：两处文档写死「step 硬上限 3600 s（prerelease 重步骤 10800 s）」，实测 3 个 step = 7200 s 超限、14 个顶层 timeout_seconds > 3600（3 个 >10800）；schema 只写 minimum:1，校验器 R2 只查 >= 1 ⇒ 上限在两层都没有任何机器强制。
**证据**：
- 文档原文：CI_SPEC.md:83「每个 step 的 timeout_seconds ≤ max(60, 3 × 最近一次实测墙钟)，且不超过硬上限（默认 3600 s）；prerelease 档重步骤另设上限 10800 s」；02_PIPELINE.md:54 同口径；
- 超限 step（全部跑在 linux-main，不享受 prerelease 10800 豁免）：CHK-CONTRACT-TEST/V6-CTEST-INTEGRATION = 7200（profiles ['linux-main']）、CHK-UNIT/V6-CTEST-UNIT = 7200（['linux-main']）、CHK-NWORKER/V6-DETERMINISM = 7200（['linux-main','prerelease']）；
- 超 3600 的顶层 14 项：CHK-UNIT 21420、CHK-REALDATA-E2E 20000、CHK-INVARIANT 18300、CHK-CONTRACT-TEST 9120、CHK-NWORKER 9060、LINUX-MAIN-BUILD-TREE 9000、CHK-ORACLE 8400、CHK-BUILD-LINUX 7260、CHK-SANITIZER 7260、CHK-ABI 5580、CHK-SYNTH-P2 5460、CHK-COVERAGE 5460、CHK-SYNTH-P3 4560、CHK-BUILD-WIN 3660；其中 CHK-UNIT / CHK-INVARIANT / CHK-REALDATA-E2E 三项 > 10800，而 CHK-UNIT 挂在 fast/linux-main/windows-main；
- 无强制：checks.schema.json 两处 timeout_seconds 均为 {"type":"integer","minimum":1}（:63-66、:204-207）；validate_registry.py:286-288 与 :342-344 只报 must be integer >= 1；grep -n 'timeout' validate_registry.py | grep -i 'max|3600|cap' ⇒ 零命中。
- 反方核验：① 文档字面只约束「step」，故 14 个顶层超限是否违规有解释空间（顶层是包一层 run_checks.py 的预算，需容纳子步求和）；但那 3 个 step 是字面违规、无解释空间；② 超限不是「抄错数」——CHK-UNIT 21420 s ≈ 5.95 h 与 fast 档定位本身矛盾；③ 不在 PASS 表（grep -n 'timeout|3600' 检查-修复验证.md 零命中）；④ S9-08 只对 03_GATES §6.2/6.3/6.4 的计数，未涉 timeout 上限 ⇒ 不重复。
**建议改法**：checks.schema.json 两处补 "maximum": 3600（prerelease 档用独立 step 级 override 字段），validate_registry R2 同步加上限；或改文档明确「顶层 = 子步求和，可超」并把 3 个 7200 s step 拆分/移到 prerelease。
**所属面**：③④

---

### 黄-04 ｜黄｜④
**文件:行**：eng/ci/exemptions.json:6（never_waivable）、:20（checker 字段）↔ eng/ci/validate_registry.py:512-543（R14 实现）↔ eng/ci/checks.json 的 waivable 字段
**问题描述**：豁免政策声明「never_waivable 含 P0 门，由 validate_registry --strict（R14）校验」，但 R14 实际只校验豁免登记表的外形（数组/高水位/approved_by），从不读 checks.json 的 waivable ⇒ 政策的点名检查器是个假锚；同时全表有 7 个 step 级 waivable:true，其中 3 个挂在文档标 P0 的检查项下。
**证据**：
- exemptions.json:20："checker": "python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict（R14）"；:6 "never_waivable": ["P0 门","SCI/ALG Oracle","ABI","生产路由",…]；
- R14 实体（validate_registry.py:512-543）逐行只有：文件存在、可解析、exemptions 是数组、high_water.max_entries 存在、条目数 ≤ 高水位、条目字段齐全、approved_by == "负责人" ⇒ 没有任何一处引用 waivable 或 never_waivable；
- 顶层 waivable:true 仅 1 项 CHK-REALDATA-E2E（已由 S23:170 与 工程控制/05_门禁/门禁现状与旧门禁处置.md:45 报过，本条不重复计）；
- step 级 7 项（S23 只数了顶层，此处为新面），按 01_CHECKS.md §3 的级别逐条对表：
  DEEP-CLANG-BUILD ← CHK-BUILD-LINUX = P0；TESTKIT-LIST ← CHK-UNIT = P0；ACTIONS-LOCK-ONLINE ← CHK-ACTIONS-LOCK = P0；DEEP-COMPLEXITY ← CHK-STATIC = P1；UT-CPU-AVX512 ← CHK-ISA-EQ = P1；DEEP-SAN-TSAN ← CHK-SANITIZER = P1；DEEP-COV-PY ← CHK-COVERAGE =（§2 表未列级）；
- 后果机理：eng/ci/run_checks.py:732-745 —— step.get("waivable") 为真且 exit 77 ⇒ V_SKIP_WAIVABLE；eng/ci/run.py:1273-1280 —— passed and skipped ⇒ verdict = "PASS" ⇒ 挂在 P0 项下的 step 以 77 退出时，P0 门整体仍判 PASS；grep -n 'exit(77)|return 77|SKIP_EXIT' eng/ci/check_realdata_e2e.py ⇒ :44 SKIP_EXIT = 77、:243-244 前置缺失即 return SKIP_EXIT（自述 slow/waivable）；
- 反方核验：① exemptions.json approvals: [] / exemptions: [] / high_water.max_entries: 0，note 明写「不得新增任何豁免…允许 CI 保持红」⇒ 当前没有任何已授豁免在遮红，故本条定黄不定红；② 01_CHECKS.md:253 的 P0 定义是「红灯阻塞合并，无 waiver」，:254 P1 才是「可负责人豁免、豁免须显式登记」⇒ 政策与注册表确实冲突；③ eng/ci/run_checks.py:742-745 对非 waivable 单元的 77 判 FAIL（fail-closed）⇒ 机制本身是健全的，坏在「P0 被预先标成 waivable」。
**建议改法**：R14 补一条「checks.json 中 waivable:true 的检查项/step，其所属检查在 01_CHECKS.md §2 的级别不得为 P0」，或读 never_waivable 做白名单校验；对 3 个 P0 项下的 waivable step 逐个改判（DEEP-CLANG-BUILD 缺 clang 应走 prerequisite_tools 显式 FAIL 而非 SKIP）。
**所属面**：④

---

### 黄-05 ｜黄｜④
**文件:行**：docs/ci/CI_SPEC.md:232-234（§9.3）↔ eng/ci/run.py:88/:665/:837/:910/:916 ↔ eng/ci/run_checks.py:11/:158
**问题描述**：规范里 FAIL(dirty)（写出可写面之外即红）这条硬判据只在 run.py 实现；而 01_CHECKS.md:268-272 把 python3 eng/ci/run_checks.py --all 指定为本地入口、CI_SPEC:233 也点名它——这条入口完全没有脏写检测。
**证据**：
- grep -n 'dirty' eng/ci/run_checks.py ⇒ 仅 1 处命中：:158 字段名元组 dirty_ignore_exact，无 detect_dirty/V_DIRTY；
- grep -n 'V_DIRTY|detect_dirty|strict_workspace|dirty_checked' eng/ci/run.py ⇒ :88 V_DIRTY、:665 detect_dirty、:837/:910/:916 strict_workspace、:930 dirty_checked、:1073 判定 ⇒ 脏写检测只在 run.py；
- 两个执行器同时在库并存：run_checks.py:11 docstring 自述「与 eng/ci/run.py 并存：工作流暂仍调用 run.py」；.github/workflows/ci-linux.yml:141 / ci-windows.yml:110 调 run.py ⇒ CI 面是覆盖的。
- 反方核验：① CI 链路不缺（workflow 调 run.py），故不是「CI 漏判脏写」；② CI_SPEC.md:233 的原句是「eng/ci/run_checks.py 另按同一字段把『真写跟踪树』的单元排入独占道」——它对 run_checks 的承诺是独占道而非 FAIL(dirty)，所以严格意义上缺的是「本地入口与 CI 入口判据不等价」这一层；③ run_checks.py:785-793 step_is_exclusive 确实消费 mutates_workspace ⇒ 独占道承诺本身是兑现的；④ 不在 PASS 表。
**建议改法**：要么给 run_checks.py 补 detect_dirty 同实现（两入口同一退出判据，符合 run_checks.py:743-745 自己写的「两入口对同一退出码的判定必须一致」），要么在 CI_SPEC §9.3 与 01_CHECKS §5 明写「FAIL(dirty) 仅由 run.py 强制，本地 run_checks.py 不判脏写」。
**所属面**：④

---

### 黄-06 ｜黄｜④
**文件:行**：eng/ci/ledgers/dead_config_keys.json（12 条）↔ eng/ci/check_config_consumed.py:86-102（evaluate）↔ docs/contracts/CONFIG_CONTRACT.md:85
**问题描述**：死键抑制台账 12 条全部处于不可能触发状态，且键名格式混用两代；检查器只有一条「命中即跳过」规则，没有任何陈旧台账规则（对比 registration anchor 台账）。
**证据**：逐条复算（template_keys() × production_blob()）：
- 当前死键 = 0（evaluate() ⇒ findings=[] , dead_keys=[]）⇒ 12 条永不被查询；
- 键格式两代混用：evaluate() 算出的 key 是 dead_config_key:<path_key>，而模板叶子路径已迁到 blocks[].<key> ⇒ 对仍存在于模板中的 3 个叶子，台账存的是旧式 dead_config_key:snr_path / :rotation_deg / :crpix_px，实际计算值是 dead_config_key:blocks[].snr_path 等 ⇒ 永远匹配不上（同台账里 dead_config_key:blocks[].sparse_snr_spacing_px 又是新式 ⇒ 同一份台账两种格式并存）；
- 另 9 条其叶子键已不在任何模板中（wcs.center_deg、wcs.s_out_deg、blocks[].sparse_snr_spacing_px 连 lib/** 都无读取点）⇒ 按台账自述用途「承载生产配置模板中生产代码零读取的键」已超出适用范围；
- 无陈旧规则：grep -n 'stale|ledger' eng/ci/check_config_consumed.py ⇒ 除 evaluate() 的 if key in ledger: continue（:98-99）外零规则；grep -n 'stale|policy' eng/ci/ledgers/dead_config_keys.json ⇒ 零字段；
- 文档关联：docs/contracts/CONFIG_CONTRACT.md:85 写 storage_form 的 no-op 形态「由该台账逐条登记」——台账确有该条（dead_config_key:storage_form），但其叶子 storage_form 已不在任何模板（grep -rn 'storage_form' eng/packaging/config/templates/*.json 零命中）。
- 反方核验：① 方向是安全的——将来真出现死键时，格式不匹配会让台账拦不住 → 判红（fail-red），不是假绿；② 本片实跑 python3 eng/ci/check_config_consumed.py ⇒ rc=0，0 findings，该门当前是绿的；③ 不在 PASS 表；S9-04 只把 dead_config_keys 当反方证据引用（storage_form 一节），未审其 12 条本身。
**建议改法**：按现行 path_key 重写 3 条旧式 id；对已移出模板的 9 条按各自 exit_condition 收口或删除；给 check_config_consumed.py 补一条 LEDGER_STALE：台账条目对应的 path_key 不在模板中（或已可被消费）⇒ 报点名，参照 registration_anchor_ledger 的做法。
**所属面**：④

---

### 黄-07 ｜黄｜②③
**文件:行**：eng/tools/check_gates_and_tolerances.py:12（G3 docstring）、:36（STAT_TOKENS）↔ docs/science/algorithms/GATES_AND_TOLERANCES.md:26（R4）
**问题描述**：检查器实际接受的「统计量令牌」比它自己声明的判据多 3 个；20 条门行中有 2 条正是靠这 3 个多出来的令牌才通过，即文档 R4 说不合格的行被放行了。
**证据**：
- STAT_TOKENS = [max, median, p95, rms, bitwise, 精确, 比例, 计数, 密度]（:36，9 个）；
- 文档 R4：docs/science/algorithms/GATES_AND_TOLERANCES.md:26 只列 max/median/p95/rms/bitwise/精确（6 个）；工具自己的 G3 docstring（:12）复述的也是这 6 个；
- 用工具自身的 split_row()（:49+）复算 20 行：G-P1-STAR-RECALL（:58，统计列仅「比例（逐场）」）、G-P1-STAR-FP（:59，仅「计数密度（每千像素）」）——这 2 行若按文档 R4 的 6 令牌集合会判不合格；
- 实跑 python3 eng/tools/check_gates_and_tolerances.py ⇒ rc=0，20 gates / 22 pass / 0 fail（门是绿的）。
- 反方核验：① 这不是科学数值争议——数值面不在本片重开，D 系终裁不动；问题是判据文本与判据代码不一致（②内部不一致 + ③文档与代码冲突）；②「比例/计数密度」作为统计量语义上合理（逐场比例、每千像素计数），很可能是文档 R4 漏列而非工具越权——两种修法都成立，故只登记不擅改。
**建议改法**：二选一——GATES_AND_TOLERANCES.md:26 R4 补齐 比例/计数/密度，或 STAT_TOKENS 收敛为 6 个并把 2 行门表的统计列改成合规写法；G3 docstring 与两处保持同源。
**所属面**：②③

---

### 黄-08 ｜黄｜④
**文件:行**：docs/science/{ASTROMETRY,CALIBRATION,DRIZZLE,INTEGRATION,NOISE_MODEL,PHASE2_UPM,PHASE3_HIPS_TO_FITS,PHOTOMETRY,PSF,REJECTION}.md（各文件 :293/:506/:249/:183/:411/:386/:216/:337/:208/:346）↔ eng/tests/sciencelint/test_sciencelint.py ↔ eng/ci/checks.json
**问题描述**：10 份科学文档在文件里写死「science_contract_lint PASS」，10/10 实测仍 PASS（结论为真）；但 CI 里承载这个声称的只有 CHK-UNIT 下的 UT-SCIENCELINT 一步，而该 unittest 只覆盖 2 份文档 ⇒ 另外 8 份的 PASS 声称无任何 CI 保护，且工具本身没有 --self-test。
**证据**：
- 逐份实跑 python3 -B eng/tools/science_contract_lint.py <doc> ⇒ 10 次全部 SCIENCE_CONTRACT_LINT_PASS kind=sci files=1 sections=15、rc=0 ⇒ 声称成立；
- 唯一 CI 载体：checks.json 中 grep -n 'science_contract_lint' ⇒ 零命中；唯一相关步骤 UT-SCIENCELINT（under CHK-UNIT）= python3 -B -m unittest discover -s eng/tests/sciencelint …；该测试文件的实文档用例只有 test_01 → docs/science/CALIBRATION.md 与 test_06 → docs/science/algorithms/CALIBRATION_ALGORITHMS.md（其余 3 例是 mutation 注入）⇒ 8/10 声称文档不在测试面；
- grep -n 'self-test|self_test' eng/tools/science_contract_lint.py ⇒ 零命中 ⇒ 工具无 --self-test；
- grep -rn 'science_contract_lint' docs/ci/ ENGINEERING_SPEC.md ⇒ 零命中 ⇒ 也没有文档条目登记它的负例面义务。
- 反方核验：① 声称本身当前为真（10/10 实跑 PASS），不是幻觉陈述；② 属「文档说有、代码没接」的保护面缺口而非断言失真，故黄；③ 与红-06 同源但证据独立，不合并计数。
**建议改法**：把 UT-SCIENCELINT 扩到 10 份声称文档各一例（或给 science_contract_lint.py 加 --self-test 并注册独立 step），让「文件里写 PASS」这件事本身被门盯住。
**所属面**：④

---

### 黄-09 ｜黄｜③④
**文件:行**：eng/ci/known_failures.json（v2，failures[0].evidence[0..3]、contract.source、excluded_by_policy）↔ eng/tests/unit/CMakeLists.txt:1320-1336 ↔ eng/tools/HANDOVER.md:62/:109
**问题描述**：唯一一条已知失败条目的全部证据锚都过期——行号漂移约 240 行、引用的 HANDOVER.md 根文件已被删除、引用的 07_CI_MACHINE_CONTRACT.md / 02_GATES_AND_EXECUTION.md 已在治理改版中删除、引用的日志文件不存在。
**证据**：
- 行漂移：条目 reason 写「eng/tests/unit/CMakeLists.txt:1081」、evidence[0] 写 :1077-1096；实测该块已迁到 :1320（注释 # F-CI-002-01(CI-002 owner 裁决): 本块依赖 p1noise_under_test…）、:1323（if(TARGET p1noise_under_test AND TARGET astrocs_p1_noise) 守卫）、:1334（add_test）⇒ 漂移 ≈ 240 行；
- HANDOVER.md 已删：git log --diff-filter=D -- HANDOVER.md ⇒ 6fa8f5cf 删除；ls HANDOVER.md ⇒ No such file；evidence[1] 写 HANDOVER.md:62,109（F-AIO-001=p1_noise_adapter 全量唯一 FAIL…）；现存 eng/tools/HANDOVER.md:62 = 「不宣布发布…」、:109 = 「F:\Astro dev\… 状态件」⇒ 两处都不提 F-AIO-001；全仓 grep -rn 'F-AIO-001' ⇒ 唯一命中 lib/phase1_session/memory.md:118；
- 合同文档已删：contract.source = 07_CI_MACHINE_CONTRACT.md §…、excluded_by_policy = 02_GATES_AND_EXECUTION.md …，两文件均在 a861d8f6 "docs: 以新设计文档集替换旧治理体系" 删除；同类悬空引用还散布在 eng/ci/run.py:5、eng/tools/quality/known_failures_baseline.py:11/:69、ci_coverage_runner.py:5、check_complexity.py:5、run_monitored.py:932、eng/ci/tests/test_ci001_failclosed.py:4、eng/tests/quality/test_known_failures_baseline_ci.py:7 共 7 处 eng/*.py；
- 日志不存在：evidence[3] run/ci/ci-baseline-001/ctest_full_pre.log ⇒ ls No such file。
- 反方核验（为什么是黄不是红）：① 条目实质仍成立——p1noise_under_test 在受控源树中只存在于 run/P1-CONCURRENCY-CALIB-01/wsrc/…/tests/p1noise/CMakeLists.txt:68（非在库产物），主树 eng/tests/unit/CMakeLists.txt:1323 的 if(TARGET …) 守卫因此关闭、该 CTest 目标零注册 ⇒ 「门卫关闭 ⇒ 测试未执行」的条件描述为真；② expiry = 2026-12-31 未到期；③ 已由 S23:169（K-03）以「1 条已知失败」形式对表过条目数，但未审这些证据锚，故不重复计数；④ 不在 PASS 表。
**建议改法**：把 evidence 四条按现址重锚（:1320-1336、lib/phase1_session/memory.md:118），删掉不存在的日志引用或补登；contract.source / excluded_by_policy 改指现存的 docs/ci/01_CHECKS.md §2.1 与 03_GATES；顺带清理 7 处 eng/*.py 对已删文档的引用。
**所属面**：③④

---

### 黄-10 ｜黄｜④
**文件:行**：eng/ci/check_registration_anchors.py:58（PATH_FILE_RE）、:103-115（_scan_class）
**问题描述**：锚门的取值分类器要求整个值严格匹配纯路径形态，因此 eng/ci/known_failures.json 里带行号/带注释的证据串全部返回 None ⇒ 从不被检查——这正是黄-09 那批死锚能安然通过 P0 锚门的原因。
**证据**：用该模块自身的 _iter_strings + _scan_class + _is_historical 对 known_failures.json 逐值复算，key ∈ LIVE_KEYS 的 8 个值中 7 个 cls=None（不检查）、仅 removals[0].evidence[0]（run/release-rescue/fd-utcli/REPORT.md，纯路径）判为 runtime 被 R2 捕获：
- $.contract.source → cls=None（07_CI_MACHINE_CONTRACT.md §…）
- $.failures[0].evidence[0] → cls=None（eng/tests/unit/CMakeLists.txt:1077-1096（…））
- $.failures[0].evidence[1] → cls=None（HANDOVER.md:62,109（…））
- $.failures[0].evidence[2] → cls=None（lib/phase1_session/memory.md:118（…））
- $.failures[0].evidence[3] → cls=None（run/ci/ci-baseline-001/ctest_full_pre.log（…））
- $.removals[0].evidence[0] → cls=runtime（纯路径 ⇒ 被查）
- $.removals[0].evidence[1] → cls=None（run/…/utcli.log（前台独立…））
机理：PATH_FILE_RE = ^(?:[A-Za-z0-9_.\-]+/)+[A-Za-z0-9_.\-]+$ 要求整串只由路径字符构成——file:1077-1096 含 : ⇒ 不匹配；HANDOVER.md:62,109 含 :/, 且无目录分隔 ⇒ 不匹配；…/utcli.log（前台独立…）含全角括号 ⇒ 不匹配；于是 _scan_class 走 return None 分支，既不做 R1 存在性检查也不做 R2 run/ 检查。
- 反方核验：① EXCLUDED_CI_FILES 不含 known_failures.json ⇒ 不是被排除，是被形态过滤漏掉；② 同文件 run/release-rescue/fd-utcli/REPORT.md（纯路径）确实被查到并进台账 ⇒ 机制有效，只是覆盖面窄；③ 真实后果可量化——本应被判红的 run/ci/ci-baseline-001/ctest_full_pre.log（不存在）与 HANDOVER.md（不存在）在 31 条原始 findings 中一条都没出现。
**建议改法**：_scan_class 改为先截断 file:line / file（注）后缀再匹配路径（:62 可加 PATH_LINE_RE），并对截断后落在 run/ 的值继续走 R2；或给 known_failures.json 的 evidence 增一条专用判据（每个 evidence 必须是「存在路径 + 可定位行」）。
**所属面**：④

---

### 黄-11 ｜黄｜④
**文件:行**：eng/ci/known_failures_baseline.json:3
**问题描述**：入库登记 JSON 里写死了服务器绝对路径，直接违反 AGENTS.md §3「在仓库内工作，不写死服务器绝对路径」。
**证据**：known_failures_baseline.json:3 ⇒ "repo": "/workspace/Astro CS Database"；AGENTS.md §3 末条逐字「在仓库内工作，不写死服务器绝对路径」。
**反方核验**：① 该字段的既有正确写法在同族工具里——eng/tools/quality/known_failures_baseline.py:53 用 REPO = pathlib.Path(__file__).resolve().parents[3] 运行期推导，证明仓内已有不写死的范式；② 该字段被 KNOWN-FAILURES-BASELINE 步骤列进自身 outputs，run.py 的 ignore_exact = set(check["outputs"]) 会自我豁免 ⇒ 不会因此判脏写，但换机/换路径即失效；③ 不在 PASS 表。
**建议改法**：改存仓库相对根（如 "."）或删该字段、由运行期 parents[3] 推导；同批 grep 其他入库 JSON 是否有同类绝对路径。
**所属面**：④

---

### 黄-12 ｜黄｜③④
**文件:行**：docs/ci/01_CHECKS.md:180-219（§2.1.1 台账）↔ eng/tools/check_*.py 现状
**问题描述**：§2.1.1「在库 check_*.py 三分类」台账的四个计数全部与现状不符，且有 2 个检查器三类都没归；台账自称的「应注册（7）」现已全部注册却未回写。
**证据**（按台账自己的「同名精确路径」规则在 dfe44ac2 复算）：
- 在册：文档 88 / 实测 110；
- 未注册：文档 24 / 实测 18；其中 eng/tests/** 文档 4 / 实测 4（相符）；
- 余（应注册 7／应退役 10／应删除 3）：文档 20 / 实测 14（全部 eng/tools/**）；
- 14 个未登记的 eng/tools/**：check_final_traceability、check_p1_symbol_map、check_p2_symbol_map、check_release_consistency、check_release_layout、check_reproducible_build、check_traceability、check_traceability_matrix、quality/check_comment_hygiene、quality/check_commits_csv、quality/check_conclusion_anchors、quality/check_p3_rejection_count、quality/check_source_inventory、quality/check_task_result_schema；
- 「应注册（7）」已全数落地：台账列的 7 项在 checks.json 中 7/7 命中 ⇒ 文档未回写；「应退役 10／应删除 3」的探针 rc=2 与台账一致（check_source_index_v61.py 已实际删除）；
- 三类外的 2 个：check_conclusion_anchors.py 与 check_p3_rejection_count.py 既不在「应注册」也不在「应退役/删除」，但各有 UT 载体（eng/tests/quality/test_conclusion_anchors.py、test_p3_rejection_count.py）⇒ 属第四类「有测试载体未注册」，台账的三分类表达不了。
- 反方核验：① 14 个未登记项不都是缺陷——其中 7 个已是注册检查器（回归已闭合），4 个有 UT 载体，纯「无载体且未注册」的缺口远小于 18；② 计数差主因是并发轮次持续新增 eng/tools/** 检查器，台账是快照；③ CHK-REGISTRY-DOC-SYNC 不覆盖这张表（parse_doc 只切 §2/§2.1/§2.3，:180-219 属 §2.1.1）⇒ 漂移无人判红。
**建议改法**：由脚本重生成 §2.1.1 四个计数并补「第四类：有 UT 载体未注册」；给同步门加「§2.1.1 的三分类必须覆盖全量 eng/tools + eng/tests 下 check_*.py」的判据。
**所属面**：③④

---

### 黄-13 ｜黄｜③
**文件:行**：docs/ci/01_CHECKS.md §2 表「命令/入口」列 ↔ eng/ci/checks.json 的 command
**问题描述**：7 处命令与注册表实跑命令实质不一致（另 8 处仅是注册表多包一层 run_checks.py --check，属包装差异）；按 §1「注册表双向一致」的要求，文档给的入口跑不出注册表跑的东西。
**证据**（逐行用 | 切分对表；已剔除 S9-09 报过的 2 个损坏单元格 AHPX-WEIGHT-RETIRED 的 writer_accept_legacy_meta\ 与 CHK-L4-SEAM-FOOTPRINT 的 rel_step）：
1. DOC-INDEX：文档 …check_doc_index.py --strict vs 注册 …--strict --json-out run/ci/doc-index/doc_index.json ⇒ 少 --json-out，文档入口不产证据文件；
2. ALG-LINE-ANCHORS：文档无 --json-out vs 注册带 ⇒ 同上；
3. VERSION-CONSISTENCY：命令后接散文「（缺省 expected = 根 VERSION，不写死字面量）」⇒ 单元格不是可执行命令；
4. RESOURCE-GATE-REAL：以 … 截断 vs 注册 --output run/ci/resource-gate-real/resource_gate_real.json --gate-required --gate-compute-interval 20 ⇒ 读者无法复现；
5. CHK-MASTER-UNIT-GUARD：缺 --work run/master_unit_guard/guard ⇒ check_master_unit_guard.py:305 --work 缺省 None、:317 缺省落到 repo/"run/master_unit_guard" ⇒ 文档入口写到不同目录；
6. CHK-MASTER-UNIT-GUARD-SELFTEST：缺 --work run/master_unit_guard/selftest ⇒ 同上；
7. （8 项包装型）API-DOCS、CHK-ACTIONS-LOCK、CHK-WORKFLOW-GOVERNANCE、CHK-NAMING-SURFACE、CHK-PRODUCT-CONTRACT、CHK-CITE-CLAIM-MARK-CONSISTENCY、CHK-DOC-UNVERIFIED-CITE 等：文档给底层原始脚本 vs 注册 run_checks.py --check <ID> --quiet ⇒ 实际执行同一脚本（判非缺陷，不计入 7）。
- 反方核验：① 包装型 8 项执行语义等价（run_checks.py --check X 会跑该 ID 的 step，其 step 即文档所写脚本），故不计入问题；② 无一处 ID 缺失——§2 表 136 个可解析 ID 全部能在 checks.json 找到、0 个未知 ID（反向差集 8 项 = S9-02 已报）；③ 「少 --json-out」是否算错取决于「命令/入口」列的定位（是「怎么跑」还是「跑出什么」），故定黄不定红。
**建议改法**：§2 表命令列改为从 checks.json 机械生成（或至少标注「证据参数以注册表为准」）；CHK-MASTER-UNIT-GUARD 两行补 --work；RESOURCE-GATE-REAL 去掉 … 写全。
**所属面**：③

---

### 黄-14 ｜黄｜③④
**文件:行**：eng/tools/quality/authority_surfaces.json:58-61 ↔ docs/ci/CI_SPEC.md ↔ eng/ci/ctest_skip_register.json
**问题描述**：权威面登记 JSON 把 ctest_skip_register 的权威写成「docs/ci/CI_SPEC.md（skip 必须显式登记）」，但 CI_SPEC 里根本没有这条规则；消费它的检查器只校验载体文件存在，从不校验权威串 ⇒ 这是一条幻觉权威锚。
**证据**：
- authority_surfaces.json:58-61："id": "ctest_skip_register"、"authority": "docs/ci/CI_SPEC.md（skip 必须显式登记）"；
- grep -n '必须显式登记|skip.*登记' docs/ci/CI_SPEC.md ⇒ 零命中；该文只在 :100「不是靠放宽判据、也不是靠 skip」、:105「不以 skip/xfail/环境变量绕过」两处提 skip，无任何「skip 必须登记」的条款；grep -rn '必须显式登记' docs/ ⇒ 零命中 ⇒ 该规则在全 docs 不存在；
- 消费者 eng/tools/quality/check_conclusion_truth.py:49 读 authority_surfaces.json，:227 只报「权威载体缺失」（canonical 路径存在性）⇒ authority 串与所引文档从未比对；
- 关联事实：eng/ci/ctest_skip_register.json（12 条 skip、4 类）自述「当前尚无 checker，列为 open item」；grep -rn 'ctest_skip_register' eng/ci/*.py ⇒ 零命中（该缺口已在 eng/ci/ledgers/design_clauses.json 的 DESIGN-12.4-THREE-ERROR-SEMANTICS 记为 status: "unwired"，本条只交叉引用不重复计）；baseline_evidence = run/RELEASE-02/DESIGN-CONFORMANCE/evidence/LastTestsDisabled.log 指向 run/ 临时路径；grep -rn 'ctest_skip_register' docs/ ⇒ 零命中。
- 反方核验：① 「skip 不得绕过判据」这条精神在 CI_SPEC 有（:100/:105），缺的是「必须显式登记」这条可操作条款 ⇒ 是措辞幻觉不是原则冲突；② eng/ci/README.md 提到该文件 ⇒ 文件本身有人知晓；③ 不在 PASS 表。
**建议改法**：要么在 CI_SPEC.md §9 落一条「skip 必须显式登记进 eng/ci/ctest_skip_register.json」并说明其门，要么把 authority 改指真实出处（eng/ci/README.md 或 docs/validation/v6/ORACLE_AND_ZERO_CASE_POLICY.md 的 skip-only 规则）；check_conclusion_truth 补「权威串中的 文件名 §锚 必须在该文件中出现」判据。
**所属面**：③④

---

## 三、已查无问题面（绿 16）

对下列面做了实际复算/实跑，未发现问题，据此声明覆盖：

| # | 面 | 复算/实跑结果 |
|---|---|---|
| 绿-01 | 豁免台账是否遮红 | eng/ci/exemptions.json ⇒ approvals: []、exemptions: []、high_water.max_entries: 0，note「不得新增任何豁免…允许 CI 保持红」⇒ 0 条豁免，无红灯被盖 |
| 绿-02 | 注册表结构门 | python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict ⇒ rc=0 / error_count:0 / verdict:PASS；150 ID、0 重复、schema_version=1 |
| 绿-03 | 注册表 schema 门 | run.py:326-332 load_registry() 加载 eng/ci/checks.schema.json（draft 2020-12，5 处 additionalProperties:false）并做结构校验，不合规即 RunnerError rc=2 |
| 绿-04 | 反向一致（文档承诺但无实现） | python3 eng/ci/check_registry_doc_sync.py ⇒ 输出只有 registered_not_documented（8 项 = S9-02），documented_not_registered 空；§2 表 136 个可解析 ID 与注册表全命中、0 未知 |
| 绿-05 | 资源门合同 ↔ 文档 | eng/contracts/resource_gate_v1.json::compute/memory 的 6 条判据与 docs/plugins/infrastructure/21_observability.md §8（①–⑥）、docs/ci/CI_SPEC.md:206 §9.2（四条 L2）、docs/ci/03_GATES.md:57 逐数值对齐：mean 85% / p50 90% / per-sample 85%+0.70 / queue 60%+10s 与合同字段逐一相符；enforcement 分类（enforce vs record_and_justify）两侧一致 |
| 绿-06 | 生产接线棘轮 | python3 eng/ci/check_prod_wiring.py ⇒ CHK-PROD-WIRING_PASS(ratchet) new=0 frozen=28 resolved_since_baseline=181，rc=0 |
| 绿-07 | 冻结门负例面 | python3 eng/ci/check_frozen_gate.py --self-test ⇒ FROZEN_GATE_SELFTEST cases=14 passed=14 failed=0；--evidence artifacts/acceptance/…/real16_w16_gate.json ⇒ 正确判 red + 4 violations（能红） |
| 绿-08 | 容差门判据 | python3 eng/tools/check_gates_and_tolerances.py ⇒ rc=0，20 gates / 22 pass / 0 fail（数值面无问题；令牌集差异另记黄-07） |
| 绿-09 | 科学合同 lint | science_contract_lint 对 10 份声称文档逐份实跑全部 PASS（SCIENCE_CONTRACT_LINT_PASS kind=sci files=1 sections=15）⇒ 声称为真（保护面缺口另记黄-08） |
| 绿-10 | 配置消费门 | python3 eng/ci/check_config_consumed.py ⇒ rc=0，37 个模板叶子键、0 findings、0 死键 |
| 绿-11 | 变异门登记 | python3 eng/ci/check_mutation_gates.py ⇒ OK；10 条门 + --self-test PASS（open_items[1] 的过期句属 S23 已报，交叉引用） |
| 绿-12 | 条款登记计数与 file 锚 | v6_clause_registry_v1.json 96 条分档计数自洽；53 条 source_binding.file 缺失 0（test_field_constraints_integration.py:165-174 实测通过）（locator/contains 缺口另记红-05） |
| 绿-13 | ctest 基线接线 | eng/ci/ctest_baseline.json（161 targets）消费点 = check_ctest_registration.py C5（:19/:274/:281），注册为 step CTEST-REGISTRATION ⇒ 在册且被跑（其 ID 的文档写法问题另记黄-01）；retired_code_allowlist.json 13 entries 被 CHK-RETIRED-CODE（01_CHECKS.md:69）消费 |
| 绿-14 | SKIP ≠ PASS 合同 | run_checks.py:732-745：非 waivable 单元 exit 77 ⇒ V_FAIL（注释明写「任何检查器都能 sys.exit(77) 让整门变绿 ⇒ 按 FAIL 记」）；run.py:1075-1090 同口径；run.py:1246-1249「选中检查全部合同化 SKIP ⇒ FAIL（不允许空集 PASS）」；verify_actions_lock.py:348-350 记 SKIP(77) ≠ PASS ⇒ fail-closed 机制健全（P0 被标 waivable 的问题另记黄-04） |
| 绿-15 | 极性台账无多登 | eng/ci/polarity_evidence.json::extra_records = [] ⇒ 无「注册表里没有的记录」（覆盖不足另记红-07） |
| 绿-16 | 退役/合同探针能红 | 退役 ID 注册即 rc=2 的探针、check_contract_graph / check_symbol_dimension 合同面在本片复算均按文档给定方式判红/判过，无「恒真门」迹象 |

**四面覆盖声明**：
- ① 科学性：本片工具面触及科学判据的两处（check_gates_and_tolerances 的统计令牌、science_contract_lint 的 10 份声明）均已实跑核对；未发现任何公式、默认容差、冻结定义层面的新问题——涉及口径者一律以分歧台账 D-01…D-11 终裁与 PASS 表为准，本片不重开。
- ② 行文逻辑：01_CHECKS §1/§2/§2.1.1/§3 逐节读过并复算（台账四计数、命令列、分级定义），问题记于黄-12/黄-13；结构损坏类（16 行非 5 单元格、表头尾游离 |）属 S9-09，交叉引用。
- ③ 跨文档冲突：checks.json ↔ docs/ci/* 全量 ID 对表（双向差集）＋ eng/contracts/* ↔ docs/contracts、docs/validation/v6 抽查，问题记于红-02/03/05 与黄-01/02/03/07/09/12/13/14。
- ④ 幻觉与锚：7 条红全部落此面；另做了「锚门自身盲区」的反向核对（黄-10）与「声明的 checker 是否真存在」的反向核对（黄-01/黄-14）。

---

## 四、抽样方法

1. 全量机器门实跑（只读命令，输出重定向至 /tmp，不写仓内）：validate_registry --strict、check_registry_doc_sync、check_registration_anchors、check_doc_index --strict、check_doc_index --self-test、check_agents_gov、check_agents_gov --self-test、check_gates_and_tolerances、science_contract_lint ×10、check_config_consumed、check_prod_wiring、check_frozen_gate --self-test、check_mutation_gates、polarity_probe --check。纪律：未执行任何带 --json-out / --write-baseline / --output 指向仓内（尤其 reports/**）的命令；未执行任何构建/ctest；未产生除本报告外的仓内写盘。
2. 静态复算（Python 一次性脚本，base64 管道注入，仅读）：checks.json 150 项的负例入口分层、timeout_seconds 上限扫描、waivable × 文档级别对表、§2 表 136 ID 解析、§2.1.1 三分类重算、dead_config_keys 12 条逐条状态、v6_clause_registry 53 条 source_binding 逐条锚验、reports/v6/** 全量 token 扫描、抬头形态新旧规则 271 份普查、known_failures.json 逐值锚分类复算。
3. git 只读取证：git show <commit>:<file>、git log --diff-filter=A/D、git status --porcelain、git rev-parse。未执行任何 git add/commit/checkout/stash/restore/update-index。
4. 反方核验纪律：每条红黄均做 ①是否已被 PASS 表登记 ②是否已被兄弟切片登记 ③机器门为何看不见 ④是否有反证可将其降级 四项检查；未通过证据门槛者不入册（未凑数）。
5. 抽样域：任务点名的 5 个工具全部逐个实开（check_gates_and_tolerances / science_contract_lint / check_config_consumed / check_prod_wiring / check_frozen_gate），另按域覆盖 eng/ci/ 注册表与执行器、eng/contracts/ 合同 JSON、docs/ci 与 docs/contracts、docs/validation/v6。

---

## 五、与 PASS 表／兄弟切片的去重登记（不计入本片计数）

| 来源 | 条目 | 本片处理 |
|---|---|---|
| PASS 表 | 红-4 / 红-8 / 黄-2 / 黄-5 / 黄-12 / 黄-13 等（k_corr、k_shape、行漂移 15+6+3 项） | 全部跳过，不重开 |
| PASS 表 | 黄-14（UPM_SOLVER.md:26 stale） | 跳过 |
| S9-01 | docs_fully_covered + subordinate_docs_registered（CCD 文档一份） | 不重复计；红-03 只接续 38/38 现值与两个新面＋第二份未登记文档 |
| S9-02 | 8 个注册未登记 ID ＋ :239「注册项待写入」 | 跳过；黄-02 是不同行不同 ID（:230） |
| S9-03 | CI_SPEC.md:213-214 模型分隔符泄漏 | 跳过（本片扫到同处，交叉引用） |
| S9-05 | parse_doc 只切 §2/§2.1/§2.3、R3/R4 面错位 | 跳过；黄-01/黄-12 只引用其「同步门不覆盖 §2.1.1」这一事实 |
| S9-08 | 03_GATES §6.2/6.3/6.4 计数漂移 | 跳过 |
| S9-09 | §2 表 16 行非 5 单元格、表头尾游离 | 跳过；黄-13 已剔除其点名的 2 个损坏单元格 |
| S23 K-03 / :170 | CHK-REALDATA-E2E 顶层 waivable:true 与 P0 冲突 | 跳过该 1 项；黄-04 只报新面：never_waivable 声明的 checker（R14）并未实现该政策 ＋ 7 个 step 级 waivable（3 个挂 P0 项下） |
| S23 :169 | mutation_gates.open_items[1] 过期句 | 跳过 |
| S23 :172 | 「未发现以豁免/waiver 掩盖红灯的结构性问题」 | 本片复核成立（绿-01），黄-04 补的是政策锚假接线，不推翻其结论 |
| 独立审计/证据/复核-合同层.md:549 | W3-c P1 module_dll_contract.schema.json 无效 JSON | 不在 PASS 表 ⇒ 红-01 正式登记并交叉引用 |
| 工程控制/05_门禁/门禁现状与旧门禁处置.md:45 | 「文档标 P0 而注册表 waivable: true」 | 交叉引用（S23 同源） |
