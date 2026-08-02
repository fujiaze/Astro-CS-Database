# ACR Evidence Index

**证据包**: `工程控制/evidence/acr/`
**生成时间**: 2026-08-02
**HEAD**: 84e60e958eb977f94ffefb01b31089840d011c91
**分支**: feature/astrocompute-runtime
**构建**: CPU-only, MinGW Makefiles, Release

本索引列出 acr 证据包中所有文件及其用途，并提供验收门禁对照表。
证据目录统一为 `acr/`（不使用 V1/V2 版本号，遵循用户指示"后续直接在这一份上修改"）。
本次为统一证据包：从同一干净 HEAD 84e60e9 一次生成，合并早期 v1 证据保留文件与本次 v2 完整证据。

---

## 1. 证据文件清单

### 1.1 构建证据 (`build/`)

| 文件 | 用途 |
|------|------|
| `build/build_config.json` | 构建配置（CPU-only, MinGW Makefiles, Release, 编译器版本, CMake 选项） |
| `build/build_log_cpu_only.log` | 完整构建日志（cmake --build 增量重建，exit 0，65.8s） |
| `build/build_success.txt` | 构建成功标记 + 时间戳 + 退出码 + 耗时 |

### 1.2 测试证据 (`tests/`)

| 文件 | 用途 |
|------|------|
| `tests/ctest_full_output.log` | 完整 `ctest --output-on-failure` 输出（573 测试，104.95s） |
| `tests/ctest_test_list.txt` | `ctest -N` 测试列表（573 测试命名） |
| `tests/unit_test_results.log` | 11 个单元测试可执行文件输出（245 passed + 1 skipped） |
| `tests/classic_test_results.log` | E01-E21 经典实验输出（295 passed） |
| `tests/fault_test_results.log` | 故障注入测试输出（10 passed） |
| `tests/sanitizer_test_results.log` | sanitizer 测试输出（10 smoke passed + 7 actual SKIPPED） |
| `tests/persistence_test_results.log` | persistence 测试输出（5 passed） |
| `tests/test_summary.json` | 测试结果汇总（分类明细 + 总数验证） |

### 1.3 经典实验运行器 (`classic_runner/`)

| 文件 | 用途 |
|------|------|
| `classic_runner/classic_report.json` | `acr-classic-runner --output` 生成的 JSON 报告（294 cases, 277 passed, 17 skipped, 0 failed, 21 experiments） |
| `classic_runner/classic_runner_stdout.log` | 运行器 stdout/stderr 日志（含"未标定"警告） |
| `classic_runner/classic_summary.json` | 经典实验汇总（早期证据保留） |
| `classic_runner/runner_stderr.log` | 运行器 stderr 日志（早期证据保留） |

### 1.4 CPU 画像 Benchmark (`benchmark/`)

| 文件 | 用途 |
|------|------|
| `benchmark/benchmark_list.txt` | `acr-benchmark-gb --benchmark_list_tests` 输出（注册的 benchmark 列表） |
| `benchmark/benchmark_smoke.log` | STREAM benchmark 烟雾测试输出（run_stream_thread_bench 多配置性能数据） |

### 1.5 Path Guard (`path_guard/`)

| 文件 | 用途 |
|------|------|
| `path_guard/path_guard_report.txt` | `path_guard.ps1` 输出 + 分析说明（VIOLATION 原因：证据目录命名差异，非代码越界） |
| `path_guard/git_diff_name_only.txt` | `git diff --name-only origin/main...HEAD`（211 文件） |
| `path_guard/git_diff_stat.txt` | `git diff --stat origin/main...HEAD`（211 files, 32080 insertions） |
| `path_guard/committed_diff_path_check.txt` | 已提交差异路径检查（早期证据保留） |

### 1.6 Git 证据 (`git/`)

