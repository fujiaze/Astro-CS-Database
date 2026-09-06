# V8-CI-010 双平台实测（观察）报告 — HOSTED_RUN_REPORT

- 任务：V8-CI-010（owner SA-CI-32，mode=read，只读观察，未改任何源码/控制包/派发单）
- 观察对象：SHA `25259880615bcde34c4a6dc9377d072528fc7fba`（修复提交，已确认为远端 main HEAD）的 ci-linux.yml / ci-windows.yml push runs
- 前次失败（gitlink checkout fatal，runs 34000501601/34000501567）的修复**已验证生效**：两平台 `Checkout exact SHA` 均 success
- **总体结论：RUNS_FAILURE_OBSERVED —— 两 run completed/failure，新阻塞点为 Bootstrap locked toolchain 门禁**

## 1. 两 run 状态与关键数字

| 项 | ci-linux run 34000648632 | ci-windows run 34000648616 |
|---|---|---|
| event / head_branch | push / main | push / main |
| head_sha | 25259880… | 25259880… |
| status / conclusion | completed / **failure** | completed / **failure** |
| 起跑 (run_started_at) | 2026-09-06T00:11:45Z | 2026-09-06T00:11:45Z |
| 时长 | ≈10 s（00:11:45→00:11:55，bootstrap 即败） | ≈19 s（00:11:45→00:12:04，bootstrap 即败） |
| Runner | GitHub Actions 1000000551 | GitHub Actions 1000000552 |
| 镜像 | ubuntu-24.04（20260831.293.1） | windows-2022（20260830.290.1） |
| Checkout exact SHA | **success**（gitlink 修复验证点） | **success**（gitlink 修复验证点） |
| Bootstrap locked toolchain | **failure**（exit 2，结构化 FAIL） | **failure**（exit 1，结构化 FAIL） |
| 检查执行 | Select profile / Run registered checks **SKIPPED** | Run MSVC tests / Validate candidate / Upload candidate **SKIPPED** |
| 实际执行 check 数 | **0 / 71（NOT_EXECUTED）** | **0 / 61（NOT_EXECUTED）** |
| artifact | 无（Upload small evidence failure：`artifacts/ci-public/` 不存在） | API total_count=0（Upload public CI evidence failure；candidate 未产生） |

失败步骤日志原文（结构化，无 traceback）：

```json
// linux 34000648632, exit 2
{"verdict": "FAIL", "platform": "linux", "failed_tools": [
  {"tool": "clang-19", "required": "clang-19", "observed": {"present": false, "version_line": null},
   "repair": "clang-19 不在 PATH 或版本不可解析"},
  {"tool": "cmake", "required": ">=3.31.12", "observed": {"present": true, "version_line": "cmake version 3.31.6"},
   "repair": "在 GitHub hosted runner 上复验或补齐该工具"}]}

// windows 34000648616, exit 1（pwsh 层把 bootstrap 的 exit 2 报为 step exit 1）
{"verdict": "FAIL", "platform": "windows", "failed_tools": [
  {"tool": "cmake", "required": ">=3.31.12", "observed": {"present": true, "version_line": "cmake version 3.31.6"},
   "repair": "在 GitHub hosted runner 上复验或补齐该工具"}]}
```

## 2. bootstrap 实际观测工具链版本（hosted）

| 工具 | policy 要求 | linux 实测（ubuntu-24.04, 20260831.293.1） | windows 实测（windows-2022, 20260830.290.1） |
|---|---|---|---|
| runner 镜像 | ubuntu-24.04 / windows-2022 | OK（ImageOS=ubuntu24） | OK（ImageOS=win22） |
| 架构 | x86_64 / AMD64 | x86_64 OK | AMD64 OK |
| gcc-14（primary） | gcc-14 | **通过**（不在 failed_tools；bootstrap 只打印失败项） | — |
| clang-19（secondary） | clang-19 | **FAIL：present=false，不在 PATH** | — |
| VS / toolset | VS 17.x / v143 | — | **通过**（vswhere 命中，不在 failed_tools） |
| cmake | >=3.31.12 | **FAIL：实测 3.31.6** | **FAIL：实测 3.31.6** |
| Ninja | present | 通过（不在 failed_tools） | — |

说明：`ci/bootstrap.py` 的结构化输出只列举 failed_tools；通过项仅可由"不在失败清单"推断（表内已标注）。policy 锁定值 3.31.12 与两平台 hosted 镜像实际 3.31.6 失配是本轮唯一双平台共性失败；clang-19 缺失为 linux 附加失败。

## 3. check 数量基线核验（plan-only 复算 @ 25259880）

| profile | 派发单基线 | 本地只读复算 `ci/run.py --plan-only` | hosted 实跑 |
|---|---|---|---|
| linux-main | 71 | **71**（logs/plan_linux-main_local_25259880.json） | 0（步骤 SKIPPED，未执行） |
| windows-main | 61 | **61**（logs/plan_windows-main_local_25259880.json） | 0（步骤 SKIPPED，未执行） |

