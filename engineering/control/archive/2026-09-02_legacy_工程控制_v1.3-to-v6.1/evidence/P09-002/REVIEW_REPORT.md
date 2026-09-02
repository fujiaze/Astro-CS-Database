# P09-002 REVIEW_REPORT — 独立复核

- 任务: P09-002
- 阶段: P09
- Gate: G9
- 复核日期: 2026-07-27
- 复核者: ACSD 自治 Agent（独立复核模式）

## 复核范围

对 P09-002 任务的执行结果进行独立复核，验证：
1. 任务定义的所有要求是否满足
2. 修改是否符合"最小化、外科手术式"原则
3. 是否违反任务边界（重写算法、改回外部预检测）
4. 是否引用了正确的既有证据
5. 编译和运行时验证是否真实可信
6. 通过条件是否真正满足

## 复核项目

### 1. 任务定义要求

`engineering_v1.2/tasks/P09-002.md` 要求：
- ✅ 源码证明每帧只检测一次 — 已验证（TASK_REPORT §1）
- ✅ 确认主线提交 — 已验证（TASK_REPORT §2 引用 P02-003 + P02-007 既有证据）
- ✅ 能力/日志命名改为 INTERNAL_DETECTION_SHARED_EXPORT — 已验证（TASK_REPORT §3）
- ✅ 检测调用计数 — 730/730 = 1.0 次/帧（引用 P02-007）
- ✅ 710 既有报告引用 — 引用 P02-003 + P02-007
- ✅ 命名修正测试 — 6/6 PASS（TEST_REPORT）

### 2. 修改原则审查

修改前后对比：

| 类别 | 修改前 | 修改后 | 是否符合最小化原则 |
|---|---|---|---|
| capabilities 字符串 | 3 个能力项 | 4 个能力项（新增 `internal_detection_shared_export`） | PASS — 仅新增 1 项 |
| 日志字符串 | 含 "路径B" | 含 "INTERNAL_DETECTION_SHARED_EXPORT" | PASS — 仅文本替换 |
| 注释 | 含 "P02-00X 路径 B" | 含 "P09-002 INTERNAL_DETECTION_SHARED_EXPORT (历史 P02-00X 路径 B)" | PASS — 保留可追溯性 |
| C ABI 函数名 | `ipv_solve_from_memory_with_callback` | `ipv_solve_from_memory_with_callback` (不变) | PASS |
| C++ 类型名 | `PathBCallbackCtx`, `path_b_detection_callback` | 不变 | PASS |
| 历史 evidence 文件 | `path_b_results.json` | 不变（P09-001 锁定） | PASS |

**结论**: PASS — 修改严格限于文本（capabilities + 日志 + 注释），未触及算法逻辑和 ABI。

### 3. 任务边界核对

`engineering_v1.2/tasks/P09-002.md` 禁止捷径：
- ❌ "不得重写PlateSolve算法" — 检查通过：未修改任何算法代码
- ❌ "或改回外部预检测" — 检查通过：`ipv_solve_from_detections_v1` (路径 A) 仍存在但未启用为生产路径

**grep 验证**: 搜索 `lib/` 下未发现任何新增 `sdet_detect_ex` 调用（仍为 3 处：ipv_select.cpp:639, 962, 1558，与修改前一致）。

### 4. 既有证据引用审查

P09-002 不重跑 710 帧测试，引用既有证据：

| 引用 | 文件存在性 | 内容一致性 |
|---|---|---|
| `engineering/evidence/P02-003/TEST_REPORT.md:99` | 存在 | "全量 710 帧 A/B 测试完成, 路径 B 与旧路径在成功率、RMS、n_pairs 上完全一致" — 与 TASK_REPORT §2 引用一致 |
| `engineering/evidence/P02-007/TASK_REPORT.md:36-38` | 存在 | "730 == 730, 每帧恰好 1 次 sdet_detect_ex" — 与 TASK_REPORT §2 引用一致 |
| `engineering/evidence/P02-007/TASK_REPORT.md:60-69` | 存在 | 5/5 非退化门限 PASS — 与 TASK_REPORT §2 引用一致 |

**结论**: PASS — 引用准确，未篡改既有证据。

### 5. 编译与运行时验证审查

