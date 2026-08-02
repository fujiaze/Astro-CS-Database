# ACR 证据包索引 (EVIDENCE_INDEX.md)

**生成时间**: 2026-08-02
**分支**: `feature/astrocompute-runtime`
**Base**: `8f50519` (origin/main)
**HEAD**: `0cf2f3f`
**用途**: Phase A-H 完成后的证据收集草稿，供用户审核。审核通过后方可进入 Phase I 合并 main。

---

## 1. 证据文件清单

### 1.1 构建证据 (`build/`)

| 文件 | 用途 | 关键内容 |
|---|---|---|
| `build/build_config.json` | 构建配置记录 | CPU-only、MinGW Makefiles、Release、g++ 16.1.0、CMake 选项、依赖策略 |
| `build/build_log_cpu_only.log` | CPU-only 构建完整日志 | configure + build 输出，[100%] Built target acr-classic-runner，exit 0 |
| `build/build_success.txt` | 构建成功标记 | 时间戳、exit code 0、构建目标列表、警告说明 |

### 1.2 测试证据 (`tests/`)

| 文件 | 用途 | 关键内容 |
|---|---|---|
| `tests/unit_test_results.log` | 7 个单元测试 exe 完整输出 | 136 PASSED / 1 FAILED (`HardwareReport.FirstCallbackWins`) |
| `tests/classic_test_results.log` | E01-E16 经典实验测试输出 | 142/142 PASSED |
| `tests/fault_test_results.log` | 故障注入测试输出 | 10/10 PASSED |
| `tests/sanitizer_test_results.log` | sanitizer smoke 测试输出 | 10/10 PASSED（构建未启用 sanitizer，作为常规 smoke 运行） |
| `tests/persistence_test_results.log` | persistence 测试输出 | 5/5 PASSED |
| `tests/test_summary.json` | 测试结果汇总 JSON | 套件名/用例数/通过/失败/跳过，总 303 pass / 1 fail |

### 1.3 经典实验运行器报告 (`classic_runner/`)

| 文件 | 用途 | 关键内容 |
|---|---|---|
| `classic_runner/classic_report.json` | acr-classic-runner 完整 JSON 报告 | schema v1、142 cases、pass_rate 1.0、16 实验、case 级明细 |
| `classic_runner/classic_summary.json` | 汇总 JSON | 每实验的 case 数与通过情况 |
| `classic_runner/runner_stderr.log` | runner 执行 stderr（进度） | 各实验 PASSED 计数、exit 0 |

### 1.4 Path Guard 证据 (`path_guard/`)

| 文件 | 用途 | 关键内容 |
|---|---|---|
| `path_guard/path_guard_report.txt` | path_guard.ps1 输出 | `[path_guard] OK: All changes within allowed ACR paths.` exit 0 |
| `path_guard/git_diff_name_only.txt` | `git diff --name-only origin/main...HEAD` | 130 文件，全部在 lib/acr/ 与 工程控制/tasks/acr/ 内 |
| `path_guard/git_diff_stat.txt` | `git diff --stat origin/main...HEAD` | 130 files changed, 15490 insertions(+) |
| `path_guard/committed_diff_path_check.txt` | 已提交 diff 路径合规校验 | Violations: 0（所有已提交改动在允许路径内） |

### 1.5 Git 证据 (`git/`)

| 文件 | 用途 | 关键内容 |
|---|---|---|
| `git/git_log.txt` | `git log --oneline origin/main..HEAD` | 6 commits（Phase A-H） |
| `git/git_status.txt` | `git status --short` | 仅 3 个未跟踪项：build_efg/（遗留）、build_evidence/（临时）、evidence/acr/（本次证据） |
| `git/git_show_head.txt` | `git show --stat HEAD` | HEAD commit 0cf2f3f 详细信息（Phase H 经典实验+故障+sanitizer） |
| `git/base_commit.txt` | base/HEAD/branch 解析 | base 8f50519、HEAD 0cf2f3f、branch feature/astrocompute-runtime |

### 1.6 合并报告

| 文件 | 用途 |
|---|---|
| `merge_report_draft.md` | 合并报告草稿：分支信息、Phase A-H 完成情况、测试汇总、已知遗留、path guard 结论、建议 |