registry 总数 80。基线数字与派发单一致；hosted 侧因 bootstrap 前置门禁失败，71/61 项无一执行，PASS/SKIP 分布无从产生。

## 4. candidate 深检

**NOT_EXECUTABLE**：Windows run 未产生 artifact `astrocs-windows-candidate-25259880…`（artifacts API `total_count=0`，按精确名过滤查询亦 0，见 `logs/candidate_artifact_query.json`）。`/tmp/dsh-candidate/` 为空，无下载对象。逐项结论：

- SHA256SUMS 与 zip 实体逐项相符 —— 无法执行（无 artifact）
- `validate_candidate.py <zip> --json` verdict PASS —— 无法执行（无 zip）
- `BUILD_PROVENANCE.json source_sha==25259880…` —— 无法执行（无 artifact）
- zip 内无源码/测试数据/FITS 成员 —— 无法执行（无 artifact）

## 5. resource summary

**NOT_PRESENT**：两 run 日志全文检索 `resource_monitor` / `peak` / `summary` 零命中（检查从未执行）。`ci/resource_monitor.py` 的峰值内存、CPU 利用率、超时计数三个数字本轮缺失，已在 TASK_RESULT.json 登记，待修复重跑后由下一观察轮补录。

## 6. FINDINGS（归属 + 根因假设 + 建议修复 owner；未自行改源码）

| # | 范围 | 归属检查/步骤 | 严重度 | 症状 | 根因假设 | 建议修复 owner |
|---|---|---|---|---|---|---|
| F1 | hosted linux+windows | `Bootstrap locked toolchain`（`linux_hosted.cmake` / `windows_hosted.cmake`） | blocker | policy 要求 cmake>=3.31.12，两平台镜像实测 3.31.6 | `ci/toolchain.policy.json` hosted 节 cmake=3.31.12 与当前 GitHub 镜像批次失配（疑按本地控制节点版本记录）；`bootstrap.py _CMAKE_MIN=(3,31,12)` 与 policy 联动，hosted 节无浮动 | **前台**：方案 A = hosted cmake 下限降为 project_minimum（如 >=3.30）并让 `_CMAKE_MIN` 由 policy 推导（推荐，符合约束 B.3 Linux 仅轻编译节点）；方案 B = workflow 增设 cmake 安装步（需走 actions.lock 审批） |
| F2 | hosted linux | `Bootstrap locked toolchain`（`linux_hosted.secondary_compiler`） | blocker | clang-19 present=false | policy 假定 ubuntu-24.04 镜像预装 clang-19，实际该批次未预装（或仅其他主版本） | **前台**：与 F1 同批——secondary_compiler 降至镜像实际主版本，或标注 optional；`bootstrap.py` 探测语义（major>=N）需同步确认 |
| F3 | hosted 两平台 evidence upload | `Upload small evidence` / `Upload public CI evidence`（`if: always()`） | minor | `No files were found with the provided path: artifacts/ci-public/` | bootstrap 失败路径从不生成 ci-public，`if-no-files-found: error` 使 always() 上传步在失败 run 上额外报错，掩盖主因展示 | **前台酌情**：bootstrap 失败路径预写最小 ci-public 存根（结构化失败 JSON），保证 always() 上传不空跑 |

## 7. 验收与合规

- `python3 ci/verify_actions_lock.py --offline` → exit 0（PASS, 4 entries）
- `python3 -m unittest discover -s ci/tests -p 'test_*.py'` → **216 tests OK**（24.8 s）
- `git status` 相对任务初始快照（89 行遗留变更，系其他任务产物）：仅新增 `evidence/v8_1_ci_control/tasks/V8-CI-010/**`，零额外变更
- 凭据纪律：token 仅 `git credential fill` 内联读取、零落盘；全部 API 请求 timeout ≤15 s；`logs/` 已脱敏扫描（ghp_/github_pat_/authorization 无命中）
- prohibited 全程未触碰：无 commit/push/branch/worktree/clone，未改控制包/TASK_STATE/WRITE_LEASE，未取消任何 workflow run

## 8. 遗留风险

1. **修复重跑前双平台 CI 持续红**：push 触发的后续提交仍会在 bootstrap 失败，每日 cron（19:17 UTC）同样；F1/F2 修复前 windows candidate 管线无法验证。
2. `record_observed_patch_versions: true`（windows_hosted）当前依赖 hosted 跑通后才能落地版本记录；本轮仅能记录 cmake 3.31.6 观测值。
3. gcc-14/VS/v143/Ninja 的通过结论基于"未列入 failed_tools"推断，bootstrap 结构化输出不含通过项明细；如需逐项留痕，可考虑修复批给 bootstrap `--json` 全量输出落 artifact（前台决定）。
4. 镜像批次漂移：20260830/20260831 批次观测值（cmake 3.31.6、无 clang-19）会随 GitHub 镜像更新变化，policy 若选方案 A 需给出下限而非精确锁定。

