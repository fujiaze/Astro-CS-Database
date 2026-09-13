# M6b 合并底稿 —— 追溯链与机器检查有效性 / 文档体系与交叉引用

- 合并代理：**M6b**（层2，只读复算）
- 输入切片：`问题扫描/_cache/L16.md`（19 条：P0 5 / P1 10 / P2 4）、`问题扫描/_cache/L18.md`（26 条：P0 2 / P1 19 / P2 5）、前台基准 F00-01 / F00-02 / F00-04 / F00-05（均在本域）→ **共 49 项处置（45 条叶子 + 4 条前台基准）**
- 定稿文件（一类×一级一个文件，文件内分条）：
  - `findings/E_TRACE_BREAK/p0/M6b_L16_L18.md`（1 条）
  - `findings/E_TRACE_BREAK/p1/M6b_L16_L18.md`（3 条）
  - `findings/E_TRACE_BREAK/p2/M6b_L16_L18.md`（3 条）
  - `findings/G_GOV_GATE/p0/M6b_L16_L18.md`（3 条）
  - `findings/G_GOV_GATE/p1/M6b_L16_L18.md`（3 条）
  - `findings/G_GOV_GATE/p2/M6b_L16_L18.md`（2 条）
  - `findings/I_DOC_HYGIENE/p1/M6b_L16_L18.md`（2 条）
  - `findings/I_DOC_HYGIENE/p2/M6b_L16_L18.md`（1 条）
  - `findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.md`（3 条）
  - `findings/F_TEST_GAP/p1/M6b_L16_L18.md`（1 条）
- **定稿合计 22 条：P0 4 / P1 12 / P2 6**
- 其余处置：**剔除 2 条**（L18-006 全条、L18-009 的「docs/review 未登记」子项）、**降级或收窄 6 条**（L16-011、L16-013、L16-014、L18-002、L18-011、L18-019）、**合并为单条根因 4 条**（L16-003/010/016/017 → G-004）、**移交 8 条**（L16-006、L18-010、L18-011 实现面、L18-013、L18-017、L18-023、L18-025、L18-026 记忆面）、**无法判定 3 项**（见 §6）

---

## 0 依据与口径

- 已读：`问题扫描/10_PROTOCOL.md`、`问题扫描/_merge/00_COORDINATION.md`（含前台对 F00-01 根因的改判、L18 主题映射、L16-006/L16-005 会签要求）、`_cache/L16.md`、`_cache/L18.md` 全文；宪章 §1.1/§1.2/§4.1/§6.3/§7.3/§12.1/§12.2/§12.3（十二项逐条）/§13.1/§13.2/§14.2/§16.1-16.3/§17.2/§17.10-17.12/§18.1；矩阵 JSON+CSV+LAYERS.csv+schema、SPEC、两个 check_traceability、矩阵检查器、锚门脚本+合同+契约文档、DOCUMENT_INDEX、旧表、四份 RELEASE_STATUS、ARCHITECTURE（根 + architecture）、TROUBLESHOOTING（根 + diagnostics）、registry 抽样、ci/checks.json 全文 id 枚举、ci/impact_map.json、ci/known_failures.json、ci/run.py 判据段、tests/traceability + tests/quality/test_doc_line_anchors.py + tests/arch/test_single_cli.py。
- 口径：**全部计数为本代理按当前树复算**，不沿用转述数；免报区（`run/**`、`reports/**`、`evidence/**`、`artifacts/**`、`工程控制/**`、`build/**`）只作指针存在性旁证，不作问题对象（但"真源引用这些路径"按真源问题登记）。
- 复算方法注记（重要）：`read` 工具对 **超过 2000 字符的单行会截断**，直接 `JSON.parse` 矩阵 JSON 会失败并**少数行**。首轮解析得 26 行（正是 L18-006 的错误来源），改用 250 行分块 + 行式键值解析 + `grep -c '^   "module_id":'` 独立对账后为 **30 行**。故本域所有叶子级"矩阵行数/模块数"结论都必须以行数对账复核。

---

### 0.1 截断伪影自审与结构化比对口径（据前台紧急警示执行）

前台警示后，本代理对**全部涉及两份结构化文件同构/等值判定的定稿条**做二次复核：判据一律改为「①元素计数 ②列名/键集合 ③整行 grep 定位」三件套，read 行文本只用于取短行原文作引用，不再作差异判据。自审结果：