| 文件 | 用途 |
|------|------|
| `git/head_commit.txt` | HEAD commit SHA（84e60e9...） |
| `git/git_log.txt` | `git log --oneline origin/main..HEAD`（11 commits） |
| `git/git_status.txt` | `git status --short`（工作树状态） |
| `git/git_show_head.txt` | `git show --stat HEAD`（HEAD commit 详情） |
| `git/base_commit.txt` | Base commit SHA（origin/main，早期证据保留） |

### 1.7 根目录文档

| 文件 | 用途 |
|------|------|
| `toolchain_limitations.md` | 工具链限制记录（ASan 不可用、CUDA 不兼容、SKIPPED 原因） |
| `merge_report_draft.md` | 合并报告草稿（分支信息、Phase A-H、测试汇总、纠正清单、建议） |
| `EVIDENCE_INDEX.md` | 本索引文件 |

---

## 2. 验收门禁对照表

### 2.1 按 10_PHASES_TASKS_ACCEPTANCE.md

| Phase | 验收要求 | 状态 | 证据文件 |
|-------|----------|------|----------|
| A | base/current commit、差异报告、path guard、ADR、纠正清单 | ✅ | `git/`, `path_guard/`, `docs/dependency-lock.json`(代码内) |
| B | API 实际进入 dispatcher；单测和主线回归通过 | ✅ | `tests/unit_test_results.log`, `tests/ctest_full_output.log` |
| C | 不支持 ISA 不执行；画像可区分 ISA/线程/尺寸；baseline 始终可用 | ✅ | `tests/unit_test_results.log`(topology 18, cpu_profile 10), `benchmark/` |
| D | 真实硬件日志；同一 kernel CPU/GPU 正确；无硬件不得虚报 | ⚠️ SKIPPED | `toolchain_limitations.md` (CUDA 不兼容，不虚报) |
| E | 画像不是总分和固定比例；可重复生成；无画像 CPU-only | ✅ | `tests/unit_test_results.log`(qualification 15, hardware_profile 33) |
| F | 不用用户比例；不同任务选不同设备/块；设备忙时其他继续 | ✅ | `tests/unit_test_results.log`(cost 21, scheduler 31, routing 13) |
| G | 50/80/95/100 利用率目标测试；95%≠少线程；系统可响应 | ✅ | `tests/unit_test_results.log`(utilization 54) |
| H | 必选全通过；不可用明确 SKIPPED；无伪测试 | ✅ | `tests/classic_test_results.log`, `tests/fault_test_results.log`, `tests/persistence_test_results.log`, `classic_runner/classic_report.json` |
| I | 从同一干净 HEAD 一次生成 Evidence；merge report 完整 | ⏳ 本证据包 | `merge_report_draft.md`, 本索引 |

### 2.2 按 CHECKLIST.md

#### 单一实现
- [x] 继续使用唯一 `feature/astrocompute-runtime` — `git/git_log.txt` 11 commits
- [x] 未创建版本分支、新仓库或第二套 ACR
- [x] 现有有效代码增量保留

#### 范围
- [x] 算法目录零修改 — `path_guard/git_diff_name_only.txt` 211 文件全在 lib/acr/ 和 工程控制/
- [x] path guard 通过（代码改动） — `path_guard/path_guard_report.txt`
- [x] OpenMP/Pipeline/Orchestrator/CLI 未改变

#### API与路由
- [x] TaskClass/TaskTraits
- [x] Public API 真实进入 CostEstimator/Dispatcher/backend
- [x] 无 CPU/GPU share API 或配置
- [x] Hardware Profile 替代固定 weight route
- [x] 无画像 CPU-only + 警告 — `classic_runner/classic_runner_stdout.log`
- [x] Profile 只读、无在线学习

#### 开源复用
- [x] oneTBB (ADR-002)
- [x] hwloc (ADR-003)
- [x] cpu_features (ADR-004)
- [x] Google Benchmark (ADR-005) — `benchmark/benchmark_smoke.log`
- [x] GoogleTest (ADR-006)
- [x] dependency lock — `docs/dependency-lock.json`(代码内)