---

# 修复轮 2（V8-CI-010 同任务域，hosted 失败修复）— 2026-09-06

- 执行：SA-CI-32（本机修复轮，未触发 hosted 重跑；零 git 写操作，禁改文件零 diff）
- 裁决落实：F1（cmake 3.31.6 + policy 驱动）/ F2（clang-18）/ F3+产物缺口（上传路径 + 失败诊断步）

## R2.1 裁决对照与改动 diff 摘要

| 裁决 | 落实 | 文件（diff 摘要） |
|---|---|---|
| F1：hosted cmake 3.31.12 → 3.31.6（最低要求语义）+ observed_from | ✅ | `ci/toolchain.policy.json`：linux_hosted.cmake / windows_hosted.cmake = "3.31.6"；两 hosted 节各加 `"observed_from"`（image/image_batch=20260831.293.1 / 20260830.290.1、observed_utc、实测值、semantics="最低要求下限（minimum floor），非精确锁定"）；schema_version=2、fatduck 节未动。`ci/bootstrap.py`：删除硬编码 `_CMAKE_MIN=(3,31,12)` → `_CMAKE_FALLBACK_MIN=(3,31,6)` + 新增 `_cmake_min_from(policy_section)`（policy `cmake` 字段按版本解析作下限，非版本格式如 `project_minimum_or_newer` 回退 fallback，不崩溃）；两平台 cmake 探测项 `required` 与判定均取 policy 值（本机验证 required=">=3.31.6"） |
| F2：clang-19 → clang-18（实测 18.1.3） | ✅ | `ci/toolchain.policy.json`：linux_hosted.secondary_compiler="clang-18"，observed_from.secondary_compiler="18.1.3"；`ci/bootstrap.py`：secondary fallback 默认 clang-19→clang-18，期望与 major>=N 判定跟随 policy |
| F1/F2 透传 | ✅ | `ci/bootstrap.py`：`build_report(..., policy_section)` 新增参数，`--json` 报告顶层输出 `"observed_from"`（取自对应 policy 节） |
| F3+产物缺口：上传路径 → artifacts/ci/ + failure() 诊断步 | ✅ | `ci-linux.yml`：Upload small evidence path `artifacts/ci-public/` → `artifacts/ci/`（run.py 实际输出目录 artifacts/ci/<sha12>/<run-id>/，契约 07）；新增 `Collect bootstrap diagnostics`（`if: failure()`，bash heredoc）。`ci-windows.yml`：Upload public CI evidence path 同改；新增同名义步（pwsh here-string）。诊断步重探 `ci/bootstrap.py --json`，把完整报告 + stderr 结构化 FAIL 封装写入 `artifacts/ci/BOOTSTRAP_DIAG.json`（子进程捕获、自带 try/except，不引入新失败面）；always() 上传语义与 `if-no-files-found: error` 保持，uses SHA 未动 |
| 测试同步 | ✅ | `ci/tests/test_workflow_lock.py`：mock 探测 clang-19/19.1.0 → clang-18/18.1.3；新增 4 用例（上传路径=artifacts/ci/ 且无 ci-public、failure() 诊断步存在且产出 BOOTSTRAP_DIAG.json、observed_from 透传 + policy 驱动版本、_cmake_min_from 解析与 fallback） |

禁改完整性（相对 HEAD diff 行数）：`ci/run.py`=0、`ci/checks.json`=0、`ci/actions.lock.json`=0、`fatduck.yml`=0、schema 未触碰。

## R2.2 验收实测（详见 logs/repair_round2.log）

| # | 命令 | 结果 |
|---|---|---|
| 1 | `validate_registry --strict` | exit 0，80 checks，error_count=0，PASS |
| 2 | `verify_actions_lock`（在线） | exit 0，PASS (4 entries) |
| 3 | `unittest discover -s ci/tests` | **Ran 220 tests — OK**（基线 216 全绿 + 本轮新增 4，零失败） |
| 4 | `bootstrap --policy ... --platform linux --json`（本机） | exit **2**（预期：非 hosted、缺 gcc-14）；JSON：cmake required **>=3.31.6**、clang-**18**、`observed_from.image_batch`=20260831.293.1（logs/repair_round2_bootstrap_local.json） |
| 5 | `run.py --plan-only` × 4 | fast **57** / linux-main **71** / linux-deep **7** / windows-main **61** |
| 6 | `git diff --stat` | 本轮改动 = toolchain.policy.json / bootstrap.py / 两 yml / test_workflow_lock.py（+5 文件）；diff 中另见 AGENTS.md 与 dist/audit zip 为任务开始前工作区遗留（初始快照可证），非本轮产物 |