| 比对/计数对象 | 受 >2000 字符单行截断影响 | 二次复核口径与结论 |
|---|---|---|
| docs/traceability/TRACEABILITY_MATRIX.csv（31 行） | 受影响：第 7/8/9/10/16/17/20/26/28/29 行被截断 | 三件套：grep 行首 MOD- = 30 数据行；表头 24 列；逐行 CSV 解析后每行仍得 24 字段，截断只落在末列 notes 内部（异常行 0）⇒ 前 23 列完整，"23 非 notes 列 0 差异"非截断伪影，**L18-006 剔除维持**，但判据表述已按本口径重写 |
| docs/traceability/TRACEABILITY_MATRIX.json（789 行） | 受影响：第 240/396/448/630/682/708 行截断，经逐行核对全部是 notes 行 | 状态/ID/路径字段各占短行 ⇒ 未受影响；module_id 整行 grep = 30（与解析行数一致）；层取值整行 grep：test_status VERIFIED=22、test_path 以 docs/ 开头=18、test_id 含 DESIGN=**11**、src_path 锚 .h::=11、evidence_status VERIFIED=3 / MISSING=27、science_doc 指向 docs/algorithms/=1 ⇒ 与定稿一致 |
| docs/TRACEABILITY.csv（旧表 68 行） | 不受影响：0 行截断（最长行 < 2000 字符） | 67 数据行；requirement_type 67/67 = science；status 67/67 = VERIFIED；requirement_id 前缀 SCI 36 / TEST 17 / DATA 5 / ALG 4 / ENG 4 / ACR 1，**API- 前缀 0 行**；API_STANDARD:15-16 举例的三个 API ID 在旧表命中 0/0/0（全仓命中 8/6/42，全在别处）⇒ E-001 路由层结论与"0 API-* 行"为截断无关的硬事实 |
| 两表可交比 ID 与 TEST 证据（E-001） | 旧表侧不受影响；矩阵侧只用短行 | 8 个可交比 ID、TEST 证据一致 0/8；判据取旧表 test_ids/test_files 短行 + 矩阵 test_id/test_path 短行，不经 notes ⇒ 复核后不变 |
| 锚规模 805 / 裸 :NN 223 / 悬空权威 138 处（E-002/E-004） | 不受影响（判据为整行 grep 命中数，非两文件等值比较） | 复核后不变；各条已注明"grep 命中数"口径 |
| 16 行 notes 自述未接线、其中 14 行七层 VERIFIED（G-001 实例 C） | notes 属截断高危列 | 改为双判据且不依赖 notes 全文：整行 grep「未建」命中 16 个 notes 行（含被截断的 6 行；rg 对整行原文匹配，不受显示截断影响）+ 七层 VERIFIED 由状态短行判定；交集 = **14 行**（另 2 行为 upm-apply/upm-fit，其 SRC/TEST 本为 MISSING），与"全 VERIFIED 19 行"交叉核对一致 ⇒ 数值维持、判据重写 |

**唯一因截断而更正的数**：*-DESIGN-001 设计态 TEST ID 由 9 改为 **11**（G-001 证据段与本文 §2 已同步更正）。除此之外本域定稿不存在"以按行读到的差异为判据"的情形。

---

## 1 逐条处置表（四态）

### 1.1 前台基准（4 项）

| 项 | 复算结论 | 四态 | 定稿去处 |
|---|---|---|---|
| F00-01（追溯双头/旧表 fail-open） | 权威归属本身清楚（SPEC:15 JSON 权威）；失效在**路由层**（README-DOCS:13「唯一矩阵」、DEVELOPER_GUIDE:38、API_STANDARD:14-16 指向实测 0 条 `API-*` 行的旧表、9 份 SCI 尾注、DOCUMENT_INDEX:68-69 与 :75-83 双 ACTIVE_NORMATIVE、CI changed_paths 以旧表为追溯源）；`tools/quality/check_traceability.py:159` 无条件 `return 0` 复现 | 仍成立 | **M6b-E-001（P0）**；门侧并档 M5b-G-002 |
| F00-02（状态无单一事实源） | 四份 RELEASE_STATUS 并存 + 三套状态词 + `check_agents_gov.py:15` 强制保留 `REVIEW_PENDING` 而 `validate_task_ledger.py:42-43` 判其非法 + `docs/TROUBLESHOOTING.md` 通篇已废止执行面 | 仍成立 | **M6b-G-002（P0）**（README 状态段/ARCHIVED 权威条归 M5b） |
| F00-04（未接入却整行 VERIFIED） | 复现且**扩量**：自述未落地却 7 层全 VERIFIED = **14 行**（前台注记的"同类 10 行"为低值）；`grep photometric_calib CMakeLists.txt` = 0 命中 | 仍成立 | **M6b-G-001（P0）实例 C** |
| F00-05（本版实测类无据证据） | `docs/ARCHITECTURE.md:3/:114-125` "S8 gate 本版实测 PASS 9/9 / mismatches=[]" 全文无 SHA/命令行/时间/日志指针；且 `docs_machine_consistency.py`、`config_consistency_check.py` 实测 **不在 ci/checks.json 的 75 个 id 内**，`tests/**` 亦零引用 | 仍成立 | **M6b-G-003（P0）** |

### 1.2 L16（19 条）

