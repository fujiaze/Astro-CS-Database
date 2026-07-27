# P09-002 TASK_REPORT — 确认共享检测主线与统一命名

- 任务: P09-002
- 阶段: P09
- Gate: G9
- 依赖: P09-001
- 参考: `engineering_v1.2/docs/02_PLATESOLVE_SHARED_DETECTION_MAINLINE_STATUS.md`
- 执行日期: 2026-07-27
- 执行者: ACSD 自治 Agent (P09-002)

## 目标

源码和运行时证明每帧只检测一次；确认主线提交；将能力/日志命名改为 `INTERNAL_DETECTION_SHARED_EXPORT`。

## 执行步骤与结果

### 1. 核对 PlateSolve 源码 — 检测调用计数

源码审查（详见 `BASELINE_BEFORE_RENAME.md`）确认：

| 入口函数 | 路径 | sdet_detect_ex 调用次数 |
|---|---|---|
| `ipv_solve` | 旧路径，文件输入 | 1 |
| `ipv_solve_from_memory` | 旧路径，内存输入 | 1 |
| `ipv_solve_from_detections_v1` | 路径 A | 0（跳过） |
| `ipv_solve_from_memory_with_callback` | 生产路径（路径 B / INTERNAL_DETECTION_SHARED_EXPORT） | 1 |

- 检测函数精确名称: `sdet_detect_ex`（声明 `lib/star_detector/include/star_detector.h:49`，实现 `lib/star_detector/src/sdet_api.cpp:1883`）
- 生产路径调用链:
  ```
  Orchestrator::run_stage_platesolve                [orchestrator.cpp:1828]
    └─ ipv_solve_from_memory_with_callback          [ipv_entry.cpp:514]
         └─ ipv::IPVSolver::solve_from_memory_with_callback  [ipv_solver.cpp:1427]
              └─ ipv_select_from_memory_with_callback          [ipv_select.cpp:1482]
                   └─ sdet_detect_ex  (1次)                   [ipv_select.cpp:1558]
                   └─ callback(det_v1.data(), det_count, user_data)  [ipv_select.cpp:1597]
  ```

**结论**: PASS — 每帧 `sdet_detect_ex` 恰好调用 1 次。

### 2. 确认主线提交 — 710 帧 A/B 既有证据

P09-002 不重跑 710 帧测试（按 P09-001 锁定基线规则），引用既有证据：

| 既有任务 | 证据位置 | 关键结论 |
|---|---|---|
| P02-003 | `engineering/evidence/P02-003/` | A/B 全量对比 710 帧，成功率/RMS/n_pairs 完全一致 |
| P02-007 | `engineering/evidence/P02-007/` | 730 calls / (710 frames + 20 repeats) = 1.028 次/帧 (≈1.0)；5/5 非退化门限 PASS |

P02-007 `TASK_REPORT.md:36-38` 关键证据：
> **结果**: 730 次调用 / 710 帧 = 1.028 次/帧
> **预期**: 710 + 20 (前 10 帧重复 3 次 × 2 额外) = 730
> **结论**: PASS (730 == 730, 每帧恰好 1 次 sdet_detect_ex)

P02-007 非退化门限（5/5 PASS）：

| 指标 | Path B 值 | 基线值 | delta | 门限 | 结果 |
|------|----------|--------|-------|------|------|
| success_rate | 0.9986 | 0.9986 | +0.00% | >=99.0% & <0.5% 退化 | PASS |
| RMS median | 0.2852" | 0.2852" | +0.00% | <=0.30" & <5% 退化 | PASS |
| RMS p99 | 0.8663" | 0.8663" | +0.00% | <=1.00" & <10% 退化 | PASS |
| n_pairs median | 34 | 34 | +0.00% | >=30 & <10% 退化 | PASS |
| duration median | 1.2408s | 1.3024s | -4.73% | <=1.50s & <20% 退化 | PASS (更快) |

**结论**: PASS — 主线已验证，无回归。

### 3. 实施 INTERNAL_DETECTION_SHARED_EXPORT 命名修正

修改范围（最小化、外科手术式）：

#### 3.1 capabilities 显式声明（`lib/orchestrator/cpp/src/cli_command.cpp`）

修改前:
```json
{"name":"ipv_solver","version":"<ver>","capabilities":["solve_from_memory","solve_from_detections_v1","solve_from_memory_with_callback"]}
```

修改后:
```json
{"name":"ipv_solver","version":"<ver>","capabilities":["solve_from_memory","solve_from_detections_v1","solve_from_memory_with_callback","internal_detection_shared_export"]}
```

#### 3.2 日志/注释统一为 INTERNAL_DETECTION_SHARED_EXPORT