## R2.3 hosted 重跑预期

- **linux（ci-linux.yml @ 新提交）**：`Bootstrap locked toolchain` 预期 PASS——cmake 3.31.6 ≥ 下限 3.31.6、gcc-14/Ninja 轮 1 已在镜像验证；clang-18 为唯一待复验项（轮 1 该镜像批次仅证明无 clang-19，18.1.3 为另行观测值；若批次漂移致 clang-18 缺失，bootstrap 仍 exit 2，但本轮新增诊断步会落 BOOTSTRAP_DIAG.json 且 evidence 上传不再空跑）。bootstrap 过后 Select profile / 71 项 checks（linux-main）可执行，artifacts/ci/ 上传有实体。
- **windows（ci-windows.yml）**：`Bootstrap locked toolchain` 预期 PASS——cmake 达标、VS 17/v143 轮 1 已验证；61 项 checks + candidate 打包/校验/上传管线恢复可执行。
- 计数回归：hosted 实跑 check 数应从 0/71、0/61 恢复为非零；resource summary 三数字由下一观察轮补录。

## R2.4 遗留风险

1. **镜像批次漂移**：observed_from 记录的 20260831.293.1 / 20260830.290.1 批次会随 GitHub 更新变化；policy 现为最低要求语义（cmake 只升不降安全；clang-18 若未来镜像移除则 bootstrap 重红，届时按同机制降/换版本或标注 optional）。
2. **clang-18 未在本轮实测**：18.1.3 为观测引用值，hosted 重跑是首次真验证；诊断步保证即使失败也有结构化证据。
3. windows 上轮 exit 2 被 pwsh 报为 step exit 1 的包装差异仍在（未改语义）；诊断步在 pwsh here-string 下首跑，若编码异常 `collection_error` 会如实记录。
4. `record_observed_patch_versions: true`（windows_hosted）仍待 hosted 跑通后落地完整版本记录。

---

# 观察轮 2（2026-09-06，owner SA-CI-32，mode=read）

- 观察对象：SHA `2508a37938b31cb9e00c3a16356cf929a7155ebc`（修复轮 2 push，远端 main HEAD）的 ci-linux.yml / ci-windows.yml runs
- 轮 1 修复 F1/F2/F3 的 hosted 验证结论：**全部生效**（bootstrap 双平台 PASS；evidence artifact 有实体）
- **总体结论：RUNS_PARTIAL_PROGRESS_THEN_FAILURE_OBSERVED —— 两 run completed/failure，阻塞点从 bootstrap 门禁推进到 71/61 项检查实跑内部的平台适配与基线状态**

## R5. 两 run 终态与时长

| 项 | ci-linux run 34005360753 | ci-windows run 34005360732 |
|---|---|---|
| event / head_branch | push / main | push / main |
| head_sha | 2508a379… | 2508a379… |
| status / conclusion | completed / **failure** | completed / **failure** |
| run_started_at | 2026-09-06T02:00:47Z | 2026-09-06T02:00:47Z |
| job 窗口 | 02:00:50→02:13:08（**≈12m21s**） | 02:00:49→02:02:56（**≈2m07s**） |
| Runner / 镜像 | GitHub Actions 1000000558 / ubuntu-24.04（20260831.293.1） | GitHub Actions 1000000552 / windows-2022（20260830.290.1） |
| Checkout exact SHA | success | success |
| Bootstrap locked toolchain | **success**（PASS, 6 items） | **success**（PASS, 5 items） |
| 检查实跑 | 71 项全执行（47 PASS） | 61 项实跑（32 PASS） |
| artifact | linux-ci-2508a379…（id 9980906245，188,056 B，213 members） | windows-ci-2508a379…（id 9980773767，149,036 B，183 members） |
| candidate | —（本 profile 无） | **未产生**（见 R8） |

失败步骤：linux `Run registered checks`（verdict=FAIL exit 1）；windows `Run MSVC tests and package candidate`（verdict=FAIL exit 1），后续 Validate/Upload candidate 因非 `always()` SKIPPED。

## R6. bootstrap 实测工具链（hosted，修复验证点）

| 工具 | linux 实测（ubuntu-24.04） | windows 实测（windows-2022） |
|---|---|---|
| gcc-14 | **14.2.0**（Ubuntu 14.2.0-4ubuntu2~24.04.1） | — |
| clang-18 | **18.1.3**（Ubuntu clang 18.1.3 (1ubuntu1)）—— F2 修复验证 ✓ | — |
| cmake | **3.31.6**（required ≥3.31.6，policy 驱动）—— F1 修复验证 ✓ | PASS 推断 ≥3.31.6（逐项版本不可得，见 F-R2-02） |
| Ninja | **1.13.2** | — |
| VS / v143 | — | PASS 推断 VS 17.x / v143（逐项版本不可得，同上） |
| runner / arch | ubuntu-24.04 / x86_64 ✓ | windows-2022 / AMD64 ✓ |

