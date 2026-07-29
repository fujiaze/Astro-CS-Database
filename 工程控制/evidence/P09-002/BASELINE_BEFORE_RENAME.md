# P09-002 修改前基线 — 命名与检测调用现状

## 目的

记录实施 `INTERNAL_DETECTION_SHARED_EXPORT` 命名修正前的源码事实状态，用于事后回归对比。

## 1. PlateSolve 入口与检测调用计数

### C ABI 入口（lib/plate_solve/cpp/ipv/src/ipv_entry.cpp）

| 入口函数 | 行号 | 路径 | sdet_detect_ex 调用次数 |
|---|---|---|---|
| `ipv_solve_create` | 237 | 实例创建 | 0 |
| `ipv_solve_destroy` | 249 | 实例销毁 | 0 |
| `ipv_solve` | 310-339 | 旧路径（文件输入） | 1（间接，经 ipv_select） |
| `ipv_solve_from_memory` | 342-379 | 旧路径（内存输入） | 1（间接，经 ipv_select_from_memory） |
| `ipv_solve_from_detections_v1` | 473-511 | 路径 A（外部 detections） | **0**（跳过 sdet_detect_ex） |
| `ipv_solve_from_memory_with_callback` | 514-552 | **生产路径**（路径 B） | 1（间接，经 ipv_select_from_memory_with_callback） |

### 检测函数精确名称

`sdet_detect_ex`（C ABI 导出）
- 声明：`lib/star_detector/include/star_detector.h:49`
- 实现：`lib/star_detector/src/sdet_api.cpp:1883`

### 生产路径调用链

```
Orchestrator::run_stage_platesolve                              [orchestrator.cpp:1828]
  └─ ipv_solve_from_memory_with_callback                        [ipv_entry.cpp:514]
       └─ do_solve_from_memory_with_callback_impl               [ipv_entry.cpp:428]
            └─ ipv::IPVSolver::solve_from_memory_with_callback  [ipv_solver.cpp]
                 └─ ipv_select_from_memory_with_callback        [ipv_select.cpp:1481]
                      └─ sdet_detect_ex  (1次)                 [ipv_select.cpp:1558]
                      └─ callback(detections, n, user_data)    [ipv_select.cpp:1597]
```

### 710 帧既有证据（不重跑）

| 任务 | 关键证据 | 位置 |
|---|---|---|
| P02-003 | A/B 全量对比，成功率/RMS/n_pairs 完全一致 | engineering/evidence/P02-003/ |
| P02-007 | 730 calls / 710 frames + 20 repeats = 1.028 次/帧 | engineering/evidence/P02-007/TASK_REPORT.md:36-38 |
| P02-007 | 5/5 非退化门限 PASS（success_rate / RMS median / RMS p99 / n_pairs median / duration median） | engineering/evidence/P02-007/TASK_REPORT.md:60-69 |

## 2. 共享检测机制现状

### callback 类型

- C ABI 端：`IpvDetectionCallback`（lib/plate_solve/cpp/ipv/include/ipv_api.h:136-140）
- C++ 内部端：`DetectionSinkFn`（lib/plate_solve/cpp/ipv/include/ipv_select.h:148-152）

### callback 注册与触发

- 注册：`Orchestrator` 在 `run_stage_platesolve` 中将 `path_b_detection_callback` 作为 callback 传入（orchestrator.cpp:1959-1962）
- 触发：PlateSolve 在 `sdet_detect_ex` 完成后、选星之前同步调用（ipv_select.cpp:1583-1600）
- 上下文：`PathBCallbackCtx`（orchestrator.cpp:1791-1795），持有 `detections_buf`（FLOAT64 [N*6]）

### star_det 块流向

```
PLATESOLVE (生产者, orchestrator.cpp:2055-2099)
  └─ fn_add_block("star_det", FLOAT32 [N,4]: x,y,flux,mag)
       └─ 描述: "星点检测结果 (路径B callback 导出): x,y,flux,mag"

PSF (消费者, orchestrator.cpp:2176-2195)
  └─ fn_get_block("star_det") -> dims[0]=N
       └─ 直接读 x/y 用于 dpsf_fit_batch (不再调用 sdet_detect_ex)
```