| 叶子条 | 叶子类/优先级 | 复算要点 | 四态 | M6b 定稿 |
|---|---|---|---|---|
| L16-001 | E/P0 | 22 VERIFIED 中 18 锚 docs/；C7 只判存在+token 可见；C8 跳过 SRC/TEST | 仍成立 | G-001（实例 A）+ G-004 |
| L16-002 | E/P0 | 27/30 EVIDENCE MISSING；3 行 VERIFIED 无路径列；`returns/` 0 文件；三 ID 在 evidence/reports 零命中 | 仍成立 | G-001（实例 B）+ G-004 |
| L16-003 | G/P0 | `quality/check_traceability.py:159 return 0`（只打印 broken/sym_broken）；`tools/check_traceability.py:16` 默认表 = `artifacts/prerelease_v5` 快照 | 仍成立 | G-004 + E-001（门侧并档 M5b-G-002，不重复定稿） |
| L16-004 | C/P0 | 14 行自述未接线却整行 VERIFIED（>10） | 仍成立 | G-001（实例 C） |
| L16-005 | C/P0 | `PHASE2_INTEGRATION.md:42/:64/:234-236` 仍把 sup_max 登记为"现状缺陷"，而 `integrate.cpp:44-50` 已按 B2-A7 整改且 `tests/unit/p2_output_semantics_test.cpp:85` 有回归门 | 仍成立（文档侧） | **C-002（P1）** |
| L16-006 | A/P1 | `INTEGRATION.md:58` `max_{valid,W>0}` 与 :63 `max(accepted support)` 作用域不等价；`stage2_common.h:17-19` 为唯一文本权威 | 仍成立 | **移交 M3a（A_SCI_DEF）**；C-002 只在文档一致性面复用其结论 |
| L16-007 | E/P1 | 交比 ID 复算 **8**（非 15），TEST 证据一致 **0/8 = 100% 互斥**；无门同读两表 | 仍成立（定量修正） | **E-001（P0）** |
| L16-008 | E/P1 | C4"任一锚命中即通过"、C7 不校验 SCI/ALG 锚、`git_ls` 无 timeout、界内错锚放行 | 仍成立 | **E-002（P1）根因** |
| L16-009 | E/P1 | 裸 `| :NN |` 表格锚复算 **223 处 / 11 篇**；合同自述 36 文档 793 锚 vs 实测 41 篇 805 锚 | 仍成立 | E-002 + G-006 |
| L16-010 | G/P1 | `PRODUCTION-GRAPH` 只 `--selftest`；`CON-COMMENTS` 指向 `check_comments.py`（与 §12.2 无关）；`check_comment_hygiene.py:116 return 0`；`validate_task_ledger.py` 未登记为检查项 | 仍成立（部分并档） | G-004（注释门空壳本体归 M5b/M6a） |
| L16-011 | E/P1 | 6 行 SCI=MISSING 而 DATA/API=VERIFIED（复算 6，非 9）；`STATUS_OK` 全局并集；SPEC:110-111 与实现判向相反；LAYERS.csv/schema 取值域不被解析 | 仍成立（定量修正） | **G-004 + G-001④** |
| L16-012 | E/P1 | 152 非占位 ID / **85** 不在 INDEX.yaml（复算一致）/ VERIFIED 合同层 **33 处（去重 28）**；descriptor 22 模块 93 ID / **72** 不在 INDEX | 仍成立 | **E-003（P1）** |
| L16-013 | E/P1 | `science_doc=docs/algorithms/STAR_PSF_ALGORITHMS.md::SCI-P1-PSF-001` 实测 token 在该文件 :203 命中 → 从"指向不存在文档"改判为"以 ALG 页冒名 SCI 权威页"；`docs/science/STAR_DETECTION.md` 实测存在且含 `SCI-P1-STAR-001` | 部分成立（改判） | **E-003 + G-001（SCI 锚不被 C7 校验）** |
| L16-014 | I/P1 | SCIENCE_FREEZE 现 77 行（旧引 :100-:125 越界）；:3-5 现在时 PASS、:4 引用不存在的 ACCEPTANCE_GATES.md、`self_review/` 实际在 `reports/self_review/`、无 ARCHIVED 标记、DOCUMENT_INDEX:535 仍 ACTIVE_INFORMATIVE | 部分成立（收窄） | **I-002（P1）** |
| L16-015 | F/P1 | 八层模型每层 1 ID+1 路径槽，无法表达 §13.1 九项必备；`check_test_contracts.py:36-43` 伞形豁免 | 仍成立 | **G-004 + F-001** |
| L16-016 | G/P2 | git 失败 → `tracked_set=None` → :206/:234 两项跟踪检查整段跳过且仍出 PASS；`subprocess` 无 timeout | 仍成立 | **E-002（第④项）+ G-004** |
| L16-017 | E/P2 | 合同 `"expiry"` 实测 0 命中；7 条豁免中 2 条锚 `run/local/bughunt/ledger.md:249`；C5 要求豁免继续命中 | 仍成立 | **G-004 + E-002** |
| L16-018 | I/P2 | SPEC:30「8 必填层」下表 9 行；SPEC:37 唯一性 vs :86 不判重；SPEC:159 `rows=` vs 实现 :452 只打 modules；CI changed_paths 指向不存在的根 `schemas/…`；试金石 RELS 也按该错路径建 env | 仍成立 | **E-005（P2）** |
| L16-019 | I/P2 | `docs/quality/complexity_baseline_v1.md:22` 以 `lib/orchestrator/cpp/src/orchestrator.cpp`（生产禁用路径）为最高复杂度基线、:4 引用不存在的 `07_CI_MACHINE_CONTRACT.md`；`coverage_baseline_v1.md:11` 同引该不存在文件；两文档均 status: ACTIVE | 仍成立 | **I-003（P2，实例并入）** |

