# GOV — 治理与权威链收敛审计（ROOT-004 扩展轴）

- **轴名**：GOV（治理与权威链收敛）｜**代号**：GOV
- **基线**：按父指令新政策（三 SHA 相等即可开工，固定 SHA 门槛作废）。开工 HEAD=main=origin/main=`900916fb0dfe93e21bd36c09908a6cc679de5256`；收工 HEAD=main=`d414c3e0e59142a4fc48760ceb97fe9ae828486b`，origin/main=`b4afc135`（并行线领先提交致收工时三 SHA 暂不等，非本轴证据锚）。
  中途漂移：`01db973b`（ROOT-007 删四旧权威根文档）→`7940d70e`→`b46a316f`（evidence 清运）→`4e95f841/b1290525`（删 artifacts/）→`b4afc135`→`d414c3e0`。首轮固定基线 `2c328348` 不符曾中止一次（见日志前半）。
- **方法**：只读取证：精读 DESIGN §0/§12、ENGINEERING_SPEC §7/§8、CP_SPEC §3/§4/§7、docs/ci/01/03、AGENTS.md；grep/python 普查当前树；直跑只读检查器取 rc；全部行号在 `b46a316f`~`d414c3e0` 复核。FATDUCK 仅 grep 文件名，未读内容。
- **摘要**（≤10 行）：
  1. ROOT-007 已删四旧权威根文件，但活动面引用与机器门注册未同步——悬空引用成为本轴主形态。
  2. README/memory/HANDOVER 三根文件仍自称旧权威链（宪章＞AGENTS＞memory），不含 DESIGN（GOV-1）。
  3. docs/DOCUMENT_INDEX.yaml 把已删宪章登记为 ACTIVE_NORMATIVE「唯一最高约束」，把 VERSION 登记为 ACTIVE_NORMATIVE，把已删控制包称为唯一 ACTIVE 包（GOV-2/8）。
  4. 三项已注册门（DOC-INDEX/ENG-CONSTRAINTS/DOC-L0）因指向已删文件实测红；ci/checks.json、ci/impact_map.json、ENGINEERING_SPEC:110 仍引用已删文件（GOV-3）。
  5. tests/代码层把已删宪章当仓库根探针与约束依据（GOV-4）。
  6. AGENTS.md 硬禁令机器门 AGENTS-GOV 实测红（10 关键词 MISS），CI 规范 CHK-AGENT-HARD-RULES 与注册表双头（GOV-5）。
  7. docs/README-DOCS.md 自称「Wiki 为准」权威链，与 DESIGN §0 冲突（GOV-6）。
  8. FATDUCK_ACCESS.md 仍 tracked、不受 .gitignore 豁免生效、被索引登记 ACTIVE_INFORMATIVE（GOV-7，P0，与 ROOT-006 同源）。
  9. 工程控制/两包偏离 CP_SPEC §4 唯一模板；ROOT-007 已执行但 TASK_LIST/ACCEPTANCE 零登记、任务数 33≠34 行（GOV-9）。
  10. 证据抖动两处（root_cleanliness 瞬态红→绿）已注明，不立条。
- **发现计数**：**P0=1 ｜ P1=7 ｜ P2=2 ｜ 合计 10**

## 发现表

