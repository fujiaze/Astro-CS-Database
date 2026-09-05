# NEGATIVE_MATRIX.md — V8-CI-009 负向注入矩阵（13 类）

- owner: SA-CI-32 ｜ base_sha: `72df03191e1d12cc733f506dc8f85a5c6c75556b` ｜ 模式: write
- 全部注入在 tempfile 临时副本 / fixture 仓库进行；ci/checks.json、ci/run.py、workflow 零改动（`CHANGED_FILES.txt`）。
- 判定口径：**REJECTED** = 现有守卫以非零退出码/异常拒绝注入样例；**GAP** = 该维度当前无守卫覆盖（样例被放行），已如实登记并附最小补齐建议，未现写守卫、未跳过用例。
- **GAP 补齐轮（本轮）**：GAP-G1 前台裁决 **POLICY(控制面保障)**（WRITE_LEASE 校验由前台 dispatch 流程人工保障，不进 ci/run.py，hosted runner 无 lease 文件）；GAP-G2/G3 最小修复采纳落地（validate_registry R9 / validate_candidate path_escape_in_zip），登记式用例翻红为拒绝断言。
- 可机器重放注入脚本：本目录 `logs/inject_*.sh|py`（仅存放于 evidence logs，不进 ci/）。
- 用例编号对应 `ci/tests/test_negative_guards.py` 类 `TestN01..TestN13`（36 用例，unittest discover 全绿）。