### 1.3 L18（26 条）

| 叶子条 | 叶子类/优先级 | 复算要点 | 四态 | M6b 定稿 |
|---|---|---|---|---|
| L18-001 | C/P0 | `docs/architecture/cpu/ARCH_CONTRACTS.md` 实测 0 文件；4 处 ACTIVE 文档（含 2 份 L2 合同表）把 ARCH 层标 VERIFIED | 仍成立 | **E-004（P1）+ G-001（P0，实例 D）** |
| L18-002 | C/P0 | `KNOWN_LIMITATIONS.md:13-14`、`TROUBLESHOOTING.md:14/:16` 把"缺 ivar → 权重回退 support"写成既有行为；实现 `stage2_common.h:94` 默认 `legacy_allow_weight_fallback=false`；宪章 §6.3:191 明文禁止 | 仍成立（文档面） | **C-001（P1）**；科学/实现面与 M3a/M4 会签（前台指定三案合一） |
| L18-003 | I/P1 | V19 世代叙述：`docs/ARCHITECTURE.md:16/:18/:51/:52`、`docs/API_REFERENCE.md:112/:127`、`docs/modules/phase2.md:6`、`docs/TROUBLESHOOTING.md` 全篇 | 仍成立 | **I-001（P1）+ G-002** |
| L18-004 | E/P1 | `ARCHITECTURE.md:129`「76 行」实测 67 数据行；`docs/TRACEABILITY_family.json` 实测 0 文件；:130「13 份 L5」实测 23 份 + 26 registry | 仍成立 | **I-003（P2，实例并入）+ E-001** |
| L18-005 | I/P1 | 两套矩阵并存且互否：SPEC:15（JSON 权威）vs README-DOCS:13（旧表唯一矩阵）vs SPEC:116-117（"互补不冲突"） | 仍成立 | **E-001（P0）** |
| L18-006 | E/P1 | 不成立：JSON 30 模块 = CSV 30 数据行，23 个非 notes 列逐行零差异；`_check_csv_parity`（:374-411）本就是 ERROR 判据 | **不再成立（叶子读截断所致）** | 剔除，另立 **F-001**（该判据无 fixture 守护） |
| L18-007 | G/P1 | 宪章 §12.1:416 点名的 `docs/owner/PHASE_OVERVIEW.md` 实测 0（现有 `PIPELINE_OVERVIEW.md`）；`docs/history/` 0；`docs/standards/02_03_06_*` 编号与文件名互指不一致 | 仍成立 | **G-007（P2）+ E-004** |
| L18-008 | E/P1 | L0 锚复算：`RELEASE_STATUS.md:70` 的 :4257/:4282/:4309 现分别为 `}`/注释/`}`，三表实测在 :5482/:5507/:5534（文件 5541 行）；:71 → :3777 无关；:94 → `p3_wcs.cpp:36` 为坐标注释 | 仍成立 | **E-002（P1，根因条目）**；MODULE_MAP 实例归 M5b |
| L18-009 | I/P1 | 四份 RELEASE_STATUS 并存 + `docs/review/RELEASE_STATUS.md:29` 复活 PASS 口径 | 部分成立 | **G-002（P0）**；「docs/review 未登记」子项**不成立**（DOCUMENT_INDEX:321 有目录级条目，覆盖判据按前缀匹配）→ 剔除 |
| L18-010 | G/P1 | README:9 以 ARCHIVED 文件为权威、:25-26 Linux 职责、:33 旧投影集、:35-36 与 owner:75 冲突 | 仍成立（部分并档） | 移交 M5b（README/ARCHIVED 权威条）；投影集部分入 **C-003** |
| L18-011 | G/P1 | `PHASE3_PROJ_IMPL.md:20/:190` 仍写 `SIN/ZEA/CAR/AIT`，同文件 §15（:348-:414）已按裁决写 TAN/SIN/CAR/AIT；registry 页实测 12 处"占位 ID"字样 | 部分成立（改判为同文档内互斥） | **C-003（P1）**；数值/实现面移交 M2 |
| L18-012 | E/P1 | 根 `schemas/`、`launch/` 实测 0 目录；`VERSIONING.md:16/:32`、`CLI_PROTOCOL_V1.md:39`、`CHANGELOG.md:40`、`VISUAL_CHECK_README.md:8/:17`、`SPEC:20` 仍用旧前缀（真实 `contracts/schemas/`、`packaging/launch/`） | 仍成立 | **E-004 + E-005** |
| L18-013 | G/P1 | `include/astrocs/exit_codes.h`、`tools/check_cli_protocol.py` 实测 0 | 仍成立 | 本体移交 **M5b-G-015**；本域只在 E-004 登记悬空面 |
| L18-014 | E/P1 | 实测 `modules/services/io/` 仅 4 文件，IO_001:27/:30 声明的 README/module.yaml/CMakeLists/io_module_api_v1.h **3/4 不存在** | 仍成立 | **E-007（P2）**（注：本条编号在 `_cache/L18.md` 中即 IO_001，前台任务书把它当作"24/25 文档条"，ID 映射已在 §3 订正） |
| L18-015 | E/P1 | `docs/audit/doc_classification.csv` 中 `docs/history/` 行实测 15 行（目标目录 0 文件） | 仍成立 | **I-003 + E-004** |
| L18-016 | G/P1 | `SPEC:6-8/:135-136` 把权威顺序锚在已废止控制包 `16_*` 与 ARCHIVED 的 DOC-GOV-CONSTRAINTS-001 的 `$F.2`（实测 0 命中） | 仍成立 | **E-006（P2）** |
| L18-017 | I/P1 | `GLOSSARY.md:14` `coverage → support` 同义映射，与宪章 §4.1「不得混淆的量」表（:100-103）+ §6.3:191 冲突 | 仍成立 | **移交 M3a（A_SCI_DEF）**；文档面入 I-003 附注 |
| L18-018 | C/P1 | PHASE3_RESAMPLE 文档内部三态互斥（DRAFT / 权威冻结 / 仅 TAN） | 仍成立 | **移交 M2**（实现口径）；本域只登记于 C-003 |
| L18-019 | E/P1 | registry 占位 ID 实测 12 处（6 文件）：`astrocs.phase2.resample.md:39-40`「1/N 等价已验」与其自述占位冲突；`docs/API_REFERENCE.md:16` 引 `docs/science/HEALPIX_MAPPING.md` 实测 0（真实在 `docs/algorithms/`） | 部分成立 | **G-001（实例 D）+ E-003** |
| L18-020 | C/P1 | `STAR_PSF_ALGORITHMS.md:201`「状态：VERIFIED（设计冻结，…落 EVIDENCE）」；矩阵对应行 TEST=VERIFIED 而 evidence=MISSING；`lib/dynamic_psf/tests/p1psf/` 实测 8 文件 | 仍成立 | **G-001（实例 A/D）** |
| L18-021 | E/P1 | `docs/architecture/CPU_ADAPTIVE_V1.md` 实测 0（被 `PHASE2_COVERAGE.md:243` 与 `lib/phase2/README.md:87` 当权威引用） | 仍成立 | **E-004** |
| L18-022 | I/P2 | `docs/performance/BASELINE.md:12` 指向 `evidence/performance/*.json`（实测 0 文件）；`docs/browser/HIPS_BROWSER.md` 停 V14/V15 冻结叙述 | 仍成立 | **I-002** |
| L18-023 | I/P2 | `memory.md:108-112` 仍留控制包流水/交接段落（与其 :3-4 自定"只保留稳定目标、当前 SHA、模块索引、开放问题"不完全一致） | 部分成立 | **移交 M6a（注释与记忆治理）** |
| L18-024 | I/P2 | INDEX.yaml 取值域与 descriptor 实际取值域不符；矩阵/ descriptor 大量 TEST 槽无测试锚 | 仍成立 | **E-003 + F-001** |
| L18-025 | C/P2 | BUILD_GRAPH 描述 `lib/astro_image_io/CMakeLists.txt`、`runtime/runtime.h`、`lib/browser/` | 移交 | **M5b-G-005**（其已定稿构建图门条） |
| L18-026 | C/P2 | `SCI` 层以模块 `memory.md` 口语记录充当科学依据 | 部分成立 | **G-001（实例 D）+ F-001**；记忆治理面移交 M6a |