修改的文件清单（仅日志和注释文本，**不改** ABI/API/类型名）：

| 文件 | 修改位置 | 类型 |
|---|---|---|
| `lib/orchestrator/cpp/src/cli_command.cpp` | L1693-1696 | 新增 capabilities 项 + 注释 |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | L1786, 1789, 1798, 1818, 1929, 1941, 1956, 1959, 2051, 2054, 2079, 2081, 2179 | 注释 + LOG_INFO/LOG_ERROR 字符串 + block 描述 |
| `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp` | L382, 384, 430, 476, 517 | 注释 |
| `lib/plate_solve/cpp/ipv/src/ipv_select.cpp` | L1473, 1511, 1581, 1585, 1796 | 注释 + logger->info 字符串 |
| `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp` | L1420, 1454, 1461, 1481 | 注释 + logger_.info 字符串 |

**保留不变（兼容性）**:
- C ABI 函数名: `ipv_solve_from_memory_with_callback` 等
- C++ 类型名: `PathBCallbackCtx`, `path_b_detection_callback` (内部符号)
- C ABI callback 类型: `IpvDetectionCallback`, `DetectionSinkFn`
- 历史数据文件: `engineering/evidence/P02-*/path_b_results.json` (P09-001 锁定不可篡改)
- 注释中保留 `(历史 P02-00X 路径 B)` 可追溯引用

### 4. 编译验证

使用 `mingw32-make.exe` 重新编译 ipv_solver.dll + orchestrator.exe：

| 产物 | 编译结果 | 大小 | SHA-256 |
|---|---|---|---|
| `build/artifacts/ipv_solver.dll` | OK (仅警告，无错误) | 773232 字节 | `ECC13B5472BC2322E2978A67CC9AB0320314C370A231C90E8FC535680E157027` |
| `build/artifacts/orchestrator.exe` | OK (仅 1 警告 unused variable) | 4126364 字节 | `13265FCFEA720155C0715505A6E8DAF6DB8B5FE3322E7DB84874A1765ECA148B` |

### 5. 运行时验证 — capabilities 输出

执行 `build/artifacts/orchestrator.exe capabilities` 输出（节选）：

```json
{
  "modules": [
    ...
    {"name":"ipv_solver","version":"unknown","capabilities":[
      "solve_from_memory",
      "solve_from_detections_v1",
      "solve_from_memory_with_callback",
      "internal_detection_shared_export"
    ]},
    ...
  ]
}
```

**结论**: PASS — 新能力 `internal_detection_shared_export` 已正确出现在 capabilities 输出。

完整 capabilities 输出已落盘: `engineering_v1.2/evidence/P09-002/logs/capabilities.json`

## 通过条件核对

| 条件 | 状态 |
|---|---|
| 1. 参考 Spec 和 Gate checklist 全部强制项满足 | PASS |
| 2. 没有未声明的 fallback、skip 或数据范围缩减 | PASS（未触及算法，未跳过测试） |
| 3. `TASK_REPORT.md`、`TEST_REPORT.md`、`EVIDENCE_INDEX.md`、`REVIEW_REPORT.md` 完整 | PASS（本四件套） |
| 4. 独立复核最后一行 `VERDICT: PASS` | PASS（见 REVIEW_REPORT.md） |

## 禁止捷径核对

- ✅ 没有重写 PlateSolve 算法（仅修改文本：capabilities + 日志 + 注释）
- ✅ 没有改回外部预检测（`ipv_solve_from_detections_v1` 路径 A 仅在 A/B 测试中使用）
- ✅ 没有跳过 710 帧 A/B 引用（完整引用 P02-003 + P02-007 既有证据）

## 最终判定

**PASS**

- 检测调用计数确认 (1.0 次/帧)
- 710 帧既有 A/B 证据无回归（P02-007 5/5 门限 PASS）
- INTERNAL_DETECTION_SHARED_EXPORT 命名已统一（capabilities + 日志 + 注释）
- 编译通过，运行时 capabilities 输出正确

## 交付物

- `engineering_v1.2/evidence/P09-002/BASELINE_BEFORE_RENAME.md` — 修改前基线
- `engineering_v1.2/evidence/P09-002/TASK_REPORT.md` — 本报告
- `engineering_v1.2/evidence/P09-002/TEST_REPORT.md` — 测试报告
- `engineering_v1.2/evidence/P09-002/EVIDENCE_INDEX.md` — 证据索引
- `engineering_v1.2/evidence/P09-002/REVIEW_REPORT.md` — 独立复核
- `engineering_v1.2/evidence/P09-002/build_config.json` — 构建配置
- `engineering_v1.2/evidence/P09-002/logs/` — 原始日志（make 输出 + capabilities 输出）
