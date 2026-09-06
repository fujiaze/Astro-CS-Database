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