---

## 2 复算与原文对照（关键项）

| 项 | 转述数 | 本代理复算数 | 方法 |
|---|---|---|---|
| 矩阵行数 | 30（JSON）vs 26（CSV）不同构 | **JSON 30 = CSV 30**，23 非 notes 列 **0 差异**（三件套口径，见 §0.1） | 元素计数（grep `^MOD-` = 30 / `"module_id":` = 30）+ 列名与键集合互校验（24 列，双向零差）+ 逐列比对（截断只落在 notes，见 §0.1） |
| 两表可交比 ID | 15，TEST 证据 100% 不一致 | **8**（三口径：8 / 8 / 含 notes 11，均非 15），TEST 证据一致 **0/8** | 旧表 `requirement_id+algorithm_id+test_ids` ∩ 矩阵九层 ID |
| TEST 层 | 22 VERIFIED 中 18 锚文档 | 复现 **22 / 18**（另 4 行锚真实测试；**11** 行为 `*-DESIGN-001`；本代理初稿写 9，系 read 长行截断伪影，已按整行 grep 计数更正） | `"test_status": "VERIFIED"` / `"test_path": "docs/` / `"test_id": "…DESIGN…"` 三条整行 grep 计数 |
| EVIDENCE 层 | 27/30 MISSING，3 行指向 returns/ | 复现 **27 / 3**；`returns/**` = 0 文件；三 EVID-* 在 `evidence/**`+`reports/**` **0 命中** | glob + 两目录 grep |
| 自述未接线却整行 VERIFIED | 10 行（前台注记） | **14 行**（全 VERIFIED 共 19 行） | 逐行 notes 扫描 |
| SRC 锚在声明 | — | 22 个 VERIFIED 中 **11** 锚 `.h/.hpp` | 后缀判定 |
| SCI=MISSING 而下游 VERIFIED | 9 | **6** | 非豁免 kind 过滤后逐行 |
| 矩阵 ID 未登记 | 152 / 85 / 33 | **152 / 85 / 33 处（去重 28）** | INDEX.yaml `- id:` 全集比对 |
| descriptor ID | 22 / 93 / 72 | **22 / 93 / 72** | `module_adapters.cpp` 逐字段 |
| 锚规模 | 合同 36 文档 / 793 锚 | **41 篇 / 805 带名锚 + 223 裸 :NN 锚** | 移植 ANCHOR_RE |
| 不存在文档被引用 | 15+ 处 | `lib/**` **36 文件 133 处** + `docs/**` **3 文件 5 处** | 六类文档名并集 grep |
| DOCUMENT_INDEX | 159 条仅 11 条入锚合同 | **236 条（active 205 / archived 31；ACTIVE_NORMATIVE 123）**，锚门作用域 **41 篇** | 索引条目 + doc_globs 实测命中 |
| CI 检查项 | 111 | 本轮 `"id":` 命中 **75**（快照差异） | ci/checks.json 全文枚举行 |
| L0 锚抽查 | 4/14 漂移 | **4/4 抽点全错**（:4257/:4282/:4309/:3777 + p3_wcs:36） | 逐行 read 对照 |
| ivar 回退口径 | 文档承认回退 | 实现默认 `legacy_allow_weight_fallback=false`；宪章 §6.3:191 禁止 | 三源逐字对照 |
| 豁免到期 | 无 expiry | 合同 `"expiry"` 命中 **0**；2/7 条锚 `run/**` | grep + 逐条读 |