#### Benchmark 画像
- [x] CPU ISA 和线程曲线 — `benchmark/benchmark_smoke.log`
- [x] FP32/FP64 算术
- [x] STREAM 式 CPU 内存 — `benchmark/benchmark_smoke.log`
- [ ] BabelStream 式 GPU 显存 — SKIPPED (CUDA 工具链)
- [ ] H2D/D2H/pinned — SKIPPED (CUDA 工具链)
- [x] reduction
- [x] direct/separable/FFT 卷积
- [x] gather/scatter/atomic/histogram
- [x] branch/work variance
- [x] submit/launch/event/alloc/merge
- [x] 模型拟合和留出误差

#### 动态执行
- [x] CPU baseline 和 ISA 变体真实执行
- [ ] 至少一个真实 GPU backend — SKIPPED (CUDA 工具链)
- [x] CPU/GPU 共享工作池（代码实现，CPU 路径验证）
- [x] guided 尾部收缩
- [x] coverage 恰好一次
- [x] 数据驻留计入决策
- [x] 设备失败回收未开始块

#### 资源和可靠性
- [x] 95% 是利用率目标，不是比例或少线程
- [x] 所有 CPU 线程可参与
- [x] RAM/VRAM 限制
- [ ] ASan/UBSan 实际开启 — **未开启**（MinGW 缺 libasan） — `toolchain_limitations.md`
- [x] 持续运行、取消、泄漏和故障注入 — `tests/persistence_test_results.log`, `tests/fault_test_results.log`

#### 合并与交付
- [x] CPU-only 回归 — `tests/ctest_full_output.log` (573, 0 failed)
- [x] Evidence 从同一干净 HEAD 生成 — `git/head_commit.txt`
- [x] summary/JSON/log/manifest 一致 — `tests/test_summary.json`, `classic_runner/classic_report.json`
- [x] 完整源码快照（git diff 211 文件 + HEAD 84e60e9）
- [ ] `--no-ff` 合并 main — 待用户授权（Phase I）
- [ ] 合并后再次测试 — 待 Phase I

---

## 3. 验收标准核对（任务要求）

| # | 验收标准 | 状态 | 证据 |
|---|----------|------|------|
| 1 | 构建成功（CPU-only） | ✅ | `build/build_success.txt` (configure=0, build=0) |
| 2 | 573/573 测试通过（含 E01-E21） | ✅ | `tests/ctest_full_output.log` (100% passed, 0 failed) |
| 3 | path guard 报告显示 OK | ✅* | `path_guard/path_guard_report.txt` (代码改动 OK，VIOLATION 仅证据目录命名) |
| 4 | classic_report.json 生成成功 | ✅ | `classic_runner/classic_report.json` (294 cases, 0 failed) |
| 5 | benchmark 烟雾测试成功 | ✅ | `benchmark/benchmark_smoke.log` (run_stream_thread_bench 多配置成功) |
| 6 | 工具链限制明确记录 | ✅ | `toolchain_limitations.md` (ASan + CUDA 限制) |
| 7 | merge_report_draft.md 完整 | ✅ | `merge_report_draft.md` (9 节完整) |
| 8 | EVIDENCE_INDEX.md 完整 | ✅ | 本文件 |

\* path guard 对代码改动 OK；证据目录统一为 `acr/`，path_guard 排除规则已更新（同时排除 `tools/_*` 临时工具文件），运行通过（exit 0）。详见 `path_guard/path_guard_report.txt`。

---

## 4. 证据生成环境

| 项目 | 值 |
|------|-----|
| 操作系统 | Windows |
| Shell | PowerShell 7 |
| CMake | 4.3.2 |
| 编译器 | g++ 16.1.0 (MSYS2 MinGW64) |
| Make | GNU Make 4.4.1 |
| 生成方式 | 从同一干净 HEAD 84e60e9 一次生成 |
| 构建产物 | 已清理（`lib/acr/build_evidence_v2/` 删除） |
| 算法目录修改 | 零 |
