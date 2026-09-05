# ci/INVENTORY_REPORT.md — V8-CI-001 真实检查资产盘点报告

- 任务：V8-CI-001（owner=SA-CI-32，mode=write）
- base SHA：`b4f923ccbcde51e1a9112aba04815351169091c7`（HEAD == base，全程未 commit）
- 完成时间：2026-09-05T12:45Z（UTC）
- 产物：`ci/checks.json`（70 项）、`ci/impact_map.json`、`ci/validate_registry.py`、`tests/test_index.csv`、本报告
- 验收命令：`python3 ci/validate_registry.py --registry ci/checks.json --strict` → **exit 0（PASS, 70 checks, 0 errors）**
- 负向样例：6 类缺陷（重复 id / 不存在命令 / heavy 无 monitor / mutates 入 fast / 非法 profile / 缺字段）全部 exit 1 拦截，证据在 `evidence/v8_1_ci_control/tasks/V8-CI-001/logs/`（validate_positive.log、/tmp 样例结果复制于 logs/validate_negative_*.log）

## 1. 盘点方法

1. `find tools -name "check_*.py" / verify_*.py` 全量枚举 + `ci/`、`tests/`、CMake/CTest 配置扫描；
2. 静态副作用分析（open(w)/write_text/makedirs/subprocess）划分「可只读实测」与「静态登记」；
3. 全部外部命令经 `timeout 30`（重单测 120s）驱动执行，逐条记录 exit code / 时长 / git status 前后签名到 `commands.jsonl` + `logs/`；
4. 本机工具链（ADOPT-004 锁存）：python3 3.13.5、git 2.47.3、zstd 1.5.7 可用；**cmake / gcc / g++ / clang / clang++ / ninja / make / ccache / pytest 全部缺失**（ci/toolchain.lock.json missing_tools）；另缺 python 库 numpy（tests/io 依赖）。
5. 3 次实测写副作用事故（详见 §6）当场用 `git show HEAD:` 内容回写恢复，工作区已回到 clean（仅剩本任务允许路径与既有 untracked）。

## 2. 分类汇总（ci/checks.json 共 70 项）

| 类别 | 数量 | 代表项（当前实测状态） |
|---|---|---|
| 文档/索引/治理类 | 9 | DOC-INDEX(FAIL:1 文档未入索引)、VERSION-NAMESPACES(PASS)、ENG-CONSTRAINTS(FAIL:冻结文件 SHA 滞后)、DOC-L0(FAIL)、GLOSSARY-DOCS(PASS)、MODULE-READMES(PASS)、AGENTS-GOV(FAIL:8 标记缺失)、VERSION-CONSISTENCY(PASS)、API-DOCS(PASS) |
| 契约套件（tools/quality/contracts） | 11 | 9 PASS；CON-DOC-SYMBOLS(FAIL:7 项 DOC-BAD-SYMBOL/FILE)、CON-FULL-INTEGRATION(FAIL:传导) |
| 静态源码/架构/图类 | 20 | ACR-DORMANT(PASS)、NO-SERIAL-HEAVY(FAIL:resource gate 无生产调用方)、SERIAL-HARDCODE(FAIL:workers=1 硬编码)、THREAD-BUDGET(FAIL:注释行误报)、PRODUCTION-GRAPH(selftest PASS)、ISA-LEAK-SELFTEST/SERIAL-HEAVY-SELFTEST/PROD-REACH-SELFTEST(PASS)、ABI-BOUNDARY/AIO-OWNERSHIP/DUPLICATION/UNIT-CLOSURE/WARNING-SUPPRESSION/PIPELINE-TRACE/DATA-ARTIFACTS/CONTRACT-GRAPH(PASS)、CLI-COMMAND-LAYER/CLI-RUN-PRESET/P3-STATUS(FAIL)、AST-API(ENV_FAIL:缺 clang) |
| 追溯类 | 4 | TRACEABILITY(quality 版，PASS，写已跟踪 evidence JSON→mutates)、TRACEABILITY-CODE(PASS)、TASK-RESULT-SCHEMA(FAIL:1 个 V6.1 status 非法)、TRACEABILITY-MATRIX(PASS) |
| 单测类（UT-*） | 20 | 16 个 unittest 目录 + 4 个 CPU 编译驱动；实测 554 用例（详见 §4） |
| 监控/资源/CI 元检查 | 6 | LOG-CONTRACT-SELFCHECK(PASS)、KNOWN-FAILURES-BASELINE(PASS,findings=40/reproduced=22)、TOOLCHAIN-VERIFY(PASS)、WORKSPACE-ADOPTION(FAIL:REMOTE_RELATION SHA 在途漂移,waivable)、RECONCILE-STATE(FAIL:state 195 vs 台账 191,waivable)、TESTKIT-LIST(PASS,waivable) |

profile 覆盖：fast=57、linux-main=70、windows-main=58、linux-deep/fatduck=0（heavy 执行器未建，见 §5）；platform：any=64、linux=6；mutates_workspace=true=2（TRACEABILITY、UT-ARCH，均未入 fast）；heavy=1（UT-QUALITY，requires_monitor=true）；waivable=3。

**禁止编造声明**：70 项的 command[0] 全部为 python3，command[1]（或 -s 目录）全部经 `os.path.isfile/isdir` 校验存在；本机未实际执行过的命令（CPU 驱动 4 项、部分 hosted 形态）在 notes/报告如实标注 ENV_FAIL-prerequisite 或「未实测」，无任何 PASS 标注。

## 3. missing prerequisites（本机工具链缺失 → 受影响项）