---

## 2.5 任务书编号与 _cache 实际编号对照（防前台按错号并线）

| 任务书所指 | 实际 _cache 条目 | 处理 |
|---|---|---|
| 「L16-011 = 交叉引用/门侧证据」 | L16-011 实为「链断裂方向与取值域未实现；LAYERS.csv/Schema 是死文档」 | 事实定稿于 **M6b-G-001④ + G-004**；交叉引用门侧证据实际来自 L16-008/009/016/017 → **M6b-E-002** |
| 「L18-014 = 24/25/02_FROZEN/05_COMMON_CONTRACTS 不存在」 | L18-014 实为「IO-001 冻结合同文件清单与树内实存不符」 | 两条均定稿：清单面 → **M6b-E-007**；不存在文档族 → **M6b-E-004**（计数由转述「15+ 处」复算为 **138 处**） |
| 「L18-016 = DOCUMENT_INDEX 宣称机器门覆盖 vs 实测覆盖」 | L18-016 实为「追溯合同把权威层级锚在被废止控制包/已归档约束」 | 分定为 **M6b-G-006**（覆盖率复算）与 **M6b-E-006**（权威锚失效）；转述「159 条仅 11 条入锚合同」当前树不可复现，改为 **205 active / 123 ACTIVE_NORMATIVE / 锚门 41 篇** |
| 「L18-008 = 交叉引用失效」 | L18-008 实为「owner/review L0 源码行锚漂移」 | 定稿于 **M6b-E-002**（根因）+ **M6b-C-003**（:94 锚实例） |
| 「两表可交比 15 个 ID」 | 实测 8（含 notes 提及 11） | 已在 **M6b-E-001** 定稿段与 §2 复算表注明修正 |
| 「整行 VERIFIED 同类 10 行」 | 实测 14 行 | 已在 **M6b-G-001 实例 C** 扩量 |