| ID | 定位 | 违反条款（最新权威） | 当前证据（命令→逐字≤3行） | 严重度 | 影响 | 整改建议（最小改动面） | 建议文件域 | 验收门（单命令） | GAP/任务关系 | 旧清单同源 |
|---|---|---|---|---|---|---|---|---|---|---|
| GOV-1 | README.md:12-13；memory.md:10-13,43；HANDOVER.md:7-8 | ASTROCS_DESIGN.md §0（L29-31：与其他文档冲突以本文为准；Agent 无权放宽解释）——三文件宜称「冻结宪章＞AGENTS.md＞memory.md」为权威链且不含 DESIGN | `grep -n 'ASTROCS_PROJECT_CONSTITUTION' README.md memory.md HANDOVER.md` → 「README.md:12:> 根 `ASTROCS_PROJECT_CONSTITUTION.md` 是 FROZEN 唯一最高约束；」「HANDOVER.md:7:> 权威：…（冻结宪章，GOV-001 FROZEN」 | P1 | Agent 开工链第一入口指向已删非权威文件，权威链不收敛 | 三文件抬头块改为 DESIGN §0 链；HANDOVER 处置按 ROOT-007 保留表归 GOV-001/DOC-001 裁决 | README.md, memory.md, HANDOVER.md | `timeout 30 grep -c 'ASTROCS_PROJECT_CONSTITUTION\|REVIEW\.md' README.md memory.md HANDOVER.md`→0 | 与 GAP-001/002 同源不删条；GOV-001 | M5b-E-05 |
| GOV-2 | docs/DOCUMENT_INDEX.yaml:9-13,35-37,38,42-43,48,50-51,556-558,749,753,755-757 | DESIGN §0 L29-31；ENGINEERING_SPEC §8 L119（删除/重命名无悬空引用）| `python3 tools/doccheck/check_doc_index.py`→`"verdict": "DOC_INDEX_FAIL"` rc=1；索引仍登 `ASTROCS_PROJECT_CONSTITUTION.md: ACTIVE_NORMATIVE`、:749 称已删 `工程控制/AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912/` 为「唯一 ACTIVE 控制包」、:16 称唯一登记源=`工程控制/ACTIVITY_STATE.md`（文件不存在） | P1 | 机器索引自称与 §0 冲突的权威链且大面积悬空 | 按 §0 链重建索引：删宪章/CHANGELOG/REVIEW ACTIVE 条目、VERSION 降级、docs/standards 出 ACTIVE_NORMATIVE、清 ACTIVITY_STATE/RESCUE 引用；或整体废止索引交 CI-001 | docs/DOCUMENT_INDEX.yaml（连带 tools/doccheck） | `python3 tools/doccheck/check_doc_index.py --strict; echo rc=$?`→0 | 与 GAP-002/019 同源；GOV-001/DOC-001 | M6b-E-006 |
| GOV-3 | tools/doccheck/check_engineering_constraints.py:34；ci/checks.json:185,1073；ci/impact_map.json:559；ci/tests/test_impact_map.py:77；ci/check_version.py:38；ENGINEERING_SPEC.md:110 | ENGINEERING_SPEC §7 L109（修改代码/测试后同步订正 ci/checks.json）+ §8 L116-119（注册表唯一、无悬空） | `python3 tools/doccheck/check_engineering_constraints.py`→`"check":"constraints_file_exists"…pass:false` rc=1；`python3 tools/check_l0_docs.py`→`DOC-002_L0_VIOLATION: REVIEW.md missing` rc=1 | P1 | ROOT-007 删除未同步，三项已注册门红、注册表指向不存在文件 | 移除/改写 ENG-CONSTRAINTS 注册项与其检查器、check_l0_docs 的 REVIEW 期望、impact_map、check_version；§7:110 的 CHANGELOG 承认条款由前台/负责人订正 | ci/**, tools/doccheck/**, ENGINEERING_SPEC.md | `timeout 60 python3 tools/doccheck/check_engineering_constraints.py; echo rc=$?`→0（或该项已注销） | 与 GAP-016/027 同源；GOV-001+CI-001 | M6b-E-004 |
| GOV-4 | tests/unit/p1001_real_nodes_test.cpp:134,139；tests/cli/test_cli001_vpi.py:4；lib/acr/README.md:11、lib/acr/CMakeLists.txt:4、lib/acr/ci/check_acr_dormant.py:6；lib/dynamic_psf/tests/p1psf/p1psf_oracle.hpp:112、p1psf_tests_perf.cpp:5；lib/snr_estimator/README.md:194 | DESIGN §0 L31（不得为通过检查改写权威）；ENG_SPEC §8 L119 无悬空引用 | `grep -n 'ASTROCS_PROJECT_CONSTITUTION' tests/unit/p1001_real_nodes_test.cpp`→`:139: if (fs::exists(probe / "ASTROCS_PROJECT_CONSTITUTION.md", ec))`（已删文件作仓库根探针） | P1 | 测试根探测逻辑随删除失效，注释层以已删约束为绑定依据 | 探针改指 ASTROCS_DESIGN.md 或 CMakeLists.txt；权威注释改引 §0 链现行条款号 | tests/**, lib/**（注释/README） | `timeout 120 grep -rn 'ASTROCS_PROJECT_CONSTITUTION\|AstroCS_ENGINEERING_CONSTRAINTS' tests lib cli\|wc -l`→0 | 与 GAP-019 同源；GOV-001（ARCH-001 顺带） | M6b-E-004 |
| GOV-5 | ci/checks.json:185（AGENTS-GOV=tools/check_agents_gov.py）；对照 docs/ci/01_CHECKS.md:22 | docs/ci/01_CHECKS.md §2 CHK-AGENT-HARD-RULES=P0；03_GATES §1（P0 红灯无豁免）；DESIGN §0 链②层硬验证 | `python3 tools/check_agents_gov.py`→`GOV_CHECK_FAIL missing=['main-only','amd64','节点','cpu-only','单入口','资源门禁','无硬编码','alpha/发布','状态机','不停工']` rc=1 | P1 | 权威链第二层的机器验证红且检查器口径与现行 AGENTS.md 文本脱节；CI 规范 CHK-* 与注册表双头（CHK- 注册数 0） | 检查器关键词表对齐现行 AGENTS.md §5，或按 docs/ci 注册 CHK-AGENT-HARD-RULES 并注销旧 AGENTS-GOV | tools/check_agents_gov.py, ci/checks.json | `timeout 60 python3 tools/check_agents_gov.py; echo rc=$?`→0 | 与 GAP-016 同源；CI-001/GOV-001 | 无（本轮新测） |
| GOV-6 | docs/README-DOCS.md:6（L0 含已删 CHANGELOG.md）、权威链行（L15-19：Wiki 前置、「矛盾以 Wiki 为准」）、L4=docs/standards、「docs/history/」 | DESIGN §0 L29-31；docs/ci/01_CHECKS.md:24 CHK-STALE-DOC | `grep -n '矛盾以 Wiki 为准' docs/README-DOCS.md`→「Wiki 为 L0 前置核心约束（见 V19R8 S0…矛盾以 Wiki 为准…」；L0 列表含 `CHANGELOG.md`（已删） | P1 | 文档体系入口宣告与 §0 冲突的权威链并给非权威层（standards）以规范位 | 按 §0 链重写或删除 README-DOCS 并入索引重建；删 Wiki 优先声明 | docs/README-DOCS.md | `timeout 30 grep -c '矛盾以 Wiki 为准\|CHANGELOG' docs/README-DOCS.md`→0 | 与 GAP-026/002 同源；WIKI-001/GOV-001 | SA-N-04（部分） |
| GOV-7 | FATDUCK_ACCESS.md（根，tracked，mode 600）；docs/DOCUMENT_INDEX.yaml:42-43；.gitignore:149；tools/pack_audit_package.py:18,27 | AGENTS.md §5（凭据禁令）；DESIGN §12 L511（发布包白名单/凭据面）——凭据文件仍入仓即安全与凭据面破坏 | `git ls-files --error-unmatch FATDUCK_ACCESS.md`→rc=0（d414c3e0 仍 tracked）；`git check-ignore -v`→rc=1（tracked 豁免 ignore 条目）；索引:43 登其 `ACTIVE_INFORMATIVE`（未读文件内容） | **P0** | 接入凭据持续暴露于仓库与历史，且被活动索引登记 | ROOT-006：git rm --cached + 历史面处置（负责人）+ 删索引条目；顺带把 ROOT_FILES 中已删 CHANGELOG.md 摘除；pack 侧 DENY_PATHS 防线保留 | 仓库根 + docs/DOCUMENT_INDEX.yaml + tools/pack_audit_package.py | `git ls-files --error-unmatch FATDUCK_ACCESS.md; echo rc=$?`→非 0 | 与 GAP-028/ROOT-006 已立条目同源不删条；ROOT-006 | 无（FD-G-001 为影子树入口冲突型，不同源） |
| GOV-8 | docs/DOCUMENT_INDEX.yaml:50-51（VERSION=ACTIVE_NORMATIVE）；README.md:3；README.md:107；（历史）CHANGELOG@01db973b^:5-6 | DESIGN §12 L508-509（Alpha 前不存在任何版本信息）；ENG_SPEC §7 L110（VERSION/CHANGELOG 仅「内部助记」，且 CHANGELOG 已删仍被 §7 引用） | `sed -n '50,51p' docs/DOCUMENT_INDEX.yaml`→`- path: "VERSION"/status: ACTIVE_NORMATIVE`；`git show 01db973b^:CHANGELOG.md\|sed -n '5,6p'`→「当前产品版本节：根 `VERSION` = `0.11.0-alpha.2`（GOV-003 唯一源；生成串 `0.11.0-alpha.2+g<commit12>`…」 | P1 | 版本助记占据规范登记面并自称唯一源，超出 §7 承认形态；README 仍指已删 CHANGELOG「含当前 alpha 节」 | 索引 VERSION 降 INFORMATIVE/删除，README:3,:107 版本与悬空引用订正；程序面版本残留属 GAP-017 另轴 | docs/DOCUMENT_INDEX.yaml, README.md | `timeout 30 grep -A1 'path: "VERSION"' docs/DOCUMENT_INDEX.yaml\|grep -c ACTIVE_NORMATIVE`→0 | 与 GAP-017 同源（文档面为新增）；GOV-001/DOC-001 | V21-N-14（版本口径族） |
| GOV-9 | 工程控制/SCI-RES-01/（缺 GAP_AUDIT/ACCEPTANCE/SUMMARY 三件）；工程控制/PROJECT-GOVERNANCE-01/{DISPATCH,OPERATOR,RETENTION,ROOT_LEDGER}.md；TASK_LIST.md:3 与 :10-43；ACCEPTANCE.md | CONTROL_PACK_SPEC §4 L78-91（控制包目录唯一模板）、§7.2 L181-185（验收记录） | `ls 工程控制/SCI-RES-01/`→「00_README.md RESEARCH_PROTOCOL.md TASK_LIST.md tasks」；`grep -n 'ROOT-007' TASK_LIST.md ACCEPTANCE.md`→无行（但 tasks/ROOT-007.md 存在且已由 7940d70e 入库）；「任务数：33」vs `grep -c '^\| L'`=34 | P2 | 控制包目录偏离唯一模板；已执行任务的台账缺位使 §7 验收三层不可追 | 前台补 ROOT-007 行/验收条目并订正任务数；SCI-RES-01 若属研究包须负责人批准 CP_SPEC 增豁免类别，否则补三件 | 工程控制/PROJECT-GOVERNANCE-01/{TASK_LIST,ACCEPTANCE}.md, 工程控制/SCI-RES-01/ | `timeout 30 grep -c 'ROOT-007' 工程控制/PROJECT-GOVERNANCE-01/TASK_LIST.md`→≥1 且任务数=行数 | 归属：前台台账+GOV-001（模板条款） | 无（ROOT-007 为调度线新立） |
| GOV-10 | memory.md:36,48,81,87,112 | docs/ci/01_CHECKS.md:24 CHK-STALE-DOC（活动文档无陈旧版本号/历史状态冒充）；ENG_SPEC §2 L19（禁堆历史流水） | `grep -n 'da3c4b4a' memory.md`→「:36: 一致 = `da3c4b4a…`。」「:48: `da3c4b4a`（DOC-CONV-001 收敛基线）。」等 4 处自报过期基线 SHA | P2 | 活动记忆文档冒充当前基线，误导开工核对 | 该行删或标 history 注记（与 GOV-1 同批处置） | memory.md | `timeout 30 grep -c 'da3c4b4a' memory.md`→0 | 与 GAP-002 同源；DOC-001 | 无 |

### 活动面旧权威/废止路径残留计数（当前树，docs/** 排除 docs/archive 与 docs/standards 本体；排除巨目录）

| 检索词 | 命中 | 文件数 |
|---|---|---|
| 宪章 | 165 | 55 |
| ASTROCS_PROJECT_CONSTITUTION | 14 | 9 |
| AstroCS_ENGINEERING_CONSTRAINTS | 18 | 15 |
| 工程控制/AstroCS_（已删控制包） | 19 | 18 |
| STANDARDS_REGISTRY | 4 | 3 |

（命令：`grep -rn --exclude-dir={run,build,out,artifacts,evidence,reports,graph,worktrees,Testing,logs,third_party,设计大纲,GaiaDR3,GaiaDR3SP,.git,…} --include='*.md|*.yaml|*.json' <词> docs | grep -v '^docs/archive/\|^docs/standards/' | wc -l`；lib/tests/ci/tools 另见 GOV-3/4/5 逐点。）

## GOV-001 / DOC-001 / ROOT-002 可验收清单（任务卡逐门 + 本轮实测态）

**GOV-001（收敛最高权威、根目录与废止治理入口）**——tasks/GOV-001.md
1. 活动面旧宪章/旧约束不再作上位权威（归档处除外）→ **未达**：GOV-1/2/3/4/8 全部命中；
2. `python3 tools/check_agents_gov.py` rc=0 → **实测 rc=1**（10 关键词 MISS，GOV-5）；
3. `python3 tools/doccheck/check_engineering_constraints.py` rc=0 或删除且 checks.json 同步移除 → **实测 rc=1 且未同步**（GOV-3）;
4. `git diff --stat` 不含实现文件 →（验收时由前台核，本轴不改文件）；
5. 根目录顶层条目差集入自证、无新增未确认根条目 → ROOT-007 后根条目已变（artifacts/ 删除），须以 d414c3e0 重取差集。

**DOC-001（活动文档收敛）**——tasks/DOC-001.md
1. 无已删控制包路径引用 → **未达**：DOCUMENT_INDEX:749/753、HANDOVER.md:22、memory.md:111 等（GOV-1/2），计数见上表；
2. 无旧宪章当权威表述（复用 GOV-001 grep 门）→ **未达**（GOV-1/2/6/8）；
3. 状态词全落 §11.3 词表 → 本轴抽查 README/memory 抬头未涉状态词违规，未全量普查（归 DOC 轴）；
4. `python3 tools/doccheck/check_doc_index.py --strict` rc=0 → **实测 rc=1**（GOV-2）；
5. `git diff --stat` 未改 docs/science、docs/algorithms 内容行 → 验收时前台核；注意 docs/science/v6、docs/algorithms/v6 内有宪章引用（PSFW_FREEZE_RESEARCH.md:9,350、phase2-psfsw/README.md:24、ALG_P1_001:24 等），该两面为只读权威，订正须走文档集变更流程（DESIGN §0 L30）。

**ROOT-002（根目录长效机器门）**——tasks/ROOT-002.md
1. 检查器正例 rc=0 → **实测 rc=0**（violations=0；b46a316f 曾一次 rc=1，系并行线删 artifacts/ 瞬态，复跑绿——建议门语义把 artifacts/ 登记同步进 root_manifest F 类）；
2. 白名单与 §7 逐条对照无「比文档更宽」→ **实测符合**：allowed_files⊖§7=∅；.github 在 allowed_dirs 与 §7 原文一致；VERSION/HANDOVER/FATDUCK 不在白名单（F2/未知条目走处置登记）；
3. `python3 -m unittest discover -s tests/quality -t tests/quality` rc=0 → **实测 FAILED (failures=1, errors=4, skipped=6)**（test_docchk002_mutation 族；与 GAP-028 登记红灯同源，归 TEST-GREEN-001）；
4. 人为新建未登记根文件必红 → 本轴零写禁令未做注入实验，留前台验收时执行。

## 附：与 GAP 面核对结论
GAP-001/002/017/019（旧权威/悬空/版本/旧包路径）在本轮全部仍成立且行号已重定位；ROOT-007 删除把「文件在位违规」转为「引用悬空违规」，新增红门三枚（GOV-3）与 AGENTS-GOV 红（GOV-5）为执行期新事实。U-05（活动引用判定口径）由本轴计数表提供逐点实据。U-08 型三 SHA 漂移在收工时再现（main 领先 origin/main 1），由前台按 AGENTS.md §7 push 收敛。

完整命令与逐字输出：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/GOV.log`。