| 缺失项 | 受影响（登记为 hosted / ENV_FAIL-prerequisite） |
|---|---|
| cmake/ninja/make | tests/unit CTest（56 add_test）、UT-QUALITY（make_linux_release 打包） |
| gcc/g++ | UT-BACKEND(29 errors)、UT-ABI(9 errors)、UT-CLI(setUpClass 需构建产物)、tests/cpu×4 驱动、PROD-REACH/ISA-LEAK 完整形态 |
| clang | AST-API（tools/check_ast_api.py） |
| pytest | 无注册项依赖（全部 unittest 跑法）；hosted CI 可用 |
| numpy（python 库） | UT-IO（3 errors） |

## 4. tests/ 单测盘点（tests/test_index.csv 全目录覆盖，22 行）

- 实测总计 **554 个 unittest 用例**（根级可导入 11 目录 387 + 缺 `__init__.py` 目录式补测 4 目录 167；u05=tests/quality 超时无计数）；
- 纯 PASS 目录：glossary(5)、monitoring(60)、pipeline(6)、runtime(70,skip9)、sciencelint(6)、traceability(9)、artifact(137)、contracts(9)；
- 真实内容 FAIL：arch(2)、version(2)、cli(1)；
- ENV_FAIL-prerequisite：backend(29=g++)、abi(9=gcc)、io(3=numpy)、cpu×4(未实测)、unit CTest(56,未实测)、quality(30s 超时,heavy)；
- 结构性发现：**tests/{abi,artifact,contracts,io,quality} 缺 `__init__.py`** → 根级 `unittest discover -s tests/X -t .` 报 ImportError，注册表命令统一采用目录式 `discover -s tests/X -t tests/X`（本机已验证）。

## 5. seed（ci/checks.seed.json）与实测差异表

| seed 项 | 差异 | 处置 |
|---|---|---|
| VERSION-CONSISTENCY | seed 命令缺必填 `--expected`（argparse error exit 2）；outputs 声明的 artifacts/checks/version.json 无出处 | 已对齐：`--expected 0.11.0-alpha.2`，outputs=[]（stdout JSON） |
| TRACEABILITY | `--strict` 参数不存在；脚本重写已跟踪 reports/v19r2/evidence/quality/traceability_check.json | 已对齐：去 --strict，mutates=true，移出 fast |
| PRODUCTION-GRAPH | `--strict` 不存在；完整形态需 --ir/--module-index/--trace，但 graph/l0_graph.json 等输入未入库 | 登记 selftest 形态；完整形态待 graph 产物入库后切换 |
| NO-SERIAL-HEAVY | `--strict` 不存在 | 已对齐：无参形态（当前 FAIL=真实发现） |
| ACR-DORMANT | `--forbid acr` 不存在；check_prod_reachability 需构建二进制 | 已对齐：以 tools/check_legacy_exit.py 为 ACR dormant 门（PASS） |
| SYNTHETIC-SCIENCE | `ci/run_synthetic.py` **不存在** | 未登记；留待执行器创建任务（V8-CI-002 后）按本 schema 入册 |
| LINUX-BUILD-TEST | `ci/build_test.py` **不存在**；`--preset linux-ci-gcc` 不在 CMakePresets（仅 base-msvc/win-msvc-17.14.39-x64/linux-control） | 未登记；hosted 执行器任务负责 |
| LINUX-DEEP | `ci/run_deep.py` **不存在** | 未登记 |
| WINDOWS-BUILD-PACKAGE | `ci/build_test.py` **不存在** | 未登记；windows-main hosted |
| FATDUCK-PUBLISH-POLICY | 命令指向 runner 本机 `D:\AstroCSRunner\harness\verify-publish.exe`，非仓库入口 | 未登记；fatduck runner 部署任务负责 |

补充发现：`.github/` 目录不存在（无任何 workflow）——V8-CI-003+ 创建；`ci/run.py` 统一执行器属 V8-CI-002。

## 6. 实测副作用事故（已全部恢复，工作区 clean）

| # | 脚本 | 行为 | 恢复 |
|---|---|---|---|
| 1 | tools/quality/known_failures_baseline.py | 默认覆盖写已跟踪 evidence/v6_1_rework/tasks/R0-004/KNOWN_FAILURES_BASELINE.json | git show HEAD 回写；registry 强制 `--output artifacts/...` |
| 2 | tools/docs_machine_consistency.py | 无视 --help 无条件覆盖写 reports/v19r3/evidence/quality/docs_consistency.json | 同上；未入 registry（登记 mutates，待修复 CLI 后入册） |
| 3 | tests/arch/test_inventory.py | unittest 中经生成器重写 docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv 且幂等性 FAIL | 同上；UT-ARCH 登记 mutates=true、不入 fast |

## 7. 当前 FAIL 清单（known_failures.json 候选，供 V8-CI-002 基线决策）

涉及**永不豁免**类的：SERIAL-HARDCODE（workers=1 硬编码，QA-002）、NO-SERIAL-HEAVY（resource gate 无生产调用方）、TRACEABILITY/TASK-RESULT-SCHEMA/VERSION 相关仅结构性 FAIL（TASK_RESULT status 非法 1 项、冻结文件 SHA 滞后属负责人修订在途）。
非豁免类：DOC-INDEX（CPU_003 未入索引）、CON-DOC-SYMBOLS（7 项符号/文件漂移）、AGENTS-GOV（8 标记）、DOC-L0（docs/review 缺失）、P3-STATUS（cli 无 phase3 注册）、THREAD-BUDGET（注释行误报——检查器缺陷而非工作区缺陷）、UT-VERSION（旧检查器期望过期）、UT-ARCH（幂等性）、CLI-COMMAND-LAYER/CLI-RUN-PRESET（V7 期检查器现状）、WORKSPACE-ADOPTION/RECONCILE-STATE（V8.1 接管在途漂移，waivable）。
