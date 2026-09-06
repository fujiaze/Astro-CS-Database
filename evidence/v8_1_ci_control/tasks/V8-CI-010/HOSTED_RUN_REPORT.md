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

---

# 观察轮 3（SA-CI-32，@ HEAD 77da09effe1f997f3149f4845ac8c3a471d47038，2026-09-06）

只读观察：push 触发 ci-linux.yml / ci-windows.yml（run 34008273326 / 34008273413），
API trace + 双平台 artifact 全量解包留档 logs/round3/。零 git 写操作；无源码修改；
token 内联不落盘。对照基线 = 轮 2（同 profile，HEAD 2508a379）。

## R20. run 终态与时长

| | linux（34008273326） | windows（34008273413） |
| --- | --- | --- |
| workflow / 触发 | ci-linux.yml / push | ci-windows.yml / push |
| job | 101419369057（ubuntu-24.04，镜像 20260831.293.1） | 101419369159（windows-2022，镜像 20260830.290.1） |
| 终态 | completed / **failure** | completed / **failure** |
| 时段（UTC） | 03:09:11 → 03:21:22（≈12m11s） | 03:09:14 → 03:11:34（≈2m20s） |
| 步骤 | Checkout ✓ / Bootstrap ✓ / Select profile ✓ / Run registered checks ✗ / 诊断步 ✓ / Upload evidence ✓ | Bootstrap ✓（5 items）/ Run MSVC tests and package candidate ✗ / Validate+Upload candidate skipped / 诊断步 ✓ / Upload evidence ✓ |
| 汇总行 | `verdict=FAIL total=71 pass=47 fail=23 known_fail=0 skipped_waivable=1`（FAIL 21 + TIMEOUT 2） | `verdict=FAIL total=61 pass=32 fail=28 known_fail=0 skipped_waivable=1`（FAIL 27 + FAIL(missing_output) 1） |

## R21. check 分布对照（轮 3 vs 轮 2，逐 id 闭合）

| profile | PASS | FAIL | TIMEOUT | SKIP(waivable) | 合计 | 轮 2 分布 |
| --- | --- | --- | --- | --- | --- | --- |
| linux-main | 47 | 21 | 2 | 1 | 71 | 47/21/2/1 —— **完全一致** |
| windows-main | 32 | 27 | 0 | 1（+missing_output 1 计入 fail 28） | 61 | 32/28/0/1 —— **fail id 集合完全一致** |

- linux 非 PASS 24 项逐一对照轮 2 fail_ids/timeout_ids/skipped_ids：差集为空
  （AGENTS-GOV、CLI-COMMAND-LAYER、CLI-RUN-PRESET、CON-DOC-SYMBOLS、
  CON-FULL-INTEGRATION、DOC-INDEX、DOC-L0、ENG-CONSTRAINTS、NO-SERIAL-HEAVY、
  P3-STATUS、RECONCILE-STATE、SERIAL-HARDCODE、TASK-RESULT-SCHEMA、THREAD-BUDGET、
  UT-API、UT-ARCH、UT-BACKEND(T)、UT-CLI(T)、UT-CPU-AVX512、UT-IO、UT-QUALITY、
  UT-VERSION、WORKSPACE-ADOPTION + BUILD-GCC-RELEASE（两轮均 SKIPPED(waivable)
  同 reason，见 R23——**非本轮变化**，轮 2 观察已登记于 skipped_ids）。
- windows 28 个 fail id 与轮 2 完全一致；唯一 verdict 类别变化：WIN-TEST-UNIT
  FAIL → **FAIL(missing_output)**（见 R24）。本轮无新失败 id，轮 2 登记的
  F-R2-03（PyYAML/CONTRACT-GRAPH，hosted 实测 `CONTRACT_GRAPH_FAIL: 需要 PyYAML`）、
  F-R2-04（numpy/UT-IO errors=3）、F-R2-05（dumpbin）、F-R2-09（基线态 17+2 项）、
  F-R2-10（UT-BACKEND/UT-CLI timeout 300s，exit -9 signal 9）全部维持原判。

## R22. resource summary（F-R2-01 shim 效果判定）

