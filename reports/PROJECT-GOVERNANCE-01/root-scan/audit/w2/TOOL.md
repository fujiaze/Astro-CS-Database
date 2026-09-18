# W2-TOOL · 第二波文件级审计 · tools/** 全量 checker/工具轴

## 抬头
- **轴名**：W2-TOOL（tools/** 文件域穷尽审计：tools/quality 90、tools/v6 6、monitoring 4、doccheck 3、astrometry_oracle 3、traceability 2、realdata 2、arch 2、testkit 1、graph 1、根级 tools 文件 57；含契约夹具与 README）
- **基线**：开工 2026-09-16 实测 `git rev-parse HEAD main origin/main` = `180c8a0a` / `180c8a0a` / `b4afc135`——三向不等（origin/main 滞后），等 2 分钟重试一次仍不等（同值），按任务书以 **HEAD=main 相等为准开工**，开工 SHA=`180c8a0ad9755e513670c7a1feb8fa215a87e1d7`。收工时实测 HEAD=main=origin/main=`32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（窗口内并行线推进多提交并曾重排 ci/checks.json 147→38 项中）。工作树含并行未提交改动：注册表类证据钉在开工 HEAD（`git show HEAD:ci/checks.json` 当时版），行号与判定证据按取证时刻工作树实况。**窗口内变化如实注明**（如 check_module_map 由 untracked→已提交；API-DOCS/CLI 门 fail-closed 改造与旧世代退役由并行线提交落地）。
- **方法**：`git ls-files tools/` 173 全量枚举；逐文件三查（能红性夹具/selftest、fail-open 形态、宣称面 vs 实判面、输出/判定一致性）；铸造工具按第一波（QA 轴、shards V13/M5b/M6b）同源标注；小文件批量过、可疑点真跑取证。
- **摘要（≤10 行）**
  1. 铸台账家族（P0×2 新点 + 家族）：`update_audit_status.py` 与 `v19r3_audit.py` 按 type 机械铸 VERIFIED/PASS、F05-F11 恒 True、findings_p*=0 写死，零证据锚（第一波 V13-N-03 同族，工具本体即污染源）。
  2. 注册门永绿：`tools/quality/check_traceability.py`（id=TRACEABILITY，waivable=false）末行无条件 `return 0`——实测 `rows=63 ok=37 broken=26` 仍 rc=0。
  3. `v19r3_traceability.py` 铸造 docs/TRACEABILITY.csv：63 契约硬编码白名单自称「全量」、test-ID 判据=子串存在、TEST-PR-UPM-008/009 与 005/006 重复行压行充数，且运行即覆写受跟踪权威 csv。
  4. CON-* 契约家族（11 注册门）：判定面普遍弱于宣称（umbrella 逃逸、关键字 substring、阈值计数、宣称 5 项实查 2 项）；`check_api_contracts.py` 对 status/test_ids 列零逻辑（QA-1 P0 的检查器侧同点）；49 个夹具文件全树零消费者（ESPEC §8 正反例要求未接线）。
  5. 旧权威锚定门：AGENTS-GOV 要求 V5 十要素（含 Fatduck/vm-bj 字面）实测 10/10 缺失恒红；DOC-L0 要求已删 REVIEW.md 恒红；ENG-CONSTRAINTS 以非权威约束文件在根为判据；P3-STATUS/MODULE-READMES/CON-TRACE 锚旧命令树/旧 5 模块/旧 SCI 词表。
  6. 版本族门（UT-VERSION/VERSION-NAMESPACES/gen_version）把版本串注入 CMake/CLI/文档当现行纪律维护，与 DESIGN §12「Alpha 前程序与代码不含任何版本信息」实质冲突。
  7. 空转恒绿族：TASK-RESULT-SCHEMA 实测 `checked=0` rc=0；DUPLICATION 规则1 为 `pass` 无操作+无二进制即跳；ABI-BOUNDARY 语料实为 1 个头（目录不存在）；PIPELINE-TRACE 无 stages 即跳；LINK_SCAN 无二进制即 PASS。
  8. ACR-DORMANT 门实测以未捕获 FileNotFoundError 崩溃（读已迁移头），红灯≠判定；CON-DOC-SYMBOLS 无界 `repo.rglob("*")` 实测 >60s 未完。
  9. `astro_toolkit.py`+tools/README.md 提供子 Agent 一键 git_add/commit/push 批量通道，README 自述目的「减少子 Agent 频繁触发沙箱确认」——制度性绕开 AGENTS.md §5 硬禁令。
  10. 正面样本（不立条）：check_root_cleanliness/known_failures_baseline/check_serial_heavy/check_prod_reachability/check_isa_leak/check_pipeline_graph/check_module_map --selftest 实测全 PASS；5 个 SELFTEST 门在册；6 个退役工具退役注记由并行线落地中。
- **发现计数**：P0 = 3 ｜ P1 = 7 ｜ P2 = 6（共 16 条；覆盖清单 173/173 文件 100% 判毕：OK 78 / FINDING 85 / NA 10）

## ① 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据（本轮真跑） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-TOOL-1 | tools/quality/update_audit_status.py:95-135 | ASTROCS_DESIGN §12「未实现/未验收必须明确报告，不得用文档声明冒充」；ESPEC §8 能红能绿；§11.3 状态阶梯 | 读文件 95-122：按 type 分支机械写 review_status=VERIFIED、findings_p0..3="0"、7 项 *_ok="PASS"；:44 V19_CARRY=set() 写死空集（宣称 hash 一致=carry 无任何复算）；:134 batch_summary 行写死 PASS/0/0/0 | P0 | 发布判据所读的台账 100% 自铸、零证据锚；V19R2 世代 reports/v19r2/*.csv 至今在册 | 删除或重写为「锚复算器」（哈希/test 可注册/rc 三锚），存量行改 UNREVIEWED 起审 | tools/quality/, reports/v19r2/ | `grep -c "VERIFIED" tools/quality/update_audit_status.py` 期望 0（或文件删除） | 第一波 shards V13-N-03（P0 判 OPEN，铸源即本文件）、QA-1/QA-10 铸造家族；归 CI-001+DATA-001 |
| W2-TOOL-2 | tools/quality/v19r3_audit.py:164-236 | 同上 DESIGN §12 + §11.3（VERIFIED 语义）；ESPEC §8 | 读文件：F03-F11 全部硬编码 True（:169-177），*_ok 七列写死 "PASS"（:231-234），review_status 仅按 os.path.exists 即「V19R3-FRESH-VERIFIED」（:224），头注宣称「unreviewed=0」为恒真等式（:266-267） | P0 | V19R3 FRESH 全量复核名义存在、实为存在性普查；下游审核读到的 100% verified 系铸造 | 同 W2-TOOL-1：F05-F11 必须逐项接可复算判定器，否则如实标 MANUAL/UNREVIEWED | tools/quality/ | `python3 -c "import re;s=open('tools/quality/v19r3_audit.py').read();print(len(re.findall(r'True,\s+#',s)))"` 期望 0 | 与 V13-N-03/M6b-G-001 同族（AUDIT_INDEX 跨轴共性 2「状态铸造家族」） |
| W2-TOOL-3 | tools/quality/check_traceability.py:143-159 | DESIGN §12「追踪无断链」为发布候选前提；ESPEC §8 每项检查能红能绿；docs/ci/03_GATES §1 | 命令：`timeout 60 python3 tools/quality/check_traceability.py`；输出：`traceability: rows=63 ok=37 broken=26 symbols=13/13` 且 rc=0；:159 无条件 `return 0`（broken 仅写 evidence json）；开工 HEAD 注册 id=TRACEABILITY（linux-main+windows-main，waivable=false） | P0 | 不可豁免追溯门结构上永绿，26 断链照绿；CI-001 重排文档还引用其「rc=0」为保留依据（01_CHECKS §2.2:49） | 末行改 `return 0 if not broken else 1`；补 --selftest 注入断链负例 | tools/quality/, ci/ | `python3 tools/quality/check_traceability.py; echo rc=$?` 在现存 26 broken 下期望 rc=1 | 第一波 M6b-E-001/V13-N-04（消费端不校验 status 同族）；归 CI-001 |
| W2-TOOL-4 | tools/quality/v19r3_traceability.py:9,284-302,347-353,418 | ESPEC §8「无悬空引用」；DESIGN §11.3 VERIFIED 语义；§12 | 读文件：docstring 宣称「checker 全量集合检查」而 CONTRACTS=硬编码 63 行白名单；:289-291 pr_tests[7][8] 与 [4][5] 同名重复→TEST-PR-UPM-008/009 行压行充数；:353 test-ID 判定=re.escape 子串存在于文件（注释提及即过）；:418 运行即覆写受跟踪 docs/TRACEABILITY.csv（本轮未跑，防写） | P1 | docs/TRACEABILITY.csv 的 VERIFIED 由「锚存在」弱判据铸造且含重复充数行；工具改权威跟踪文件=副作用门 | 行集去重、test-ID 改可注册用例名匹配、输出改 run/ 草稿+人工晋升、宣称改「白名单核对」 | tools/quality/, docs/ | `python3 -c "import csv;r=list(csv.reader(open('docs/TRACEABILITY.csv')));ids=[x[6:] for x in r[1:]];print(len(ids)-len(set(map(str,ids))))"` 配套重复锚=0（订正后复跑本工具 diff 为空） | V13-N-04（63/63 VERIFIED 无门校验）+ QA-10（VERIFIED 挪用）同源；归 CI-001+DATA-001 |
| W2-TOOL-5 | tools/quality/contracts/check_api_contracts.py:48-52 | ESPEC §8「检查器覆盖：核心合同有独立测试…」；DESIGN §11.3 | 读文件：required=["symbol","full_signature","header","linkage"]，全文对 status/test_ids 列零逻辑（grep 命中 0）；本轮真跑该门：status=FAIL 全为 API-BAD-HEADER 346 行（签名面反而全过）——门在红但红的不是 VERIFIED/占位 test_id 造假面 | P1 | API_CONTRACTS 382 行 VERIFIED+356 占位 TST-GEN-001 的唯一消费门不校验该列（QA-1 P0 的检查器侧实证） | 门增判：status=VERIFIED 行必须带可解析 test_id，否则 FAIL | tools/quality/contracts/, docs/contracts/ | `python3 tools/quality/contracts/check_api_contracts.py --require-status-evidence; echo rc=$?`（负例落地后自带 mutation 用例） | QA-1/M5b-C-01 直接同源（列位改判：铸造在 QA-1、本条钉工具） |
| W2-TOOL-6 | tools/quality/contracts/{check_test_contracts,check_traceability,check_science_units,check_forbidden_patterns,check_execution_contracts,check_build_graph,check_comments,check_full_integration,generate_contract_report}.py | ESPEC §8（能红能绿+覆盖口径）；01_CHECKS §1；DESIGN §4.3/§1.3 | 读文件实证：test_contracts:38「含 TST- 子串即由 synthetic_gate 伞盖放行」（宣称 TST 注册/可运行/断言对应=未实现）；traceability:59-62 核心=关键字 substring+死代码 `for kw: pass`；science_units 判据=计数阈值 ≥20/≥5；forbidden_patterns 宣称 5 项、:54-57 三项注释 skip、线程正则只咬字面 "16"；execution_contracts:60-64 生产并行轴文件缺失即 continue（ARCH-001 迁移后=空转绿）；generate_contract_report:43 解析多行 JSON 恒失败退 rc-only | P1 | 9 个注册 waivable=false 门的实判面远窄于宣称面；随 lib 迁移将成批空转恒绿 | 按宣称逐项落地或收窄宣称；输入缺失改 fail-closed；伞盖逃逸删除 | tools/quality/contracts/ | `timeout 300 python3 -B -m unittest discover -s tests/contracts -t tests/contracts` 期望 rc=0 且新增 mutation 用例入册 | 与 QA-8（CHK-ORACLE 缺实体）同域；归 CI-001 |
| W2-TOOL-7 | tools/quality/contracts/fixtures/**（34 文件）+ tools/quality/fixtures/extract_cpp_api/**（5 文件） | ESPEC §8「每项检查有正例与负例（能绿能红）」 | 命令：全树 grep（tools/ci/tests/.github）「contracts/fixtures」「fixtures/extract_cpp_api」消费者=0；夹具所在 11 个 CON-* 门均无 --fixture/--selftest 入口（本轮逐个真跑确认） | P2 | 负例数据在册但永不执行=假红面；审计者易误信「已具负例」 | 为每 CON-* 门加 --selftest 装载同名夹具（红/绿双向断言），或删夹具免误导 | tools/quality/contracts/ | `timeout 300 python3 tests/quality/test_con_selftests.py`（夹具接线用例，待建） | 归 CI-001；与第一波「145→27 CHK 迁移+负例注入」议题（CI.md §重跑前提 3）同源 |
| W2-TOOL-8 | tools/check_agents_gov.py:8-19; tools/check_l0_docs.py; tools/doccheck/check_engineering_constraints.py:1-6; tools/check_p3_status.py:24-30; tools/check_module_readmes.py:11-17; tools/check_version_consistency.py; tools/doccheck/check_version_namespaces.py; tools/gen_version.py | 权威链 ASTROCS_DESIGN §0/§6.2/§12 + ESPEC §7（AGENTS.md 为现行②号权威、根 REVIEW.md 已由 ROOT-007 删除、命令树唯一 normalize/mosaic/export、Alpha 前零版本信息）；SHARD_BRIEF §2 映射表 | 命令：`timeout 30 python3 tools/check_agents_gov.py` 输出 GOV_CHECK_FAIL missing 十要素全缺（含 "Fatduck"/"vm-bj" 字面）；`check_l0_docs.py` rc=1 "REVIEW.md missing"；`check_module_readmes.py` PASS 自报「5 模块」而仓内 lib 模块 30+；`check_p3_status.py` 判据=cli 含 "phase3" 子串（新 §6.2 树下必红） | P1 | 五个 fast/main 不可豁免门以已废止旧权威为判据：红者恒红堵 main、绿者靠旧面残留虚绿；AGENTS-GOV 还要求把 Fatduck/vm-bj 节点名写回 AGENTS.md（与 ROOT-006 凭据收紧相反方向） | 旧代门按 01_CHECKS 27 CHK-* 口径重定义或退役（并行退役线补批） | tools/, ci/ | `python3 tools/check_agents_gov.py; echo rc=$?` 与 `python3 tools/check_l0_docs.py; echo rc=$?`（订正后期望各自 rc=0 或 id 出册） | GOV 轴（三门实测红）/CI-001 退役线同源；GAP-027 同族 |
| W2-TOOL-9 | tools/check_warning_suppression.py:26-28（docstring）与构建阶段 | ESPEC §8 确定性执行器；AGENTS.md §4 最小改动面（检查器不得改工作树） | 读文件：静态干净+构建树存在即 `touch` 生产源强制重编并统计警告；本轮实测该门 25s 未返回（batch 超时 rc=TIMEOUT） | P2 | 注册门带写副作用+高时耗，并行工作树下易制造竞态假象 | 判定改纯读日志（compile_commands+已存 build 日志），禁 touch | tools/ | `timeout 60 python3 tools/check_warning_suppression.py; echo rc=$?` 期望 60s 内返回且 `git status --porcelain|wc -l` 不因运行增加 | QA-7（写副作用测试同模式） |
| W2-TOOL-10 | tools/quality/check_task_result_schema.py（默认输入 evidence/v6_1_rework/tasks 不存在）; tools/check_duplication.py:22-31; tools/check_pipeline_trace.py:33-37,60-70; tools/check_abi_boundary.py:14-19; tools/check_link_scan.py:20-24; tools/check_serial_hardcode.py:19-25 | ESPEC §8 能红能绿（空集不得伪 PASS）；DESIGN §12 | 命令：`timeout 15 python3 tools/quality/check_task_result_schema.py` 输出 `SCHEMA_CHECK_PASS checked=0` rc=0（注册 TASK-RESULT-SCHEMA waivable=false）；`check_duplication.py` rc=0 而规则1 `if p.exists(): pass` 无操作+二进制缺失跳 nm；abi 门语料=1 文件+lib/backend_host（实测不存在）；link_scan 无二进制=PASS | P1 | 一批 fast/main 门在输入/语料缺失时空转恒绿；随 ARCH 迁移扩大 | 空输入=FAIL（fail-closed），语料改 git ls-files 派生并加「命中数≥N 否则红」守卫 | tools/ | `python3 tools/quality/check_task_result_schema.py; echo rc=$?` 期望缺目录时 rc≠0（订正后） | ARCH 轴「ABI 门语料空转」+ QA-4 GAP-027 fail-closed 传导未及工具面（同源不删条） |
| W2-TOOL-11 | tools/check_legacy_exit.py:46 | ESPEC §8 确定性执行器；DESIGN §1.3 ACR DORMANT（判据对象仍应为活动面） | 命令：`timeout 40 python3 tools/check_legacy_exit.py`；输出：FileNotFoundError: lib/astro_image_io/include/aio_pipeline_engine.h rc=1——注册门 ACR-DORMANT（fast+双 main，waivable=false）当前以崩溃变红，非判定红；且二进制存在才查符号（isfile 无 else） | P1 | ACR 休眠门失去判定面；崩溃红会被误读为「门有效」 | 缺失输入 fail-closed 并给显式诊断；符号面改 compile_commands/安装清单驱动 | tools/ | `python3 tools/check_legacy_exit.py --selftest; echo rc=$?` 期望 rc=0（新建自证） | 与 QA-2（门红挂 main）同形异源；归 CI-001+ARCH-001 |
| W2-TOOL-12 | tools/quality/check_commits_csv.py:178 | ESPEC §8 能红能绿（自证负例） | 命令：`timeout 60 python3 tools/quality/check_commits_csv.py --selftest`；输出：`rows[0]["parent_commit"]="0"*40 IndexError: list index out of range` rc=1——自测本身崩溃 | P2 | 提交链验证器的 tamper 负例永不生效（GOV 台账可造假的机器守护缺自证） | 修 selftest 行装载逻辑；入 CI 册（现未注册） | tools/quality/ | `python3 tools/quality/check_commits_csv.py --selftest; echo rc=$?` 期望 rc=0 | GOV 轴治理门同域；归 CI-001 |
| W2-TOOL-13 | tools/astro_toolkit.py:138-167,417-419; tools/README.md 开头 | AGENTS.md §5/§7 与 ESPEC §6「SubAgent 零 git 写权限；前台串行提交」硬禁令 | 读文件：step_git_add/git_commit/git_push 三实现 + README 自述「Python 脚本+JSON 配置驱动…用于减少子 Agent 频繁触发沙箱确认」；示例配置含 add/commit/push 全链 | P1 | 制度性绕开沙箱与提交纪律的批量通道在册可用；README 即使用说明 | 删除 git_* 步骤类型（保留 run/log 只读类），README 同步；如需批量提交，只暴露给前台 | tools/ | `grep -c "git_push" tools/astro_toolkit.py tools/README.md` 期望 0 0 | 归 GOV-001/CI-001；凭据/纪律面与 ROOT-006 收紧方向相逆 |
| W2-TOOL-14 | tools/realdata/README.md:4 | SHARD_BRIEF §2（旧控制包路径工程控制/AstroCS_* 已删→CONTROL_PACK_SPEC §4 唯一模板）；ESPEC §8 无悬空引用 | 命令：`ls 工程控制/`；输出：仅 PROJECT-GOVERNANCE-01、SCI-RES-01；README 引用 `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/04_OWNER_DECISIONS_20260910.md` 不存在 | P2 | 域文档悬空引用（REAL-001 验收前置文档指向已废止控制包） | 引用改现行权威落位或标注已废止+git 可取回路径 | tools/realdata/ | `timeout 30 grep -c "AstroCS_CONSTITUTION_ALIGNMENT" tools/realdata/README.md` 期望 0 | ROOT-007/GOV 轴悬空引用 165+ 同族（本域命中 1 处） |
| W2-TOOL-15 | tools/make_linux_release.py:3-14; tools/make_windows_release.py:3-20; tools/check_final_traceability.py:26-42; tools/check_release_layout.py:12-14; tools/check_release_consistency.py:22-31; 同族 build_v19r2/r3/r4、assemble_v17、gen_v19_* 共 11 文件 | DESIGN §10.1（唯一 exe `ACSD Cli.exe`/`acsd_cli`+schemas+manifest 安装树）与 §12/ESPEC §7（Alpha 前程序与产物零版本信息；VERSION 仅内部助记） | 读文件：两 make_*_release 打包结构含 `VERSION (0.10.0-alpha.2+g<commit>)`/SBOM 版本名注入产物树；check_final_traceability 判据 `ver != "0.10.0-alpha.2"` 即 FAIL（写死旧版号+print 里写死 "66 claims"）；release_layout 以 dist/astrocs-alpha 旧产品形态为必含 | P2 | 旧世代发布/版本工具群未入退役线（对照 make_capsule 等 4 件已有 RETIRED 注记 WIP）；一旦被用于候选包即产出自相矛盾发布证据 | 纳入 CI-001 退役批（fail-closed 退役注记+exit 2），发布面统一改 CHK-PACKAGE 新形态 | tools/ | `timeout 30 grep -l "0.10.0-alpha" tools/*.py tools/quality/*.py` 期望无发布判定类命中 | PKG 轴（CHK-PACKAGE 空转 P0）同源工具侧；归 PKG-001+CI-001 |
| W2-TOOL-16 | tools/quality/contracts/check_doc_symbols.py:53 | ESPEC §8（检查器确定性/可运行性）；03_GATES（fast 面时耗约束精神） | 命令：`timeout 60 python3 tools/quality/contracts/check_doc_symbols.py --repo .`；输出：60s 超时未完成（known_files=set(全仓 rglob("*")) 含 build/run/数据树），门 CON-DOC-SYMBOLS 注册 fast+双 main 且 timeout=None | P2 | 快面层门可拖挂 CI；与 DEEP-* 无超时注册叠加放大 | known_files 改 git ls-files 集合；限域 walk 排除产物目录 | tools/quality/contracts/ | `timeout 60 python3 tools/quality/contracts/check_doc_symbols.py --repo .; echo rc=$?` 期望 60s 内 rc=0/1 | CI-001 门重排线在途（WT 已出册，收口须回归 fast 时耗预算） |

**未立条但如实记录**（宁缺毋滥）：① CHK-MODULE-MANIFEST 开工时注册在 HEAD 而 checker=UNTRACKED（clean-clone 必红），收工前已由并行线提交（实测 `git cat-file -e HEAD:tools/quality/check_module_map.py` OK + selftest 8 例 PASS），窗口内自愈不立条；② API-DOCS 恒绿（GAP-027）与 CLI 命令面旧判据（CLI-RUN-PRESET/CLI-COMMAND-LAYER）在本轴窗口内由 CI-001 fail-closed 化并提交，实测 `check_cli_command_layer.py --self-test` rc=0、CLI-RUN-PRESET 新版判据 rc=0——第一波相关条目现势应转 RESOLVED 方向，交主控复核；③ TRACE-MISSING 类红（CON-TRACE rc=1、TEST-BAD-FILE 30、API-BAD-HEADER 346）为数据漂移红灯，门的红性成立。

## ② 覆盖清单

tools/README.md	FINDING:W2-TOOL-13
tools/api_doc_consistency.py	OK
tools/arch/build_production_execution_inventory.py	NA:lib-module
tools/arch/check_thread_budget.py	OK
tools/assemble_audit.py	NA:duplicated-by-wip
tools/assemble_v17_review_pkg.py	FINDING:W2-TOOL-15
tools/astro_toolkit.py	FINDING:W2-TOOL-13
tools/astrocs_diagnose.py	OK
tools/astrometry_oracle/compare_astrometry.py	OK
tools/astrometry_oracle/gen_synthetic_from_axy.py	OK
tools/astrometry_oracle/make_astrocs_ref.py	OK
tools/check_abi_boundary.py	FINDING:W2-TOOL-10
tools/check_agents_gov.py	FINDING:W2-TOOL-8
tools/check_aio_ownership.py	OK
tools/check_api_docs.py	OK
tools/check_ast_api.py	OK
tools/check_baseline_opcodes.py	OK
tools/check_cli_command_layer.py	OK
tools/check_cli_run_preset.py	OK
tools/check_contract_graph.py	OK
tools/check_data_artifacts.py	OK
tools/check_duplication.py	FINDING:W2-TOOL-10
tools/check_final_traceability.py	FINDING:W2-TOOL-15
tools/check_glossary.py	OK
tools/check_l0_docs.py	FINDING:W2-TOOL-8
tools/check_legacy_exit.py	FINDING:W2-TOOL-11
tools/check_link_scan.py	FINDING:W2-TOOL-10
tools/check_module_readmes.py	FINDING:W2-TOOL-8
tools/check_p1_symbol_map.py	OK
tools/check_p2_symbol_map.py	OK
tools/check_p3_status.py	FINDING:W2-TOOL-8
tools/check_pipeline_trace.py	FINDING:W2-TOOL-10
tools/check_release_consistency.py	FINDING:W2-TOOL-15
tools/check_release_layout.py	FINDING:W2-TOOL-15
tools/check_reproducible_build.py	OK
tools/check_serial_hardcode.py	FINDING:W2-TOOL-10
tools/check_traceability.py	OK
tools/check_unit_closure.py	OK
tools/check_version_consistency.py	FINDING:W2-TOOL-8
tools/check_warning_suppression.py	FINDING:W2-TOOL-9
tools/config_consistency_check.py	OK
tools/doccheck/check_doc_index.py	OK
tools/doccheck/check_engineering_constraints.py	FINDING:W2-TOOL-8
tools/doccheck/check_version_namespaces.py	FINDING:W2-TOOL-8
tools/docs_machine_consistency.py	OK
tools/gen_audit_pack.py	NA:lib-module
tools/gen_backends_manifest.py	OK
tools/gen_cfitsio_list.py	NA:lib-module
tools/gen_provider_manifests.py	OK
tools/gen_repo_source_manifest.py	OK
tools/gen_v19_evidence.py	FINDING:W2-TOOL-15
tools/gen_v19_source_snapshot.py	FINDING:W2-TOOL-15
tools/gen_version.py	FINDING:W2-TOOL-8
tools/gen_visual_views.py	NA:lib-module
tools/graph/render_run_graph.py	OK
tools/make_capsule.py	NA:duplicated-by-wip
tools/make_linux_release.py	FINDING:W2-TOOL-15
tools/make_rev2_capsule.py	NA:duplicated-by-wip
tools/make_windows_release.py	FINDING:W2-TOOL-15
tools/migrate_stage2_config.py	OK
tools/monitoring/check_log_contract.py	OK
tools/monitoring/resource_probe.py	NA:lib-module
tools/monitoring/run_monitored.py	OK
tools/monitoring/verify_monitor_csv.py	OK
tools/no_legacy_production_reference.py	OK
tools/pack_audit_package.py	NA:duplicated-by-wip
tools/phase1_e2e_bench.py	OK
tools/quality/build_v19r2_package.py	FINDING:W2-TOOL-15
tools/quality/build_v19r3_package.py	FINDING:W2-TOOL-15
tools/quality/build_v19r4_package.py	FINDING:W2-TOOL-15
tools/quality/check_comment_hygiene.py	OK
tools/quality/check_commits_csv.py	FINDING:W2-TOOL-12
tools/quality/check_complexity.py	OK
tools/quality/check_ctest_registration.py	OK
tools/quality/check_isa_leak.py	OK
tools/quality/check_module_map.py	OK
tools/quality/check_pipeline_graph.py	OK
tools/quality/check_prod_reachability.py	OK
tools/quality/check_root_cleanliness.py	OK
tools/quality/check_secret_hygiene.py	OK
tools/quality/check_serial_heavy.py	OK
tools/quality/check_source_index_v61.py	OK
tools/quality/check_source_inventory.py	OK
tools/quality/check_task_result_schema.py	FINDING:W2-TOOL-10
tools/quality/check_traceability.py	FINDING:W2-TOOL-3
tools/quality/ci_coverage_runner.py	OK
tools/quality/ci_windows_driver.py	OK
tools/quality/compare_products.py	OK
tools/quality/contracts/check_api_contracts.py	FINDING:W2-TOOL-5
tools/quality/contracts/check_build_graph.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_comments.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_config_contracts.py	OK
tools/quality/contracts/check_doc_symbols.py	FINDING:W2-TOOL-16
tools/quality/contracts/check_execution_contracts.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_forbidden_patterns.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_full_integration.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_science_units.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_test_contracts.py	FINDING:W2-TOOL-6
tools/quality/contracts/check_traceability.py	FINDING:W2-TOOL-6
tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_api_contracts/invalid_sig_mismatch.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_api_contracts/valid_min.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_build_graph/invalid_missing_define.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_build_graph/invalid_missing_source.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_build_graph/valid_min.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_comments/invalid_stale_claim.cpp	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_comments/invalid_wrong_thread.cpp	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_comments/valid_min.cpp	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_config_contracts/invalid_default_mismatch.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_config_contracts/invalid_enum_mismatch.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_config_contracts/valid_min.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_doc_symbols/invalid_archive_symbol.md	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_doc_symbols/invalid_missing_symbol.md	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_doc_symbols/valid_min.md	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_execution_contracts/invalid_claim_without_runtime.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_execution_contracts/invalid_missing_define.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_execution_contracts/valid_min.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_forbidden_patterns/invalid_abs_path.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_forbidden_patterns/invalid_hardcoded_threads.cpp	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_forbidden_patterns/valid_min.cpp	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_full_integration/invalid_waivers.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_full_integration/valid_min.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_science_units/invalid_precision_conflict.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_science_units/invalid_unit_mismatch.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_science_units/valid_min.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_test_contracts/invalid_missing_assertion.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_test_contracts/invalid_unregistered.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_test_contracts/valid_min.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_traceability/invalid_dup_id.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_traceability/invalid_missing_doc.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/check_traceability/valid_min.csv	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/generate_report/invalid_stability.json	FINDING:W2-TOOL-7
tools/quality/contracts/fixtures/generate_report/valid_input.json	FINDING:W2-TOOL-7
tools/quality/contracts/generate_contract_report.py	FINDING:W2-TOOL-6
tools/quality/deep_ci_driver.py	NA:duplicated-by-wip
tools/quality/extract_cpp_api.py	OK
tools/quality/fixtures/extract_cpp_api/invalid_missing_export.hpp	FINDING:W2-TOOL-7
tools/quality/fixtures/extract_cpp_api/invalid_sig_mismatch.hpp	FINDING:W2-TOOL-7
tools/quality/fixtures/extract_cpp_api/lib/test/include/invalid_sig_mismatch.hpp	FINDING:W2-TOOL-7
tools/quality/fixtures/extract_cpp_api/lib/test/include/valid_min.hpp	FINDING:W2-TOOL-7
tools/quality/fixtures/extract_cpp_api/valid_min.hpp	FINDING:W2-TOOL-7
tools/quality/fixtures/module_map_fixture.py	OK
tools/quality/frame_qc_grid.py	OK
tools/quality/gen_commits_csv.py	OK
tools/quality/gen_module_readmes.py	OK
tools/quality/gen_run_graphs.py	OK
tools/quality/gen_source_index_v61.py	OK
tools/quality/known_failures_baseline.py	OK
tools/quality/resource_monitor.py	OK
tools/quality/strip_version_comments.py	OK
tools/quality/tsan/cfitsio.supp	OK
tools/quality/update_audit_status.py	FINDING:W2-TOOL-1
tools/quality/v19r3_audit.py	FINDING:W2-TOOL-2
tools/quality/v19r3_sanitizer.sh	OK
tools/quality/v19r3_static.py	OK
tools/quality/v19r3_strip_comments.py	OK
tools/quality/v19r3_traceability.py	FINDING:W2-TOOL-4
tools/quality/v19r4_strip_comments.py	OK
tools/quality/validate_task_ledger.py	OK
tools/realdata/README.md	FINDING:W2-TOOL-14
tools/realdata/match_plan.py	OK
tools/redteam_v19.py	OK
tools/science_contract_lint.py	OK
tools/testkit/check_testkit.py	OK
tools/traceability/check_traceability_matrix.py	OK
tools/traceability/gen_traceability_csv.py	OK
tools/v6/check_v6_runtime_closure.py	OK
tools/v6/v6_cli_mode_matrix.py	OK
tools/v6/v6_ctest_driver.py	OK
tools/v6/v6_determinism_driver.py	OK
tools/v6/v6_runtime_mutation_driver.py	OK
tools/v6/v6_runtime_oracle.py	OK
tools/validate_cpu_profile.py	OK

### 统计
files_total = 173
verdict_counts = OK:78 / FINDING:85 (W2-TOOL-1..16) / NA:duplicated-by-wip:5 / NA:lib-module:5
枚举命令 = cd "/workspace/Astro CS Database" && git ls-files tools/ (173, 收工 HEAD=32a5f5f3) ; 开工枚举同命令=171 tracked + git status --porcelain tools/ 补 2 UNTRACKED(check_module_map.py, fixtures/module_map_fixture.py→收工时均已入库)

*合规声明：零修复零 git 写；除本报告与 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_TOOL.log 外未写任何仓库路径；未读取/打印/复制 FATDUCK_ACCESS.md；命令全部带 timeout。*