## 3. 命名现状清单

### capabilities 字符串（lib/orchestrator/cpp/src/cli_command.cpp:1692）

修改前：
```json
{"name":"ipv_solver","version":"<ver>","capabilities":["solve_from_memory","solve_from_detections_v1","solve_from_memory_with_callback"]}
```

未声明共享检测/路径 B/内部检测语义。**未出现** `INTERNAL_DETECTION_SHARED_EXPORT`、`shared detection`、`path_b` 等任何字符串。

### 源码中的命名变体（修改前）

| 命名变体 | 出现位置（关键代表） | 类型 |
|---|---|---|
| `路径B` / `路径 B` | orchestrator.cpp:1786,1797,1817,1928,1955,1958,2050-2053,2077,2079 等 | 注释 + 日志 |
| `路径B` / `路径 B` | ipv_entry.cpp:382,391,427,472,513 | 注释 |
| `路径B` / `路径 B` | ipv_select.cpp:1473,1510,1580,1584 | 注释 + 日志 |
| `路径B` / `路径 B` | ipv_solver.cpp:1420,1453,1480 | 注释 + 日志 |
| `PathBCallbackCtx` | orchestrator.cpp:1791 | 类型名（C++ 标识符） |
| `path_b_detection_callback` | orchestrator.cpp:1799 | 函数名（C++ 标识符） |
| `path_b_results.json` | engineering/evidence/P02-003/、P02-007/ | 文件名（外部数据） |
| `UPSTREAM_SHARED_DETECTIONS` | engineering/evidence/P02-003/ADR.md 等 | v1.1 决策命名 |
| `MERGE_PATH_B` | engineering/evidence/P02-003/ | v1.1 路径决策名 |
| `PRESERVE_INTERNAL_DETECTION_EXPORT` | engineering/control/DECISION_REGISTER.md:13 | v1.1 备选命名 |
| `INTERNAL_DETECTION_SHARED_EXPORT` | **未出现在源码** | v1.2 目标命名（仅文档） |

### 修改范围（最小化）

**改（人类可读文本）**：
- `cli_command.cpp` capabilities：追加 `"internal_detection_shared_export"`
- `orchestrator.cpp` 日志/注释：`路径B` → `INTERNAL_DETECTION_SHARED_EXPORT`
- `ipv_entry.cpp` 注释：`路径B` → `INTERNAL_DETECTION_SHARED_EXPORT`
- `ipv_select.cpp` 日志/注释：`路径B` → `INTERNAL_DETECTION_SHARED_EXPORT`
- `ipv_solver.cpp` 日志/注释：`路径B` → `INTERNAL_DETECTION_SHARED_EXPORT`

**不改（保留兼容性）**：
- C ABI 函数名：`ipv_solve_from_memory_with_callback` 等（保持 ABI 稳定）
- C++ 类型名：`PathBCallbackCtx`、`path_b_detection_callback`（保持内部符号稳定）
- C ABI callback 类型：`IpvDetectionCallback`、`DetectionSinkFn`
- 历史数据文件：`path_b_results.json` 等已落盘 JSON（保持历史可读）
- 旧 evidence 目录文本：`engineering/evidence/P02-xxx/`（P09-001 已锁定为不可篡改）

## 4. 共识事实

- v1.1 HEAD: `ed145a7 docs: 补充项目 README + 生成 v1.1 审计包`
- 710 帧 A/B 已通过（P02-003 + P02-007 双重验证）
- 生产路径已统一为路径 B（callback 导出），每帧 `sdet_detect_ex` 调用 1.0 次
- PSF 通过 `star_det` 块共享 PLATESOLVE 的检测结果，不再二次检测

## 5. 修改前事实 SHA-256

待 T3 完成后补录"修改前" commit hash（即 v1.1 HEAD）与"修改后" commit hash 差异。

- 修改前 HEAD: ed145a7
- 修改前 lib/orchestrator/cpp/src/cli_command.cpp SHA-256: 待 T3 计算并填入 TEST_REPORT
- 修改前 lib/orchestrator/cpp/src/orchestrator.cpp SHA-256: 待 T3 计算并填入 TEST_REPORT