- **windows：包装层修复确认生效。** WIN-BUILD-RELEASE / WIN-TEST-UNIT 经
  ci/resource_monitor.py 包装真实执行（轮 2 死于 shim ModuleNotFoundError，本轮
  monitor JSON 均产出，且 --output 指向 run/ci/monitor/*.json 的落盘链未再报错）。
  monitor 数字（hosted 实测，两检查 host_probe 一致）：

  | 字段 | WIN-BUILD-RELEASE | WIN-TEST-UNIT |
  | --- | --- | --- |
  | 被包装 exit_code | 1（configure 即败，见 R24） | 0（ctest 空跑，见 R24） |
  | duration_seconds | 0.422 | 0.813 |
  | cpu_samples（采样数） | 3（@0/0.219/0.422s） | 5（@0…0.813s） |
  | cpu_percent_avg / median | null | null |
  | peak_rss_kb / rss_start_kb | null | null |
  | threads_max | 0 | 0 |
  | io_read/write_bytes | null | null |
  | host_probe | affinity=4、logical=4、effective=4.0、max_workers=4（derived） | 同左 |

  cpu/rss/threads null 与 0 与轮 2 预期一致（run_monitored 采集探针为 Linux
  /proc 语义，windows 宿主无 psutil），非回归；host_probe 4 核探测成功（affinity
  fallback:cpu_count）说明监控进程本体存活并完成采样循环。
- **linux：数字未产出，非 shim 缺陷。** linux-main 中唯一 monitor 包装检查
  BUILD-GCC-RELEASE 未真实执行 —— run.py 前置预检命中 F-R3-03（R23，两轮同因）→
  SKIPPED(waivable)，monitor JSON 不存在。DEEP-* 不在本 profile。结论：
  F-R2-01 shim 修复在两平台包装链路均验证打通（windows 端到端实跑、linux 端
  registry 复验 requires_monitor 包装器就绪检查通过——即监控脚本自身不再触发
  ModuleNotFoundError 类前置失败）。

## R23. linux BUILD-GCC-RELEASE 持续误 SKIP（新发现 F-R3-03，根因本轮定位）

- 现象：轮 2 与轮 3 **均** `SKIPPED(waivable) reason=prerequisite 未满足
  （waivable）：command 引用的仓库路径不存在：run/ci/build-gcc-release`（轮 2
  登记 skipped_ids，非本轮迁移）。但轮 2 仅登记现象，未定位根因；本轮据
  artifact + registry + run.py 源读定位如下。
- 根因链（读源实证，非改动）：ci/checks.json 注册 command 为
  `ci/resource_monitor.py tools/quality/deep_ci_driver.py build-gcc-release --build-dir run/ci/build-gcc-release`；
  ci/run.py probe_prerequisite（448-470 行）对 command 中含 `/` 的参数逐一做
  仓库路径存在性探测（仅豁免 `outputs` 登记项），而 `run/ci/build-gcc-release`
  是**运行时产物目录**（--build-dir 输出，非登记 output）——hosted 干净 checkout
  上必然不存在 → 前置预检把「本轮要生成的目录」误判为「必须预先存在的输入」。
  同 command 中 `run/ci/monitor/BUILD-GCC-RELEASE.json` 与
  `run/ci/build-gcc-release-summary.json` 因已登记 outputs 被豁免，唯 build-dir
  漏登记。
- 影响面：registry 全量扫描，同形态 build-dir 位置参数（未登记 outputs）共 5 处：
  BUILD-GCC-RELEASE（run/ci/build-gcc-release）、DEEP-CLANG-BUILD（build-clang）、
  DEEP-SAN-ASAN（build-qa-asan）、DEEP-SAN-TSAN（build-qa-tsan）、
  DEEP-COV-CPP（build-cov）——DEEP-* 4 项同踩但属 linux-deep profile 本轮未触发，
  linux-main 仅 BUILD-GCC-RELEASE 一处中招。`python3 -B` 参数不受影响
  （`-B` 以 `-` 开头被预检跳过）。
- 连带效果：该项 skip 后 linux-main 无任何监控包装检查实跑 → resource summary
  linux 侧无 monitor JSON（R22）。F-R2-08（gcc 实编译失败链）从未在 hosted
  linux-main 有机会暴露——被预检 SKIP 遮蔽，失败本体未消失。

## R24. windows WIN-BUILD-RELEASE / WIN-TEST-UNIT / candidate 管线

- WIN-BUILD-RELEASE：包装执行成功，内部 `ci_windows_driver.py --stages
  configure,build` exit 1 —— configure 阶段 `cmake --preset
  win-msvc-17.14.39-x64` **0.422s 即败**。stdout_tail 仅驱动横幅
  `[ci_windows_driver] configure: cmake --preset win-msvc-17.14.39-x64`，stderr 空；
  cmake 真实错误行只进 driver summary JSON（落 run/ci/win-build-summary.json，
  不在 evidence artifact 契约内）→ **hosted 观测盲点**（新发现 F-R3-02）。
  根因强假设：CMakePresets.json hidden `base-msvc` 硬编码
  `CMAKE_GENERATOR_INSTANCE=C:/AstroCS/toolchains/vs2022-17.14.39`，hosted
  windows-2022 runner 无该目录 → configure 立即 exit 1（0.42s 与 generator
  instance 探测失败的耗时特征一致；preset 为冻结文件，改动权在前台）。
- WIN-TEST-UNIT：exit 0（0.813s，monitor samples 5）但登记 output
  `run/ci/win-test-junit.xml` 未产出 → verdict=**FAIL(missing_output)**（轮 3
  新出现类别）。stdout_tail 仅 ctest 调用横幅，ctest 自身输出（含是否
  "No tests found"）同样只进 run/ci/win-test-summary.json（F-R3-02 盲点延伸）；
  「configure 失败 → build 目录无测试 → ctest 空跑 exit 0」为机制级联推断
  （依据：0.813s 时长 + exit 0 + junit 缺失 + 与 WIN-BUILD 同 preset 链）。
  判定：**测试未真实运行**，R13 的「测试实际执行」预期只达成半程
  （包装层 ✓ / cmake 链 ✗）。
- WIN-PACKAGE-CANDIDATE：SKIPPED(waivable)（dumpbin 不在 PATH）维持轮 2 预期
  （F-R2-05）；Validate candidate / Upload candidate 两步 skipped，candidate
  artifact 为 0，evidence artifact 恰 1 个（id 9981668901）——管线链路
  「configure 败 → package 未执行 → candidate 缺失」闭环成立，无悬空状态。

## R25. bootstrap 与诊断步（F-R2-02 收口 + 新发现 F-R3-01）

- linux：BOOTSTRAP_DIAG.json **完整回填**（report 非空，bootstrap_exit_code=0；
  gcc-14 14.2.0-4ubuntu2~24.04.1、clang-18 18.1.3、cmake 3.31.6（policy 最低
  >=3.31.6 恰好达标）、Ninja 1.13.2、镜像 20260831.293.1、ok=true）——轮 2
  修复（policy 下限 + _force_utf8_stdio）hosted 侧验证通过。**F-R2-02 在
  linux 侧收口。**
- windows：bootstrap 主步 PASS（exit 0，5 items），但 BOOTSTRAP_DIAG.json
  report=null、stderr_fail_payload=null，collection_error=
  `AttributeError("'NoneType' object has no attribute 'strip'")`。job log
  217-229 行铁证：`Exception in thread Thread-1 (_readerthread): UnicodeDecodeError:
  'charmap' codec can't decode byte 0x81 in position 350` —— 诊断步
  （ci-windows.yml Collect bootstrap diagnostics）`subprocess.run(...,
  text=True)` 未显式 encoding，windows cp1252 控制台下 bootstrap --json 输出
  的 UTF-8 字节流 decode 炸 → stdout=None → .strip() 抛 AttributeError。
  **新发现 F-R3-01**（与 F-R2-06 同族：业务侧 UTF-8 已修，诊断接入侧漏网；
  ci-linux.yml 诊断步为 bash heredoc 无此问题，linux 侧完好的原因）。
- BOOTSTRAP_DIAG 出现本身属预期：诊断步 `if: failure()` 由 run.py check 失败
  触发（非 bootstrap 失败），两平台一致。

## R26. 观察轮 3 FINDINGS（新增 3 项）

| id | 模块 | severity | 根因 | 建议（仅登记，改动权在前台） |
| --- | --- | --- | --- | --- |
| F-R3-01 | .github/workflows/ci-windows.yml 诊断步（接入侧） | major(diagnostic-loss) | `subprocess.run(text=True)` 无 encoding → cp1252 下 UnicodeDecodeError → report/stderr 双 null（job log 217-229） | subprocess.run 增加 `encoding="utf-8", errors="replace"`（与 F-R2-06 同族修法）；ci-linux.yml bash 侧无需改动 |
| F-R3-02 | tools/quality/ci_windows_driver.py（观测面） | minor(observability) | cmake 错误输出只进 run/ci/*-summary.json（未上传），evidence 侧 WIN-BUILD stderr 为空 → hosted 无法定位 configure 失败行 | summary JSON 并入 evidence artifact 契约目录，或驱动在 stage 失败时把 output_tail 回显 stderr（同 F-R3-01 接入侧修法族） |
| F-R3-03 | ci/run.py probe_prerequisite + ci/checks.json（登记域） | minor(false-skip) | `run/ci/build-gcc-release` 是 --build-dir 运行时产物，被路径预检误判为须预先存在的输入 → BUILD-GCC-RELEASE 两轮持续误 SKIP（轮 2 仅登记现象，本轮定位根因；连带 linux resource summary 空采） | 方案 A：run.py 预检豁免 `--build-dir`/`--*-dir` 类 flag 的紧邻参数；方案 B：checks.json 把 build-dir 补入 outputs 登记（outputs 语义为「检查产物」，build 目录可归入）。倾向 A（不稀释 outputs 语义）；均属冻结登记域，改动权在前台 |

## R27. V8-CI-010 关闭差距（对照派发单 requirements 1-6）

1. hosted 双平台 bootstrap PASS —— **已达成并连续两轮保持**（F1/F2 修复收口）。
2. 71/61 项全量执行与计数回归 —— 项数达成，但 linux BUILD-GCC-RELEASE 被
   F-R3-03 误 SKIP（未真实执行）、windows WIN-TEST-UNIT 因 configure 级联
   missing_output（测试未实跑）→ **未完全达成**。
3. 监控包装检查真实执行并产出 resource summary —— windows 侧达成（cpu/rss
   null 为宿主探针语义限制，已在 R22 说明）；linux 侧被 F-R3-03 遮蔽 → 部分。
4. candidate 管线 —— WIN-PACKAGE dumpbin skip 属登记预期，但 configure 失败
   （preset CMAKE_GENERATOR_INSTANCE）使 candidate 链路在 hosted 永远无法
   打通 → **阻塞**，需前台处理 preset 或 CI 侧生成器参数。
5. 失败归属与 FINDINGS 登记 —— 已完成（轮 2 十项维持 + 本轮 F-R3-01/02/03）。
6. 证据链与验收 —— logs/round3/ 全量留档（API trace、job log、双平台 artifact
   zip+解包、BOOTSTRAP_DIAG），trace 无凭据断言 PASS，`git status --porcelain`
   89 行与任务初始快照一致（零越界新增），unittest 231 OK。

**观察轮 3 总判定：NOT_CLOSABLE —— 剩余差距集中在 F-R3-03（误 SKIP）、
windows preset generator instance（configure 即败级联）、F-R3-01（诊断步接入
侧 encoding）；三者均为前台可修项，修复后建议一轮定向重验（linux 看
BUILD-GCC-RELEASE 实跑 + monitor JSON；windows 看 configure 通过 + junit 产出 +
BOOTSTRAP_DIAG 回填）。**

---

## R28. 修复轮 4（收口轮，SA-CI-32 本机执行；HEAD=77da09ef）

按前台四项裁决落地，全部属 CI 接入/工具链配置侧。本节为本机修复与验收记录，
hosted 定向重验待控制面触发。

### 四项修复

| # | 裁决项 | 文件 | 修法 | 自验 |
| --- | --- | --- | --- | --- |
| 1 | F-R3-03（核心） | `ci/run.py` | probe_prerequisite 路径探测循环中，outputs/绝对路径排除之后新增：仓库相对参数 `run`、`run/`、`run\` 前缀一律跳过探测（注释说明 run/ 为 AGENTS.md 定义的运行时产物目录，与 outputs 排除同语义）；其余探测语义（command[0]、prerequisite_tools、python -m、含空白参数、绝对路径、outputs、requires_monitor、platform 门控）不变 | 主仓库 7 个携带 run/ 前缀参数的检查 probe 全通过；非 run/ 不存在路径仍被拒（回归保护用例） |
| 2 | preset 硬编码 | `CMakePresets.json` + `cmake/toolchain/verify_toolchain.py` | base-msvc 移除 `CMAKE_GENERATOR_INSTANCE`（cmake 经 vswhere 自动发现 VS 实例，双端安全）；toolset `v143,host=x64,version=14.44.35207` → `v143,host=x64`（去固定 MSVC build 号；hosted 用镜像自带 v143 最新，本地用本地 VS 默认）；vendor.windows_formal/$comment/description 同步；**preset 名与 binaryDir 不变**。校验器同步两处断言保持 fail-fast：version= 键由『必须匹配』改『禁止出现』、GENERATOR_INSTANCE 由『必须存在』改『禁止硬编码』；contract.windows.toolset_version/vs_installation 保留为本地冻结安装基准记录 | 校验器实跑 `TOOLCHAIN_CONTRACT_PASS`；driver `build_dir_from_preset`/`_toolchain_from_presets` 消费面实测正常；全文件无本地私有实例路径 |
| 3 | F-R3-01 | `.github/workflows/ci-windows.yml` | 诊断步 subprocess.run 补 `encoding="utf-8", errors="replace"`（与 ci/tests/_helpers.py F-R2-06 同法）→ BOOTSTRAP_DIAG 双 null 根因消除 | YAML 解析通过；pyyaml 解析内嵌 python 体确认实参在位 |
| 4 | F-R3-02 | `.github/workflows/ci-windows.yml` | Upload public CI evidence 步 path 改多路径：`artifacts/ci/` + `run/ci/win-build-summary.json`（逐行展开；某路径缺失仅 warning，artifacts/ci/ 有内容即不触发 if-no-files-found: error）→ cmake 真实错误证据随 artifact 上传 | YAML 解析通过；test_workflow_lock 正向断言新路径在 windows evidence 上传集合 |

引用点核查（裁决要求）：ci_windows_driver.py 仅引用 preset 名与 binaryDir，
无 GENERATOR_INSTANCE/toolset version 引用；ci/checks.json 中的 run/ 路径全部
位于 outputs（探测豁免不受影响）；packaging/gen_sbom_input.py 机器路径扫描对
`C:/AstroCS/toolchains/` 白名单，preset 移除该行后无影响。dependency-lock.json
remaining_known 首条与 preset-contract.json vs_installation 为描述性/基准记录
（无机器校验其内容），未改动，列入遗留风险。

### 单测

- 新增 `ci/tests/test_run_prefix_probe.py`（4 项）：① 受试 id 自洽（7 个检查
  确携带 run/ 前缀参数）；② 主仓库真实登记 probe 全通过（mock which 放行
  prerequisite_tools）；③ fixture 端到端——command 引用不存在且未登记的
  `run/ci/...` 路径真跑 PASS（waivable=False 硬失败面，修复前同类参数
  FAIL(prerequisite)）；④ 回归保护——非 run/ 不存在路径仍 FAIL(prerequisite)
  且理由含「仓库路径不存在」。
- 因果验证：临时回退修复（stash/pop，已恢复无遗留）后新测试 2 项失败，
  修复后全绿。
- 同步：`test_workflow_lock.py::test_evidence_upload_path_matches_run_output_dir`
  按多路径逐行解析，并正向断言 F-R3-02 新路径。

### preset 本地验证

- `cmake --preset win-msvc-17.14.39-x64`：本机无 cmake（`which cmake` 无结果，
  报错原文 `cmake: 未找到命令`，exit 127），无法获得 cmake 解析报错；
  裁决预期（报 generator/VS 发现类错误而非 CMAKE_GENERATOR_INSTANCE 路径错误）
  无法在本机实证，留待 hosted windows-2022 轮。
- 结构级替代验证 15/15 PASS（logs/repair_round4.log）：JSON 可解析；preset 名、
  binaryDir、generator、architecture、SDK、CRT、ACR 全部不变；toolset
  `v143,host=x64` 无 version= pin；GENERATOR_INSTANCE 已移除；
  `cmake/toolchain/verify_toolchain.py` → `TOOLCHAIN_CONTRACT_PASS`。

### 验收逐条实测（logs/repair_round4.log 全量留档）

1. `python3 ci/validate_registry.py --registry ci/checks.json --strict` →
   verdict PASS，exit 0。
2. `python3 -m unittest discover -s ci/tests -p 'test_*.py'` → Ran 235 tests,
   OK（231 基线 + 新增 4；test_workflow_lock 同步后重跑确认）。
3. plan-only：fast=57 / linux-main=71 / linux-deep=7 / windows-main=61，
   四 profile 不变。注意 plan-only 不探测 runtime 产物，BUILD-GCC-RELEASE
   实跑解锁以单测（端到端用例）+ hosted 轮验证。
4. `git diff --stat`：本轮新增改动 = ci/run.py(+6)、CMakePresets.json(11 行±)、
   cmake/toolchain/verify_toolchain.py(27 行±)、ci-windows.yml(10 行±)、
   ci/tests/test_workflow_lock.py(19 行±) + 新增测试/证据；diff 中 AGENTS.md
   与 dist/audit zip 删除为修复轮 1-3 遗留工作区状态（非本轮产生）。
5. `python3 ci/run.py --profile linux-main --check BUILD-GCC-RELEASE --plan-only`
   → selected_count=1，BUILD-GCC-RELEASE 在计划中（不再因 run/ 前缀被预检拒）。

### hosted 定向重验预期（哪些 id 状态会变）

- **linux**：BUILD-GCC-RELEASE 由 SKIPPED(waivable)（理由「仓库路径不存在」）
  变为真实执行——PASS 或真实 FAIL；若 hosted 缺 gcc/cmake 则按
  prerequisite_tools 语义 SKIPPED（理由含工具名，可区分）。DEEP-CLANG-BUILD、
  DEEP-SAN-ASAN、DEEP-SAN-TSAN、DEEP-COV-CPP、DEEP-COV-PY 同理；linux resource
  summary 不再空采。
- **windows**：WIN-BUILD-RELEASE configure 不再 0.4s 即败（GENERATOR_INSTANCE
  指向不存在目录）→ configure/build 真实执行；WIN-TEST-UNIT 不再级联
  missing_output（junit 应真实产出）；WIN-PACKAGE-CANDIDATE 链路解锁
  （dumpbin skip 仍属登记预期）；BOOTSTRAP_DIAG 回填（report/stderr 不再双
  null）；evidence artifact 新增 run/ci/win-build-summary.json。
- **toolset 说明**：hosted v143 = windows-2022 镜像自带最新，本地 = 本地 VS
  默认；两端 compiler 实际版本可能与历史 14.44.35207 基准不同，属裁决放宽的
  预期行为，driver summary 的 preset_formal 会如实记录。

### 修复轮 4 遗留风险

1. BUILD-GCC-RELEASE 真实执行将首次在 hosted 产出 `run/ci/build-gcc-release`
   目录；其 --build-dir 中间产物未登记 outputs，detect_dirty 的豁免仅覆盖
   outputs 登记项与输出目录前缀——若 hosted 轮 FAIL(dirty)，属登记域
   （outputs 语义）问题，改动权在前台。
2. preset 放宽后 hosted 与本地 MSVC build 号可漂移（.vsconfig 组件锁不变，
   仍由 verify_toolchain 守护）；如需版本等价证明需另行固定安装面。
3. dependency-lock.json remaining_known 首条文字与 preset-contract.json
   vs_installation 记录已与 preset 现状不完全一致（描述性/基准记录，无机器
   校验），文字同步属文档域，未在本轮裁决范围。
4. cmake preset 真实解析未在本机实证（无 cmake）；结构级验证不能替代
   hosted configure 实跑。

**修复轮 4 判定：PASS_LOCAL（四项修复落地 + 本地验收全绿 + preset 结构级
15/15）；CLOSABLE_PENDING_HOSTED —— 待控制面触发一轮 hosted 定向重验
（linux BUILD-GCC-RELEASE 实跑 + monitor JSON；windows configure 通过 +
junit 产出 + BOOTSTRAP_DIAG 回填 + win-build-summary 上传）后即可关闭。**

---

# 观察轮 4（定向重验收口轮，SA-CI-32，2026-09-06 04:23Z~05:0xZ，mode=read）

观察对象：SHA `7e974087ddb108f519e91f4aedaa018a97d1e5f6`（修复轮 4 push）触发的
ci-linux.yml / ci-windows.yml runs。本段编号 R29 起。

## R29. run 终态与时长

| | linux（34011382022） | windows（34011381969） |
| --- | --- | --- |
| workflow / 触发 | ci-linux.yml / push | ci-windows.yml / push |
| job | 101427716498（ubuntu-24.04，runner 1000000567） | 101427716562（windows-2022，runner 1000000568） |
| 终态 | completed / **failure** | completed / **failure** |
| 时段（UTC，job 口径） | 04:23:39 → 04:38:14（≈14m35s，轮 3 12m11s 的增量即 BUILD-GCC-RELEASE 实跑 119.65s） | 04:23:38 → 04:27:24（≈3m46s，轮 3 2m20s 的增量即 configure+build 真实执行 82.6s + ctest 4.5s） |
| 步骤 | Checkout ✓ / Bootstrap ✓ / Select profile ✓ / Run registered checks ✗ / 诊断步 ✓ / Upload small evidence ✓ | Bootstrap ✓ / Run MSVC tests and package candidate ✗ / Validate+Upload candidate skipped / 诊断步 ✓ / Upload evidence ✓ |
| 汇总行 | `verdict=FAIL total=71 pass=48 fail=23 known_fail=0 skipped_waivable=0`（FAIL 21 + TIMEOUT 2） | `verdict=FAIL total=61 pass=32 fail=28 known_fail=0 skipped_waivable=1` |
| artifact | linux-ci-7e974087…（id 9982744581，224,281 B，215 members） | windows-ci-7e974087…（id 9982604873，159,723 B，184 members） |

## R30. 四项修复 hosted 生效判定（定向核验清单逐项）

| # | 修复项 | 判定 | hosted 证据（本轮实测） |
| --- | --- | --- | --- |
| 1 | F-R3-03：run/ 前缀预检豁免 | **生效** | BUILD-GCC-RELEASE **PASS**，duration 119.65s、exit 0、prerequisite ok、outputs_missing=[]；真实执行 cmake configure（GNU 13.3.0，build dir run/ci/build-gcc-release）+ `cmake --build -j 2` 至 **[100%] Built target astrocs**（仅既有 -Wunused-function warning）；linux `skipped_waivable` 1→0，「仓库路径不存在」假 SKIP 形态消失 |
| 2 | preset 可移植化（GENERATOR_INSTANCE 移除 + toolset 去 pin） | **生效（configure 通过）** | vswhere 自动发现 VS：configure exit 0，「-- Configuring done (20.9s) -- Generating done (0.4s)」，cl.exe = MSVC **14.44.35207**（VS Enterprise installation 17.14.37614.0，与历史基准同值）；轮 3 的 0.4s 即败形态消失；build 阶段真实编译 82.6s（exit 1，归属见 R33/F-R4-01，非 preset 回归——错误行截断问题） |
| 2b | WIN-TEST-UNIT junit 产出（F-R3-02 级联解锁） | **生效** | ctest `--preset win-rel --output-junit run/ci/win-test-junit.xml` 真实执行 4.5s、**exit 8（8 个用例真实失败）**；outputs_missing=[]（monitor JSON + win-test-summary.json + junit 三件齐），FAIL(missing_output) 类别消失 |
| 3 | F-R3-01：诊断步 subprocess 显式 utf-8 | **生效** | windows BOOTSTRAP_DIAG.json report 非 null：5/5 items ok=true（runner windows-2022/AMD64/VS present 17.14.37614.0/toolset v143/cmake 3.31.6）；`collection_error` 字段消失，UnicodeDecodeError 无；stderr_fail_payload=null 为本步 bootstrap exit 0 的正常语义（无失败载荷可填），非轮 3 的双 null 故障形态 |
| 4 | F-R3-02：evidence artifact 含 win-build-summary.json | **生效** | windows artifact 解包根含 **run/ci/win-build-summary.json**（driver 版本/task_id/stages argv/output_tail 全量留痕）；artifact 184 members（轮 3 183） |

- **DEEP-\* 5 项**（DEEP-CLANG-BUILD / DEEP-SAN-ASAN / DEEP-SAN-TSAN /
  DEEP-COV-CPP / DEEP-COV-PY）：属 linux-deep profile（V8-CI-012 触发域），
  linux-main 71 项不含；本轮 hosted 无直接执行证据。同根因解锁由
  BUILD-GCC-RELEASE 同型实跑 + test_run_prefix_probe.py 4 用例间接成立；
  linux-deep 首跑时预期不再被误 SKIP。

## R31. check 分布对照（轮 4 vs 轮 3，逐 id 闭合）

| profile | 轮 3 | 轮 4 | 移位 |
| --- | --- | --- | --- |
| linux-main（71） | 47P / 21F / 2T / 1S | 48P / 21F / 2T / **0S** | 唯一闭合：BUILD-GCC-RELEASE SKIPPED→**PASS**；FAIL 21 + TIMEOUT 2（UT-BACKEND、UT-CLI）的 id 集合与轮 3 **差集为空** |
| windows-main（61） | 32P / 27F+1mo / 1S | 32P / 28F / 1S | FAIL id 集合唯一差集：WIN-TEST-UNIT（FAIL(missing_output)→**FAIL exit 8 真实测试失败**，修复解锁后的真实失败面，非回归级联）；WIN-BUILD-RELEASE 维持 FAIL 但形态从 0.4s configure 即败→82.6s 真实编译后失败 |

- 已知归属全部维持原判（只引用）：F-R2-03 PyYAML/CONTRACT-GRAPH、
  F-R2-04 numpy/UT-IO、F-R2-05 dumpbin、F-R2-09 基线态 17+2 项、
  F-R2-10 UT-BACKEND/UT-CLI timeout（exit -9 signal 9）、UT-CPU-AVX512。
- **无新增无主失败**；两平台 verdict 仍 FAIL 的构成全部落在
  已知归属 ∪ 新 FINDINGS（R33）。

## R32. resource summary（首轮 linux 实测数字）

- **linux BUILD-GCC-RELEASE monitor JSON**（run/ci/monitor/BUILD-GCC-RELEASE.json，
  经 log 全文解析）：duration 119.588s；**samples 576**（574 非空 CPU）；
  **cpu_percent_avg 192.77% / median 200.69%**（-j 2 双线程对 4 affinity 核 ≈2 核满载）；
  **peak_rss_kb 849,076（≈829 MiB）**、peak_pss_kb 801,553、rss_start 2,452；
  io_read 2,420,399,283 B（≈2.25 GiB）/ io_write 233,043,056 B（≈222 MiB）；
  **threads_max 14**；**timed_out=false**（3500s 预算未触发，超时计数 0）。
- host_probe：cpu_affinity 4 / cpu_logical 4 / cpu_physical 2（procfs）/
  mem_total 16,766,414,848 B（16 GB）/ effective_cpu_cores 4.0 / max_workers 4。
- **windows**（host 探针语义维持轮 2/3 判定）：WIN-BUILD-RELEASE 347 samples /
  WIN-TEST-UNIT 23 samples；cpu/rss null、threads_max 0 为 Linux /proc 采样
  探针在 windows 宿主无 psutil 的既有语义（host_probe 4 核探测成功，
  非回归）；WIN-BUILD-RELEASE duration 82.469s、WIN-TEST-UNIT 4.437s。

## R33. 轮 4 新观察（新 FINDINGS）

1. **F-R4-01（major，observability）**：WIN-BUILD-RELEASE build exit 1 的
   **真实错误行未留痕**。ci_windows_driver.run_step output_tail 固定保留
   最后 25 行，MSVC `--parallel 4` 下 error 行被后续 warning/link 行冲出
   窗口——win-build-summary.json build.output_tail 25 行全为 warning C4996
   与链接进行行（0 error 行）、job log 无 error C/fatal/MSB 行、monitor JSON
   stdout_tail 仅驱动横幅。build 失败归属无法从本轮 hosted 证据定位。
   归属：tools/quality/ci_windows_driver.py（前台域）。建议：stage 失败时
   全量落盘或输出 error/warning 过滤窗口（与 F-R3-02(a) 同域二次缺口）。
2. **F-R4-02（minor，evidence-contract）**：run/ci/win-test-junit.xml 与
   win-test-summary.json 已产出但不在 evidence artifact 上传多路径内
   （当前仅 artifacts/ci/ + win-build-summary.json）→ ctest exit 8 的 8 个
   失败用例名无法离线定位。归属：.github/workflows/ci-windows.yml 上传步。
   最小修：path 追加 run/ci/win-test-junit.xml（+win-test-summary.json）。
3. **F-R4-03（info，预期内兑现）**：toolset 放宽后的版本等价改为观测证明——
   本轮 hosted 实测 VS 17.14.37614.0 / MSVC 14.44.35207 **与历史基准同值，
   零漂移**；preset 契约（禁硬编码/禁 version pin）TOOLCHAIN_CONTRACT_PASS。

## R34. findings_round4（登记口径）

- F-R4-01 / F-R4-02：新登记，归属与建议如 R33；均不改源码（观察轮纪律）。
- WIN-TEST-UNIT exit 8 的 8 用例名：**归属待 F-R4-02 修复后下一轮留痕定位**
  （是否落入 F-R2-09 基线态，现无法判定）。
- 其余全部维持轮 2/3 归属引用。

## R35. closability_vs_requirements（对照派发单六条）

| # | 派发单要求 | 判定 | 理由 |
| --- | --- | --- | --- |
| 1 | 记录 main 完整 SHA 并等待该 SHA 双平台 workflows | **MET** | HEAD=7e974087 完整 SHA 双 run（push 触发）终态/时长/步骤/汇总行全记录（R29） |
| 2 | 核验 check 数量（71/61） | **MET** | linux 71/71 全部真实执行（skipped_waivable 0，BUILD-GCC-RELEASE 实跑 PASS）；windows 61 项中 60 实跑 + 1 项 SKIPPED(waivable)（dumpbin=F-R2-05 登记预期，非误 SKIP）；missing_output 类别消失；两 profile 数量与 plan-only 基线一致 |
| 3 | 核验 resource summary | **MET** | linux 首轮产出完整 monitor 数字（R32：192.77% avg / 849 MiB peak / 14 threads / 576 采样 / 超时 0）；windows monitor JSON 产出，null 字段为宿主探针登记语义 |
| 4 | 核验 artifact digest / source manifest / candidate 内容 | **NOT_MET** | candidate zip 仍未产出：WIN-PACKAGE-CANDIDATE 维持 dumpbin skip（F-R2-05 登记预期）+ 上游 WIN-BUILD-RELEASE 真实编译失败（错误行被 F-R4-01 截断，归属链断在观测面）；三件套核验持续 NOT_EXECUTABLE（轮 2 起同态） |
| 5 | 失败创建归属明确的修复提交，不 amend | **MET** | 本轮零无主失败：21F+2T 维持已知归属引用；新观察 F-R4-01/02/03 归属明确；前台修复轮 1-4 commit 均原子且未 amend |
| 6 | 证据链与验收 | **MET** | logs/round4/ 全量留档（API trace/jobs/artifacts/双 job log/双 artifact zip+解包/git status 快照）；trace 无凭据断言 PASS；unittest 235 OK；git status 89 行与初始快照一致 |

- **收口判定：NOT_CLOSABLE（差距收敛）** —— 修复轮 4 的 CLOSABLE_PENDING_HOSTED
  四项条件全部实证生效（R30）；req1/2/3/5/6 全 MET；唯一 NOT_MET 集中在
  **req4 候选链**：WIN-BUILD-RELEASE 真实编译失败（F-R4-01 遮蔽归属）+
  WIN-TEST-UNIT 8 用例失败（F-R4-02 遮蔽用例名）+ dumpbin（F-R2-05 登记预期）。
- **最小后续动作**（前台，一个观测面修复轮 + 一轮定向重验即可闭环）：
  1. ci_windows_driver.run_step 失败时保留 error 过滤窗口或全量日志
     （F-R4-01）；ci-windows.yml 上传多路径追加 win-test-junit.xml
     （F-R4-02）；
  2. hosted 定向重验 windows：读 build 真实错误行与 8 用例名 → 归属
     （源码域修复提交 / 并入 F-R2-09 基线态裁决）；
  3. dumpbin→candidate 链按 F-R2-05 既定裁决处理后，补核验
     SHA256SUMS/BUILD_PROVENANCE/candidate 内容三件套 → V8-CI-010 关闭。

## R36. 验收与凭据纪律（本轮实测）

- `git status --porcelain`：89 行遗留与任务初始快照逐行一致（非本轮产物）+
  增量恰 3 行 ` M evidence/v8_1_ci_control/tasks/V8-CI-010/{CHANGED_FILES.txt,
  HOSTED_RUN_REPORT.md,TASK_RESULT.json}`——全部落在派发允许域
  evidence/.../V8-CI-010/** 内；logs/** 按 .gitignore 不入 status；零 git
  写操作（终态快照留档 logs/round4/git_status_after_round4.txt）。
- `python3 -m unittest discover -s ci/tests -p 'test_*.py'`：**Ran 235 tests
  in 26.072s — OK**（两 run 终态后实测）。
- 凭据：token 仅 `git credential fill` 内联进程内存；全部 curl --max-time 15；
  round4/ 落盘前后全量 grep ghp_/github_pat_/ghs_/gho_/Authorization:/
  password= 无命中；runner 自掩码 `basic ***` 2 处 ×2 log 统一 [REDACTED] 化。

**观察轮 4 判定：FIX_ROUND4_ALL_EFFECTIVE —— 四项修复 hosted 全部生效验证
（BUILD-GCC-RELEASE 实跑 PASS + linux resource 数字落地；windows configure
通过 + junit 产出；BOOTSTRAP_DIAG 回填；win-build-summary 上传）；剩余失败
全部已知归属或新登记观测面缺口。V8-CI-010 未达关闭条件（req4 候选链 +
windows 失败定位依赖 F-R4-01/02 修复后定向重验）；按 R35 最小后续动作
推进后即可关闭。**

## R37. 修复轮 5（F-R4-01 / F-R4-02 前台裁决执行，@ HEAD 7e974087，mode=write，git 只读纪律不变）

**1. F-R4-01 修复（tools/quality/ci_windows_driver.py，输出 schema 向后兼容）**
- 常量新增 `ERROR_LINE_RE`（`error C\d+|error LNK\d+|fatal error|: error |/error MSB\d+|LINK : fatal`，IGNORECASE）
  与 `ERROR_LINES_CAP = 50`；新增纯函数 `collect_error_lines(lines)`（保序过滤、cap 50）。
- `run_step`：除 output_tail 外新增 **error_lines** 字段——失败（exit≠0）时对全量输出按上述正则过滤收集
  （cap 50 保序），**output_tail 语义不变**（仍最后 25 行）；成功（exit 0）时 `error_lines=[]`；
  FileNotFoundError 分支同样补 `error_lines: []`。
- `run_flow`：stage_res 透传 `error_lines`（win-build-summary.json stages[] 逐级留痕）；ci_result.schema.json
  additionalProperties=true，schema 兼容零改动。

**2. F-R4-02 修复（.github/workflows/ci-windows.yml evidence 上传步）**
- driver 实际 junit 路径确认：`DEFAULT_JUNIT = "run/ci/win-test-junit.xml"`（checks.json WIN-TEST-UNIT
  command `--junit run/ci/win-test-junit.xml`、outputs 三件套契约一致）。
- 上传多路径追加 `run/ci/win-test-junit.xml` + `run/ci/win-test-summary.json`（该 summary 为同检查
  outputs 契约第二件，ctest 失败上下文一并离线可读）；`artifacts/ci/`、`run/ci/win-build-summary.json` 原路径不动。

**3. 单测（ci/tests）**
- test_windows_ci.py：+4 新用例——`test_run_step_collects_error_lines_on_failure`（9 行混合输出中收 5 行
  error：C2065/LNK2019/C1083/MSB1009/LNK1104，warning 与噪声行不收、保序）、
  `test_run_step_error_lines_cap_50`（60 行 error 截断至 50 保序）、`test_run_step_success_has_empty_error_lines`、
  `test_collect_error_lines_pure_function`；`test_run_step_captures_output_and_exit_code` 补
  error_lines==[] 断言。
- test_workflow_lock.py：`test_evidence_upload_path_matches_run_output_dir` 补两条正向断言
  （win-test-junit.xml / win-test-summary.json，注明 F-R4-02 与 checks.json outputs 契约）。

**4. 验收实测（logs/repair_round5.log 全文留档）**
- `validate_registry --strict --registry ci/checks.json`：**verdict PASS，error_count 0，checks 80，exit 0**。
- `python3 -m unittest discover -s ci/tests -t ci/tests`：**Ran 239 tests — OK**（235 + 新增 4）。
- plan-only 四口径不变：**fast 57 / linux-main 71 / linux-deep 7 / windows-main 61**。
- `git diff --stat`：ci_windows_driver.py(+20) / ci-windows.yml(+4) / test_windows_ci.py(+57) /
  test_workflow_lock.py(+6) / evidence/**V8-CI-010/**(三件更新) —— 全部在派发允许域。
- 仓库根 tests/（非本轮域）基线对照：45 ERROR（loader/setUpClass 环境性存量）+ 4 FAIL
  （version/thread_budget/drizzle 存量门）与本轮改动零交集；PRODUCTION_EXECUTION_INVENTORY.csv
  工作区漂移为 arch 库存再生成测试对 HEAD 库存滞后的确定性重写（HEAD 基线复跑同样产生），非本轮产物。

**5. hosted 重跑预期（windows 定向重验读数能力）**
- F-R4-01：WIN-BUILD-RELEASE 失败时 win-build-summary.json 的 stages[build].error_lines 将直接给出
  `error Cxxxx / LNKxxxx / fatal error / MSBxxxx` 行（cap 50 保序），不再依赖被 --parallel warning 冲出的
  output_tail 25 行窗口——编译失败归属（源码域修复 vs F-R2-09 基线态）可判定；成功时字段空列表、
  summary 体积零影响。
- F-R4-02：windows artifact 将含 run/ci/win-test-junit.xml（JUnit XML，`<testcase name=… classname=…>` 离线
  可读）+ win-test-summary.json，WIN-TEST-UNIT exit 8 的 8 个失败用例名可离线定位并对照 F-R2-09 裁决。

**6. 工作区事件留档（round3 遗留 stash 一次性处置，见 logs/repair_round5.log §7）**
- round3 未提交的 AGENTS.md 工作区改动被项目负责人权威更新取代（现盘=HEAD：含「节点与角色」节、
  无 subagent 派发条目）；stash 版已备份 run/AGENTS_stash_backup_round3.md 后弃用。
- csv 漂移归因与保留理由如上 §4；stash 已 drop，全部内容经 checkout 恢复或同源证实。

**修复轮 5 判定：PASS_LOCAL —— 两项前台裁决修复全部落地、单测 239 OK、注册表 strict PASS、
plan-only 四口径不变、diff 范围合规；hosted 定向重验（windows build 错误行 + junit 用例名）待
控制面触发后即可完成归属判定。**