---

## 3 显式剔除与降级清单（无静默丢弃）

**剔除（2 条，均给出理由）**
1. **L18-006（JSON/CSV 不同构 30 vs 26）** — 不成立：复算两表 30/30 且逐列一致；叶子把 `read` 的长行截断误当数据缺失（本代理首轮犯同样错误并以 `grep -c` 纠正）。其判据 `_check_csv_parity` 真实存在且属 ERROR，缺的是样本守护 → 改立 **F-001**。
2. **L18-009 的「docs/review 五文档未登记」子项** — 不成立：DOCUMENT_INDEX:321 有 `- path: "docs/review"` 目录级条目，`check_doc_index.py:122` 的覆盖判据按 `p.startswith(cp.rstrip('/')+'/')` 前缀匹配，故门判"已覆盖"。剩余事实（PASS 口径回流、四份并存）保留并升为 M6b-G-002。

**降级 / 改判（6 条）**
- L16-011：9 → **6 行**（前导链 VERIFIED-on-MISSING）；本体不独立成条，并入 G-001④ + G-004（避免与"未经证据支撑的已验证声明"重复定稿）。
- L16-013：从"SCI 锚指向不存在文档"改判为"以 ALG 页冒名 SCI 权威页 + C7 不校验科学层路径"（token 实测命中 `docs/algorithms/STAR_PSF_ALGORITHMS.md:203`）；`docs/science/STAR_DETECTION.md` 是否"为凑锚造页"保留为评价性判断，不作事实断言。
- L16-014：整条从"现在时虚假验收结论"收窄为"引用文件不存在 + 无归档标记 + 指针少一层 `reports/`"，并删除越界行号引用（现文件 77 行）。
- L18-011：从"ACTIVE 文档流通旧四投影集"改判为"同一文档内新旧两版并存（§0/§11 vs §15）"，故 C-003 而非实现缺陷；实现/数值面移交 M2。
- L18-019：「未定义追溯 ID」成立，「无据已验」限定到"1/N 等价已验"这一处（其余 registry 完成度声明由 M5b-G-018 定稿），不重复。
- L16-010 / L16-003 / L16-016 / L16-017：合并为单条 **G-004**（同一根因：门判据与合同声明脱节 + fail-open 通道），逐实例保留 `path:line`。
- L18-002：由 P0 降为 **P1**（文档口径互斥），因实现默认值与宪章均禁止回退，"回退已发生"无当前证据；升 P0 的条件（M4/M3 证明生产路径确实回退）已写在 C-001 影响段。

---

## 4 已修复（并发提交期间）

| 项 | 现状 | 是否有回归守护 | 缺口处理 |
|---|---|---|---|
| L16-005 / L18-018（integrate.cpp sup_max 与 INVALID_INPUT 分支） | `lib/phase2/src/integrate.cpp:44-50` 已按 B2-A7 把 canonical reducer 前移；`tests/unit/p2_output_semantics_test.cpp:64/:85/:126` 有专门回归门 | **有**（同提交带测试） | 无 |
| L18-006（CSV/JSON 同构） | 本轮实测两表同构 | **有**：`check_traceability_matrix.py:374-411` 的 `_check_csv_parity` 属 ERROR 判据 | 无（试金石缺 fixture 的事实在 F-001 内） |
| L18-008 的部分锚（`docs/architecture/MODULE_MAP.md`） | 由其他 Agent 订正 | 部分（无界内错锚负例） | 归 M5b 实例 + F-001 |
| 「docs/owner/PHASE_OVERVIEW.md」 | 仍不存在（宪章 §12.1 点名） | — | **未修复**，保留于 G-007 |
| SCIENCE_FREEZE 的 `self_review/` 指针 | 实际文件在 `reports/self_review/round6_final_verification.md`，文档写 `self_review/round6` | — | 部分修复，残余（少 `reports/` 前缀 + 引用不存在的 `ACCEPTANCE_GATES.md`）登记于 I-002 |

> 说明：并发修复面广（B2-A1..A18 同时在跑），本表只列**本域复算确证**的已修复项；判定四态所依据的均为最后一轮读取的当前树内容。

---

## 5 移交清单