数据源：linux artifact 内 `BOOTSTRAP_DIAG.json`（诊断步 bootstrap_exit_code=0，report 6/6 items 全 ok）。windows 诊断步 `bootstrap_exit_code=1、report=null、stderr_fail_payload=null`——pwsh 子进程 cp1252 下 bootstrap `--json` 非纯 ASCII 输出（中文 repair 文案）编码崩溃，已本地 `PYTHONIOENCODING=cp1252` 复现（exit=1、stdout=0B、UnicodeEncodeError）＝ F-R2-02。

## R7. check 分布与根因（71 / 61 实跑）

### linux-main 71 项：PASS=47 / FAIL=21 / TIMEOUT=2 / SKIPPED(waivable)=1

- **TIMEOUT（2）**：UT-BACKEND、UT-CLI —— 登记 300s 不够（hosted runner 真实执行含 g++ 编译型用例）。
- **SKIPPED(waivable)（1）**：BUILD-GCC-RELEASE —— prerequisite 未满足：`run/ci/build-gcc-release` 前置产物不存在。
- **DEEP-\***：不在 linux-main（本地 plan-only 复算 71 项无 DEEP id；DEEP-* 7 项属 linux-deep profile，hosted 未运行）。
- **FAIL 归类**（完整 id 清单见 TASK_RESULT.json `observation_round2.check_distribution`）：
  1. 跨平台基线态失败（控制节点本地 linux 同命令逐项对照同 exit 1，与 2508a379 无关）：AGENTS-GOV、CLI-COMMAND-LAYER、CLI-RUN-PRESET、DOC-L0、P3-STATUS、NO-SERIAL-HEAVY、SERIAL-HARDCODE、THREAD-BUDGET、RECONCILE-STATE(w)、WORKSPACE-ADOPTION(w)、CON-DOC-SYMBOLS、CON-FULL-INTEGRATION、DOC-INDEX、ENG-CONSTRAINTS、UT-VERSION、UT-ARCH、UT-QUALITY
  2. 依赖未入库工作区文件（`git ls-tree 2508a379`：顶层 工程控制/ =0 项）：TASK-RESULT-SCHEMA（schema missing exit 2）、UT-API（`工程控制/RELEASE_V5/.../04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md` FileNotFoundError）
  3. linux 环境依赖缺口：UT-IO（镜像 python3.12 无 numpy → 3 模块 ImportError）
  4. 真实源码缺陷：UT-CPU-AVX512（`providers/cpu/avx512/src/avx512_provider.cpp:92` std::memset 在 gcc-14 `-mavx512*` 下 always_inline target option mismatch → .so 编译失败）

### windows-main 61 项：PASS=32 / FAIL=28 / SKIPPED(waivable)=1

- **SKIPPED(waivable)（1）**：WIN-PACKAGE-CANDIDATE —— dumpbin 不在 hosted PATH → candidate 打包未执行。
- **FAIL 归类**：
  1. 跨平台基线态（同 linux 同 id 16 项）：AGENTS-GOV、CLI-COMMAND-LAYER、CLI-RUN-PRESET、CON-DOC-SYMBOLS、CON-FULL-INTEGRATION、DOC-INDEX、DOC-L0、ENG-CONSTRAINTS、NO-SERIAL-HEAVY、P3-STATUS、RECONCILE-STATE(w)、SERIAL-HARDCODE、TASK-RESULT-SCHEMA、THREAD-BUDGET、UT-VERSION、WORKSPACE-ADOPTION(w)
  2. windows 平台适配（F-R2-06）：8 项 UT（CONTRACTS/GLOSSARY/MONITORING/PIPELINE/RUNTIME/SCIENCELINT/TRACEABILITY）+ VERSION-NAMESPACES —— cp1252 UnicodeDecode/EncodeError（subprocess 未显式 encoding）+ C:/D: 跨盘 os.path.relpath ValueError + r.stderr=None TypeError
  3. windows 依赖缺口：CONTRACT-GRAPH（无 PyYAML exit 2；linux 同项 PASS 交叉证实镜像差异）＝ F-R2-03
  4. 镜像噪音：WARNING-SUPPRESSION（WSL 空发行版提示被计为生产构建警告）＝ F-R2-07
  5. **shim 导入链断裂（F-R2-01，本轮最重要新发现）**：WIN-BUILD-RELEASE、WIN-TEST-UNIT —— `ci/resource_monitor.py`（runpy.run_path 桥接）执行环境内 `from tools.monitoring import resource_probe` 与 fallback `import resource_probe` 双双 ModuleNotFoundError → exit 1；**控制节点 linux 同命令复现同 traceback**（跨平台 shim 缺陷）。凡经 shim 包装的检查必败：WIN-* 2 项实踩，BUILD-GCC-RELEASE（本轮先因前置 SKIP）、DEEP-*（未运行）潜在。