#### 5.1 编译日志审查

`logs/make_ipv.out` 末尾：
```
=== DLL 编译完成: ipv_solver.dll ===
mingw32-make: Leaving directory 'F:/Astro dev/Astro CS Normalization Database/lib/plate_solve/cpp/ipv'
```

`logs/make_orc.out` 末尾：
```
g++ -O2 -std=c++17 -Wall -fopenmp -o orchestrator.exe src/main.cpp src/orchestrator.cpp ... -static -lm
mingw32-make: Leaving directory 'F:/Astro dev/Astro CS Normalization Database/lib/orchestrator/cpp'
```

**结论**: PASS — 两个产物均编译成功（退出码 0）。

#### 5.2 警告审查

ipv_solver.dll 编译警告（`logs/make_ipv.err`）：
- `cast-function-type` 警告（既有问题，与本次修改无关）
- `unused-parameter` 警告（既有问题）
- `unused-function` 警告（既有问题）
- `unused-variable` 警告（既有问题）

orchestrator.exe 编译警告（`logs/make_orc.err`）：
- `unused variable 'key_start'` 警告（既有问题，在 `cli_command.cpp:215`，与本次修改的 L1693-1696 无关）

**结论**: PASS — 所有警告均为既有问题，与本次命名修正无关。

#### 5.3 运行时 capabilities 输出审查

`logs/capabilities.json` 中 ipv_solver 模块输出：
```json
{"name":"ipv_solver","version":"unknown","capabilities":[
  "solve_from_memory",
  "solve_from_detections_v1",
  "solve_from_memory_with_callback",
  "internal_detection_shared_export"
]}
```

**结论**: PASS — 新能力 `internal_detection_shared_export` 正确出现，既有 3 个能力保留。

#### 5.4 SHA-256 完整性审查

| 产物 | SHA-256 (build.log) | SHA-256 (TEST_REPORT) | 一致性 |
|---|---|---|---|
| orchestrator.exe | `13265FCFEA720155C0715505A6E8DAF6DB8B5FE3322E7DB84874A1765ECA148B` | 同左 | PASS |
| ipv_solver.dll | `ECC13B5472BC2322E2978A67CC9AB0320314C370A231C90E8FC535680E157027` | 同左 | PASS |

### 6. 通过条件核对

| 通过条件 | 状态 | 证据位置 |
|---|---|---|
| 1. 参考 Spec 和 Gate checklist 全部强制项满足 | PASS | TASK_REPORT §1-5 |
| 2. 没有未声明的 fallback、skip 或数据范围缩减 | PASS | TASK_REPORT §禁止捷径核对 |
| 3. 四件套完整 | PASS | EVIDENCE_INDEX §2 |
| 4. 独立复核最后一行 VERDICT: PASS | 见下文 | 本文档最后一行 |

### 7. 红线核对

- ✅ 没有用旧路径 A/B 一致替代"标准 WCS 可回投真实星点"的闭环验证（本任务不涉及 WCS 闭环，仅命名修正）
- ✅ 没有在 Photometric 内部偷偷翻转 Y 来掩盖错误的 WCS 生产端
- ✅ 没有把 `F_syn` 有效等同于测光成功
- ✅ 没有把同一 HISS 的副本当成大尺度马赛克验证
- ✅ 没有把 `has_snr=false` 的真实数据等权叠加称为 SNR² 加权验证
- ✅ 没有通过降低分辨率、转 uint8、关闭 STF 或减少显示区域来宣称浏览器性能通过
- ✅ 没有让浏览器 I/O、解压、LOD 构建在 GUI/渲染线程同步执行
- ✅ 没有修改 HCSD 格式

## 复核结论

P09-002 任务的所有要求均已满足：
- 检测调用计数已确认（1.0 次/帧，引用 P02-007 既有证据）
- 主线提交已确认（引用 P02-003 既有证据，710 帧 A/B 无回归）
- 命名已统一为 INTERNAL_DETECTION_SHARED_EXPORT（capabilities + 日志 + 注释）
- 编译通过，运行时 capabilities 输出包含新能力
- 修改严格限于文本，未触及算法和 ABI
- 四件套完整，证据链可追溯

VERDICT: PASS