| 交出去的事实 | 目标 M | 我保留的挂接 | 理由 |
|---|---|---|---|
| L16-006（SCI 两条 reducer 表述不等价 + ALG 单方裁定 header 权威） | **M3a** | C-002 引用其结论 | 科学定义冲突属 A_SCI_DEF 主域 |
| L18-017（GLOSSARY 把 coverage 并入 support） | **M3a**（术语/科学） | I-003 附注 | 宪章 §4.1/§6.3 冲突由科学域裁决；本域只管交叉引用形态 |
| L18-011 / L18-018 的投影实现与数值口径 | **M2** | C-003（文档互斥面） | 冻结投影数值属标准/实现域 |
| L18-013（退出码唯一源 + CLI 协议门不存在） | **M5b**（其 G-015） | E-004 只登记悬空 | 门本体已在他处定稿 |
| L18-010（README 以 ARCHIVED 文件为权威） | **M5b** | G-002 并档 | 协调记录指定 |
| L18-025（BUILD_GRAPH 与当前树不符） | **M5b** | 无 | 其构建图门条目已定稿同类 |
| L18-023 / L18-026（memory.md 流水与口语依据） | **M6a** | G-001 实例 D | 注释/记忆治理归 L 文档内容域 |
| L16-003（TRACEABILITY 门 fail-open） | 与 **M5b-G-002** 并档 | G-004 保留清单角色 | 同一事实只定稿一处 |
| 旁路构建面（`cmake -S cli`、`cmake -S lib/phase2`、Makefile/build.ps1） | **M5a/M5b** | G-002 以"权威不可判定"并档 | F00-02 要求并档 |
| 行锚系统性漂移 | **本域（我）** | E-002 为唯一根因条 | 他域只落实例（L05-017、L07-016、L10、L12-017、L15-018、L17-005） |

---

## 6 无法判定项（§6 单列）

1. **L16-019 的「基线文档引用不存在的检查 id」**：`docs/quality/complexity_baseline_v1.md:22` 与 `coverage_baseline_v1.md` 引用的 `DEEP-COMPLEXITY`/`DEEP-COV` 与本轮枚举行的对应关系，因 `ci/checks.json` 在本代理读取期间从 3323 行变为 3355 行（id 命中数由 70→75 波动），无法给出稳定的"引用不存在"结论 → 只定稿其"以生产禁用路径 `lib/orchestrator/cpp/src/orchestrator.cpp` 为基线对象 + 引用 `07_CI_MACHINE_CONTRACT.md`（实测不存在）"两点。
2. **旧表 `test_ids` 的 8 个 distinct 测试文件是否覆盖其声明的科学断言**：文件全部存在，但逐 claim 的覆盖判定需按 §13.1 九项跑真实测试，只读域内不可判定（移交 M3/M4 的测试面）。
3. **evidence 合同按 ID 还是按任务目录解析**：`evidence/refactor/tasks/{CPU-001,IO-001}` 目录存在但内无 `EVID-*` token；若 C8 的合法口径是"按任务目录"，G-001 实例 B 应改判为"ID 未登记"而非"证据不存在"——两种口径下问题都成立，但表述不同。本条按最不利口径定稿并在此登记。

---

## 7 行号漂移处置

- **原则**：定稿锚一律写 `path::符号/字段`；行号仅作辅助，且本文件所有行号标注为"现 :`n`（本轮读取）"。**行号不符永不构成剔除理由。**
- **本轮实测漂移证据**：`lib/core/src/module_adapters.cpp` 现 5541 行，L0 `docs/owner/RELEASE_STATUS.md:70` 所引 :4257/:4282/:4309 现落在 `}`/成员注释/`}`，三张节点表实测在 :5482/:5507/:5534（漂移 >1200 行）；`lib/phase2/src/integrate.cpp` 在整改后由 76→81 行，锚合同 reason 仍写"实际 76 行"；`docs/validation/SCIENCE_FREEZE.md` 从 >125 行缩到 77 行，使 L16-014 的 :100-:125 引用越界；`ci/checks.json` 从 3323→3355 行、id 命中 70→75。
- **处置方式**：①凡引用行号者，定稿时同句给出"该行的判据字段/符号名"（例：`TRACEABILITY_MATRIX.json::modules[module_id=MOD-astrocs-phase1-photometry].src_status`）；②对整表类事实（如 22/18、27/3、14 行）改以"筛选谓词 + 计数"表达，任何读者可复算，不依赖行号；③已发现因截断读取造成的少数错误（L18-006）在 §0 与 §3 明示方法与纠正过程，供后续代理避坑；④本域 4 条 P0 的定稿不依赖任何单一行号，行号全部作废时结论仍成立。

---

## 8 自检：字段完备性

22 条定稿（E 7 / G 8 / I 3 / C 3 / F 1）逐条含：类别码 / 优先级 / 位置（`path::符号` + 现行号）/ 逐字证据摘录 / 权威依据（宪章条款号、SCI/ALG/ARCH ID、合同与检查项 ID、SPEC 节号）/ 问题说明 / 影响 / 建议处置 / 置信度 / related / 来源（Lxx-nnn 或 F00-nn）。合并条（G-001、G-004、E-002、E-004、I-003）全部保留各实例的 `path:line`，未压缩丢失。