| # | 负向样例 | 注入方式 | 守卫 | 退出码 / 判定信号 | 结论 |
|---|---------|---------|------|------------------|------|
| N01 | 假 PASS JSON（伪造 verdict=PASS 的结果文件喂结果消费路径；含非法 JSON / 重复 ID registry） | fixture 仓库预置伪造 `checks/N-T.json` + 空 `CI_RESULT.json` 后跑 `ci/run.py --output-root`；`checks.json` 写入 `{oops` / 重复 ID | `ci/run.py` load_registry 校验 + write_outputs 原子重算覆盖（伪造字段/空 checks 被清除） | 非法 JSON→exit 2；重复 ID→exit 2；伪造 PASS 遇真实 FAIL→exit 1、per-check verdict=FAIL | REJECTED |
| N02 | 检查脚本非零退出（exit 3）仍记 PASS | fixture registry 注入 `python3 -c "import sys;sys.exit(3)"` | `ci/run.py` 执行判定（verdict 仅由执行计算） | run.py exit 1；per-check `FAIL`；summary.fail=1 | REJECTED |
| N03 | 命令超时 / 被信号杀死仍记 PASS | fixture 注入 `time.sleep(4)`（timeout_seconds=1）与 SIGKILL 自杀命令 | `ci/run.py` 超时/信号捕获 | run.py exit 1；per-check `TIMEOUT` / `SIGNAL` | REJECTED |
| N04 | dirty workspace（工作区脏；WRITE_LEASE/base_sha 不符的写操作路径） | a) `mutates_workspace=false` 检查写 `dirty_probe.txt`；b) fixture 预置过期 `WRITE_LEASE.json`（base_sha 不符 + expires 2020） | a) `ci/run.py` git status 前后快照（detect_dirty）；b) **前台裁决 POLICY：WRITE_LEASE 为控制面状态，由前台 dispatch 流程人工保障，不进 ci/run.py**（hosted runner 上无 lease 文件；测试以锚点断言登记裁决现状） | a) run.py exit 1、per-check `FAIL(dirty)`、violations=[dirty_probe.txt]；b) run.py exit 0 放行（属 POLICY 预期行为，非守卫缺口） | a) REJECTED ／ b) **POLICY(控制面保障)** |
| N05 | 未登记脚本 / 未登记检查 ID 被执行链放行 | `ci/run.py --check N-GHOST`（registry 无此 ID）；registry `command=[python3, ci/nonexist.py]` | `run.py select_checks` 未登记 ID 校验；`validate_registry --strict` R4 repo 相对路径存在性 | exit 2（未登记 ID）；exit 1（file not found: ci/nonexist.py） | REJECTED |
| N06 | PR/fork 触发 Fatduck | `.github/workflows/fatduck.yml` 临时副本注入 `pull_request` / `workflow_dispatch` 触发器 | 触发器白名单守卫（`ci/tests/test_fatduck_workflow.py` 测试层守卫；本套件只读镜像 `_assert_fatduck_shape`，镜像对真实 fatduck.yml sanity 通过） | 镜像断言 AssertionError（CI 中为测试 FAIL；无独立进程退出码） | REJECTED |
| N07 | tag action（workflow 引用 tag 而非完整 SHA；workflow_run 分支漂移） | 临时副本把 `uses` 改 `actions/checkout@v7.0.1`；`workflow_run.branches=["dev"]`；临时 lock 文件写短 SHA | 40-hex 完整 SHA + actions.lock 成员断言；`workflow_run.branches==["main"]` 约束；`ci/verify_actions_lock.py` `LockInvalid` | 镜像 AssertionError ×2；`VAL.load_lock` 抛 LockInvalid | REJECTED |
| N08 | hardcoded core（注册表 check 命令硬编码线程 `-j4`/`--threads=64`/`--jobs=4`/`NPROC=8`/`OMP_NUM_THREADS=8` 赋值；执行链 env 直通） | 临时 registry：`command=["python3","ci/select_profile.py","-j4"|"--threads=64"|...]`（真实脚本，绕开 R4）；env 直通维度见 test_execution_chain_thread_env_policy__GAP_G2 登记裁决 | **`validate_registry --strict` R9（本轮补齐）**：command 全元素命中 `-j<N>`/`--jobs`/`--threads`/`NPROC`/`*_NUM_THREADS` → `hardcoded_core_in_command`；`ci/resource_monitor.py` 前缀与 `--` 之间 monitor 自身参数（--timeout/--output）白名单放行。env 直通 → **POLICY**（R9 已封参数通道，hosted runner 环境由控制面保障） | -j4 / --jobs=4 / --threads=64 / OMP_NUM_THREADS=8 / NPROC=8 → exit 1、errors 含 `R9 hardcoded_core_in_command`；monitor 前缀正例 → exit 0 | **REJECTED(本轮补齐，R9)** ／ env 维度 POLICY |
| N09 | heavy 检查无 `ci/resource_monitor.py` 前缀 / heavy 混入 fast | 临时 registry：`heavy=true, requires_monitor=false, profiles=[linux-deep]`；`mutates_workspace=true, profiles=[fast]` | `validate_registry --strict` R7（heavy⇒requires_monitor）、R8（mutates_workspace 禁入 fast） | exit 1（errors 含 `R7` / `R8`） | REJECTED |
| N10 | toolchain 版本漂移（不满足 policy） | 临时 strict policy（gcc-99/clang-99/cmake 99.99.99）喂 `ci/bootstrap.py --platform linux --policy`；另以主仓 policy 实测本机 | `ci/bootstrap.py` 逐项探测（CHECKERS[platform]） | exit 2；`failures=["runner","gcc-99","clang-99","cmake","Ninja"]`（--json 机读） | REJECTED |
| N11 | 上传 FITS / headers / 绝对路径 | 临时 zip：成员 `data/frame.fits`、`include/extra.h`（登记进 SHA256SUMS）→ 被拒；成员 `/abs/escape.dll`、`../escape.dll`（SHA256SUMS 完备）→ 被拒；逃逸路径仅出现在 SHA256SUMS 登记行 → 被拒 | `ci/validate_candidate.py` EXCLUDE 后缀规则；**path_escape_in_zip（本轮补齐）**：成员名与 SHA256SUMS 登记 relpath 经 PurePosixPath 判定 `is_absolute() or '..' in parts` → 拒绝 | FITS/header→exit 1 `excluded_entry_in_zip`；`/abs`、`..` 成员→exit 1 `path_escape_in_zip`；SUMS 登记逃逸路径→exit 1 `path_escape_in_zip` | a/b/c) **REJECTED**（c 本轮补齐） |
| N12 | Fatduck validate job 注入 checkout / 额外 run 步骤 / 仓库相对 ci/ 路径 | 临时副本向 `fatduck-validate` steps 插入 `actions/checkout@<locked-sha>`、追加 run 步骤、run 文本追加 `python3 ci/validate_candidate.py` | `test_fatduck_workflow` 形状守卫（无 checkout、恰一个 run 步、无 `ci/` 路径；镜像断言） | 镜像 AssertionError ×3 | REJECTED |
| N13 | Issue 写 token 落到 Fatduck（validate job / 顶层注入 `issues: write`） | 临时副本改 `jobs.fatduck-validate.permissions` 与顶层 `permissions` 为 `{contents: read, issues: write}` | permissions 矩阵守卫（仅 notify-owner 持 issues:write；镜像断言含顶层） | 镜像 AssertionError ×2 | REJECTED |