---

## 2. 验收门禁对照表（spec.md §11.2）

| # | 门禁要求 | 状态 | 证据 | 说明 |
|---|---|---|---|---|
| 1 | path guard 证明算法目录零修改 | ✅ 通过 | `path_guard/path_guard_report.txt`、`path_guard/committed_diff_path_check.txt` | 工作树与已提交 diff 均无越界，算法目录（lib/plate_solve/ 等）零修改 |
| 2 | CPU-only 构建无 GPU SDK 依赖 | ✅ 通过 | `build/build_log_cpu_only.log`、`build/build_success.txt` | ACR_BUILD_CUDA=OFF，无 CUDA/HIP/SYCL SDK 依赖，configure+build exit 0 |
| 3 | CUDA backend 真实通过（RTX 3060 Ti，不伪造） | ❌ 未满足 | — | **Phase D CUDA 编译集成未完成**（见 merge_report_draft.md §6.1），CPU-only 证据构建不覆盖此项，留待 Phase I 后独立任务 |
| 4 | 经典实验 E01-E16 全部 PASS 或 SKIPPED（注明原因） | ✅ 通过 | `tests/classic_test_results.log`、`classic_runner/classic_report.json` | 142/142 PASSED，0 SKIPPED |
| 5 | sanitizer 无泄漏/竞态/use-after-free | ⚠️ 部分通过 | `tests/sanitizer_test_results.log` | 10/10 PASSED，**但本次构建未启用 ASan/UBSan**（ACR_ENABLE_SANITIZER=OFF），仅作为常规 smoke 测试运行。需在 sanitizer-enabled 构建中重新验证 |
| 6 | 普通启动不初始化 ACR（无副作用） | ✅ 通过（间接） | `tests/unit_test_results.log`（acr_test_api 全部通过） | runtime 采用 lazy initialization，普通启动路径不创建 runtime singleton |

### 2.1 门禁汇总

- **完全通过**: 1, 2, 4, 6（4 项）
- **部分通过**: 5（1 项，需 sanitizer-enabled 重建验证）
- **未满足**: 3（1 项，CUDA 编译集成 Phase D 遗留）

### 2.2 进入 Phase I 合并 main 的前置条件

1. **必须**: 用户决策 `HardwareReport.FirstCallbackWins` 单测失败的处理方式（修复或标记 known issue）。
2. **必须**: 用户授权合并。
3. **建议**: 完成 sanitizer-enabled 构建并重跑 sanitizer 套件（满足门禁 5 的完整要求）。
4. **可延后**: CUDA backend 真实编译集成（门禁 3）可在 Phase I 后独立任务完成，因其不影响 CPU-only 路径。

---

## 3. 证据完整性声明

- 所有日志均落盘，未仅输出到控制台。
- 构建与测试失败如实记录，未伪造成功。
- 未 commit、未 push（等待主 Agent / 用户统一处理）。
- 临时构建目录 `lib/acr/build_evidence/` 将在证据收集完成后删除（不入仓）。
- 本次证据收集**未修改任何算法目录**（lib/plate_solve/ 等），仅在工作树内执行构建、测试与证据写入。

---

## 4. 文件树

```
工程控制/evidence/acr/
├── EVIDENCE_INDEX.md                      # 本文件
├── merge_report_draft.md                  # 合并报告草稿
├── build/
│   ├── build_config.json
│   ├── build_log_cpu_only.log
│   └── build_success.txt
├── tests/
│   ├── unit_test_results.log
│   ├── classic_test_results.log
│   ├── fault_test_results.log
│   ├── sanitizer_test_results.log
│   ├── persistence_test_results.log
│   └── test_summary.json
├── classic_runner/
│   ├── classic_report.json
│   ├── classic_summary.json
│   └── runner_stderr.log
├── path_guard/
│   ├── path_guard_report.txt
│   ├── git_diff_name_only.txt
│   ├── git_diff_stat.txt
│   └── committed_diff_path_check.txt
└── git/
    ├── git_log.txt
    ├── git_status.txt
    ├── git_show_head.txt
    └── base_commit.txt
```