## R8. candidate 深检

**NOT_PRODUCED**：`runs/34005360732/artifacts?name=astrocs-windows-candidate-2508a37938b31cb9e00c3a16356cf929a7155ebc` → total_count=0（logs/round2/candidate_artifact_query_34005360732.json）。根因链：package 检查（WIN-PACKAGE-CANDIDATE）dumpbin prerequisite SKIPPED → run.py verdict=FAIL → Validate/Upload candidate 步骤 skipped；`/tmp/dsh-candidate2/` 无下载对象。SHA256SUMS 逐项相符 / validate_candidate PASS / BUILD_PROVENANCE.source_sha / 排除规则四项均 NOT_EXECUTABLE（无对象可检）。

## R9. resource summary

**NOT_COLLECTED（两平台）**：峰值内存 / CPU 利用率 / 超时计数无 monitor JSON 来源——linux-main 无 heavy 检查实跑（BUILD-GCC-RELEASE SKIP、DEEP-* 不在 profile），windows WIN-* 死于 shim 导入未进监控层。超时计数替代口径：linux TIMEOUT verdict ×2（UT-BACKEND、UT-CLI）、windows ×0。

## R10. FINDINGS（轮 2，归属与建议见 TASK_RESULT.json findings_round2）

| id | 模块 | 严重度 | 一句话 |
|---|---|---|---|
| F-R2-01 | ci/resource_monitor.py + tools/monitoring/run_monitored.py | blocker(监控链) | shim runpy 链 sys.path 断裂，双导入均败；本地+hosted 双证实 |
| F-R2-02 | ci/bootstrap.py --json / ci-windows.yml 诊断步 | diagnostic-loss | cp1252 下 --json 输出编码崩溃 → windows 诊断 report 双 null |
| F-R2-03 | ci-windows.yml 依赖声明 | major | hosted windows 无 PyYAML → CONTRACT-GRAPH exit 2 |
| F-R2-04 | ci-linux.yml 依赖声明 | major | hosted ubuntu-24.04 无 numpy → UT-IO ImportError |
| F-R2-05 | windows-main / checks.json | major(candidate 管线) | dumpbin 不在 PATH → WIN-PACKAGE-CANDIDATE SKIP → candidate 断 |
| F-R2-06 | tests/*（windows 兼容） | major | cp1252 编码 / C:/D: 跨盘 relpath / stderr None 判定 |
| F-R2-07 | tools/check_warning_suppression.py | minor | WSL 镜像噪音计入生产构建警告 |
| F-R2-08 | providers/cpu/avx512/src/avx512_provider.cpp | major(build) | gcc-14 -mavx512\* memset always_inline mismatch 编译失败 |
| F-R2-09 | repo 内容（基线状态） | major(基线) | ~20 项治理/文档/源码状态检查干净 checkout 本就 FAIL + 检查依赖未入库 工程控制/ |
| F-R2-10 | checks.json timeout 登记 | minor | UT-BACKEND/UT-CLI 300s hosted 实跑不足 |

本轮只读观察，未改任何源码/CI/控制包文件。

## R11. 验收与证据

- `git status --porcelain`：相对任务初始快照（89 行遗留）唯一增量 = `M evidence/v8_1_ci_control/tasks/V8-CI-010/TASK_RESULT.json`；logs/** 按 .gitignore 全局 logs/ 规则不进 status（与轮 1 口径一致，round2/ 落盘 407 项以本清单计）。
- `python3 -m unittest discover -s ci/tests -p 'test_*.py'`：**Ran 220 tests — OK**（25.0s）。
- logs/round2/：两 run jobs/artifacts/candidate 查询 API trace、两平台 job 全量日志（runner 掩码已 [REDACTED] 化、无凭据）、两 artifact 原始 zip + 解包实体（CI_RESULT.json / BOOTSTRAP_DIAG.json / 61+71×checks/*.json / logs）、本地 plan 复算 @2508a379、轮询脚本。

## R12. 遗留风险（轮 2）

1. resource summary 数字持续缺位：需 F-R2-01 修复 + heavy 检查真实执行后才可回填。
2. windows bootstrap 逐项版本（VS/cmake patch 版本）无实测记录，F-R2-02 修复后下一轮补全。
3. candidate 管线端到端尚未打通：F-R2-05（dumpbin）+ F-R2-06（UT windows 适配）+ F-R2-09（基线态失败）任一未解，Validate/Upload 仍不可达。
4. UT-BACKEND/UT-CLI hosted 实跑性能未标定，timeout 调整需按 AGENTS 纪律以 benchmark 依据走登记变更。
5. 镜像批次漂移与 policy minimum-floor 语义维持轮 1 R2.4 判断。

---

# 修复轮 3（SA-CI-32，2026-09-06，@ HEAD 2508a37938b31cb9e00c3a16356cf929a7155ebc）

## R13. 范围与裁决执行

本轮仅执行 F-R2-01（blocker）+ F-R2-02 全量、F-R2-06 接入侧；F-R2-03/04/05/08/09/10
不修，仅归属登记（见 R17）。基线：上轮 hosted 双 run（34005360753 / 34005360732）
+ 本地可复现证据。全程零 git 写操作；无凭据接触。

## R14. F-R2-01 根因定位与修复（ci/resource_monitor.py）

- 根因：shim 以 ``runpy.run_path(MONITORED, run_name="__main__")`` 桥接
  ``tools/monitoring/run_monitored.py``；脚本模式下解释器会把被执行脚本所在目录置于
  ``sys.path[0]``，而 runpy.run_path 不做该注入——shim 自身被调用时 ``sys.path[0]``
  停在 ``ci/``，``tools/monitoring/`` 与 repo 根均不在 ``sys.path``，
  run_monitored 顶部 ``from tools.monitoring import resource_probe`` 与 fallback
  ``import resource_probe`` 双双 ModuleNotFoundError（hosted WIN-BUILD-RELEASE /
  WIN-TEST-UNIT 实踩 + linux 控制节点同命令复现，跨平台 shim 缺陷）。
- 修复（最小修：目标脚本目录入 sys.path，选与现设计最贴近方案；不改监控采集逻辑
  与输出 schema）：``main()`` 在 runpy 前将被包装脚本目录
  ``str(MONITORED.parent)`` 注入 ``sys.path[0]``（已存在则不重复注入），
  ``finally`` 恢复，不污染 in-process 调用方。
- 复现-修复-复验三段（完整命令与输出见 logs/repair_round3.log）：
  1. 复现：原版 shim（``git show HEAD:ci/resource_monitor.py`` 只读重放）经 ci/ 真实
     路径包装 bootstrap --help → ``ModuleNotFoundError: No module named
     'resource_probe'``、exit 1（与 hosted WIN-* traceback 同源）。
  2. 修复：注入 + finally 恢复（diff 19 行）。
  3. 复验：包装 ``ci/bootstrap.py --help`` / ``tools/quality/ci_windows_driver.py
     --help``（hosted WIN-* 被包装脚本本体）/ ``tools/monitoring/run_monitored.py
     --help`` 三链 exit 0；与直接执行逐字节一致（run_monitored stdout_tail 既有
     schema 按 ``if line:`` 剔除空行，剔除空行后逐行全等——监控采集层依裁决不动）；
     sys.path 无残留（单测锁定）。

## R15. F-R2-02 根因定位与修复（ci/bootstrap.py）

- 根因：pwsh 子进程 cp1252 控制台下，``--json`` 报告与 stderr 结构化 FAIL 的中文
  repair 文案（ensure_ascii=False 非 ASCII）在 strict 编码下 UnicodeEncodeError →
  stdout 0B、exit 1、hosted 诊断 report/stderr_fail_payload 双 null。
- 修复：新增 ``_force_utf8_stdio()``（main() 入口调用），对自身 stdout/stderr
  ``reconfigure(encoding="utf-8")``，仅原地改写既有 TextIOWrapper 的 encoding、
  绝不替换流对象（替换会丢与 argparse/stdlib 共享的缓冲绑定，--help 空行错位
  实测）；reconfigure 不可用时静默保留原流。探测子进程（capture_output bytes 侧）
  与诊断步采集逻辑不受影响。
- 复现-修复-复验三段（logs/repair_round3.log）：
  1. 复现：原版 bootstrap（git show HEAD: 只读重放）+ ``PYTHONIOENCODING=cp1252
     --json`` → exit=1、stdout_bytes=0、UnicodeEncodeError traceback×1（与 hosted
     BOOTSTRAP_DIAG 同值）。
  2. 修复：_force_utf8_stdio（diff +29 行含 docstring）。
  3. 复验：同命令 exit=2（受控失败=本地非 hosted 预期）、stdout=2056B 完整 JSON、
     stderr 结构化 FAIL payload 可解析、中文 repair 文案保真（"非 GitHub hosted" 等）、
     UnicodeEncodeError=0。

## R16. F-R2-06 接入侧修复（ci/tests，不动被测业务代码语义）

| 用例/位置 | 脆断类 | 修复 |
| --- | --- | --- |
| _helpers.py `sh()`（全部 runner/validator 调用汇点） | subprocess text=True 未显式 encoding → windows cp1252 locale 解码中文输出 UnicodeDecodeError | 显式 `encoding="utf-8", errors="replace"` |
| test_deep_profiles.py `TestProfileSelection.plan` | 同上 | 同上 |
| test_workflow_lock.py `run_script` | 同上 | 同上 |
| test_fatduck_workflow.py bash 子进程 | 同上 | 同上 |
| test_windows_ci.py runner 集成（windows-main SKIPPED 断言） | 同上 | 同上 |
| test_windows_ci.py 假 cmake/dumpbin stub | windows `shutil.which` 依赖 PATHEXT，无扩展名 stub 对 which 不可见 → prerequisite 误判 | 按 `os.name == "nt"` 写 `<tool>.exe`（posix 行为不变） |
| 跨盘 relpath（C:/D: ValueError） | 接入侧 grep 复核：ci/tests 内 relpath 用法仅 `Path.relative_to`（同根安全）与 `as_posix()`（跨平台正显），无 `os.path.relpath` 调用 | 无需改动（登记结论） |

被测业务代码（ci/run.py、tools/quality/*、tools/monitoring/*）零改动。

## R17. F 项归属更新（TASK_RESULT.json `findings_round2[].resolution_round3` 同步）

| F 项 | 轮 3 处置 | 归属 |
| --- | --- | --- |
| F-R2-01 | **FIXED_round3**（本地复现-修复-复验；待 hosted WIN-* 重跑确认） | 本轮修复 |
| F-R2-02 | **FIXED_round3**（同上） | 本轮修复 |
| F-R2-06 | **FIXED_CI-SIDE_round3**（上表 6 处接入侧；tests/* 业务侧 UT windows 适配不在本轮范围） | 本轮（接入侧部分）；业务侧 UT 归 hosted UT 修复任务 |
| F-R2-03 | NOT_FIXED_round3 — 归属登记：镜像缺口（windows hosted 无 PyYAML）→ workflow 安装步候选（登记不做，安装步属 workflow 变更域） | workflow 安装步候选（登记） |
| F-R2-04 | NOT_FIXED_round3 — 归属登记：镜像缺口（ubuntu-24.04 python3.12 无 numpy）→ workflow 安装步候选（登记不做） | workflow 安装步候选（登记） |
| F-R2-05 | NOT_FIXED_round3 — 归属登记：dumpbin prerequisite → WIN-PACKAGE 检查前提（V8-CI-006 域已 waivable，不属监控/接入域） | V8-CI-006 域 |
| F-R2-08 | NOT_FIXED_round3 — 归属登记：avx512 memset target option mismatch → G-FIX 波任务链 | G-FIX 波 |
| F-R2-09 | NOT_FIXED_round3 — 归属登记：基线态失败（治理/文档/源码状态类）→ G-FIX 波任务链 | G-FIX 波 |
| F-R2-10 | NOT_FIXED_round3 — 归属登记：UT-BACKEND/UT-CLI timeout 300s hosted 不足 → G-FIX 波任务链（登记变更流程） | G-FIX 波 |
| F-R2-07 | NOT_FIXED_round3 — 未列入本轮前台裁决范围，维持轮 2 建议归属 | （不在本轮范围） |

## R18. 修复轮 3 验收（逐条实测，logs/repair_round3.log 同步）

1. `python3 ci/validate_registry.py --registry ci/checks.json --strict` → exit 0，
   verdict=PASS（checks=80，error_count=0）。
2. `python3 -m unittest discover -s ci/tests -p 'test_*.py'` → **Ran 231 tests — OK**
   （220 基线全绿 + 11 新增：test_resource_monitor_shim 7 项 + test_bootstrap_utf8 4 项）。
3. `python3 ci/resource_monitor.py -- python3 ci/bootstrap.py --help` → exit 0，
   monitor JSON wrapped_exit_code=0（包装链语义验证通过）。
4. `python3 ci/run.py --profile windows-main --plan-only` → selected_count=**61** 不变。
5. `PYTHONIOENCODING=cp1252 python3 ci/bootstrap.py --policy ci/toolchain.policy.json
   --platform linux --json` → stdout UTF-8 JSON 正常输出，无 UnicodeEncodeError。
6. `git diff --stat`（本轮代码改动集）：仅 ci/resource_monitor.py、ci/bootstrap.py、
   ci/tests/{_helpers,test_deep_profiles,test_fatduck_workflow,test_windows_ci,
   test_workflow_lock}.py（7 文件 +74/-9）+ 新增 ci/tests/{test_resource_monitor_shim,
   test_bootstrap_utf8}.py + 证据三件。无其他文件触碰。

## R19. 遗留风险（轮 3）

1. F-R2-01/02/06(接入侧) 为本地验证修复，hosted WIN-BUILD-RELEASE / WIN-TEST-UNIT
   与 windows 诊断步需下一观察轮确认（预期见 R20）。
2. 监控链打通后 WIN-* heavy 项仍依赖 run_monitored 的 /proc 采集（Linux-only probe
   语义已在库内标注）；windows 宿主上 monitor JSON 的 cpu/rss 字段可用性待 hosted
   实测（不阻塞本轮 scope）。
3. R12 其余遗留项不变（基线态失败、timeout 标定、镜像批次漂移）。