## GAP 汇总（本轮补齐后：gaps_open=0）

| GAP | 维度 | 裁决 / 补齐结果 | 落地位置 |
|-----|------|----------------|---------|
| GAP-G1 | WRITE_LEASE / base_sha 写操作路径 | **POLICY(控制面保障)**（前台裁决，不实现）：WRITE_LEASE 是控制面状态，其校验由前台 dispatch 流程人工保障，不进 ci/run.py（hosted runner 上无 lease 文件）。用例 `test_write_lease_control_plane_policy__GAP_G1` 以锚点断言登记裁决现状 | 无代码改动；登记于 `ci/tests/test_negative_guards.py` TestN04 |
| GAP-G2 | 硬编码线程 / check 命令资源参数 | **REJECTED(本轮补齐)**：validate_registry 增 R9 `hardcoded_core_in_command`——command 全元素命中 `-j<N>`/`--jobs`/`--threads`/`NPROC`/`OMP_NUM_THREADS`/`MKL_NUM_THREADS`/`NUMEXPR_NUM_THREADS` 即 strict 拒绝；`ci/resource_monitor.py` 前缀与 `--` 之间 monitor 自身参数白名单放行。真实 registry 80 条全量扫描 0 撞线。执行链 env 直通维度 → POLICY（R9 已封参数通道，hosted runner 环境由控制面保障） | `ci/validate_registry.py` R9；用例 TestN08 翻红 |
| GAP-G3 | candidate zip 成员路径逃逸（绝对路径 / `..` 段） | **REJECTED(本轮补齐)**：validate_candidate 增 `path_escape_in_zip`——zip 成员名与 SHA256SUMS 登记 relpath 经 PurePosixPath 判定 `is_absolute() or '..' in parts` → exit 1（成员判定先于一切内容判定，双通道覆盖） | `ci/validate_candidate.py` `_zip_path_escape`；用例 TestN11 翻红 |

## 覆盖统计

- 13/13 类均落为离线用例（36 个测试方法，`python3 -m unittest discover -s ci/tests -p 'test_*.py'` → 216 tests OK）。
- 12 类完整 REJECTED；N04、N08 剩余 env 维度为 POLICY 登记式（G1 POLICY、G2 env POLICY），代码面缺口 0（gaps_open=0，policy_rulings=[GAP-G1]）。
- 本轮新增 2 用例：`test_monitor_prefixed_command_allowed_by_r9`（R9 白名单正例）、`test_sums_registered_escape_path_rejected__GAP_G3_fixed`（SHA256SUMS 登记逃逸路径）。
- 原 GAP 登记式用例（断言"当前无拒绝"）已按裁决翻红为拒绝断言（G2/G3）或改写为 POLICY 锚点断言（G1/G2-env）；锚点断言在裁决变更时自动翻红提示更新本矩阵与 TASK_RESULT.json。
