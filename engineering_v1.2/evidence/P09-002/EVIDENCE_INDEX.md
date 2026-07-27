# P09-002 EVIDENCE_INDEX — 证据索引

- 任务: P09-002
- 阶段: P09
- Gate: G9
- 编制日期: 2026-07-27

## 1. 任务定义

| 字段 | 值 |
|---|---|
| 任务 ID | P09-002 |
| 标题 | 确认共享检测主线与统一命名 |
| 任务定义文件 | `engineering_v1.2/tasks/P09-002.md` |
| 参考 Spec | `engineering_v1.2/docs/02_PLATESOLVE_SHARED_DETECTION_MAINLINE_STATUS.md` |
| 依赖 | P09-001 (DONE) |
| Gate | G9 |
| 执行者 | ACSD 自治 Agent |

## 2. 交付物

| 类型 | 路径 | SHA-256 / 说明 |
|---|---|---|
| 修改前基线 | `engineering_v1.2/evidence/P09-002/BASELINE_BEFORE_RENAME.md` | 修改前事实快照（命名变体清单 + 调用计数 + 710 帧既有证据引用） |
| 任务报告 | `engineering_v1.2/evidence/P09-002/TASK_REPORT.md` | 完整执行记录 |
| 测试报告 | `engineering_v1.2/evidence/P09-002/TEST_REPORT.md` | 6/6 测试 PASS |
| 证据索引 | `engineering_v1.2/evidence/P09-002/EVIDENCE_INDEX.md` | 本文档 |
| 独立复核 | `engineering_v1.2/evidence/P09-002/REVIEW_REPORT.md` | VERDICT: PASS |

## 3. 修改文件清单

### 3.1 源码修改（仅文本，不改 ABI）

| 文件 | 修改类型 |
|---|---|
| `lib/orchestrator/cpp/src/cli_command.cpp` | 新增 capabilities 项 + 注释 |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | 注释 + LOG_INFO/LOG_ERROR 字符串 + block 描述 |
| `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp` | 注释 |
| `lib/plate_solve/cpp/ipv/src/ipv_select.cpp` | 注释 + logger->info 字符串 |
| `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp` | 注释 + logger_.info 字符串 |

### 3.2 构建产物

| 产物 | 路径 | SHA-256 | 大小 |
|---|---|---|---|
| orchestrator.exe | `build/artifacts/orchestrator.exe` | `13265FCFEA720155C0715505A6E8DAF6DB8B5FE3322E7DB84874A1765ECA148B` | 4126364 字节 |
| ipv_solver.dll | `build/artifacts/ipv_solver.dll` | `ECC13B5472BC2322E2978A67CC9AB0320314C370A231C90E8FC535680E157027` | 773232 字节 |

## 4. 测试证据

| 测试 ID | 描述 | 证据文件 | 结果 |
|---|---|---|---|
| T1 | 静态字符串测试 | Grep 结果（见 TEST_REPORT.md） | PASS |
| T2 | 编译测试 | `logs/make_ipv.{out,err}`, `logs/make_orc.{out,err}` | PASS |
| T3 | 运行时 capabilities 测试 | `logs/capabilities.json`, `logs/capabilities.err` | PASS |
| T4 | 710 帧 A/B 既有证据引用 | `engineering/evidence/P02-003/`, `engineering/evidence/P02-007/` | PASS |
| T5 | SHA-256 完整性测试 | 本文档 §3.2 | PASS |
| T6 | 兼容性测试（未改 ABI） | Grep 结果（见 TEST_REPORT.md） | PASS |

## 5. 引用的既有证据（不可篡改，P09-001 锁定）

| 引用 | 路径 | 用途 |
|---|---|---|
| P02-003 A/B 全量测试 | `engineering/evidence/P02-003/` | 710 帧路径 B 与旧路径完全一致 |
| P02-003 路径决策 | `engineering/evidence/P02-003/PLATESOLVE_PATH_DECISION.md` | 路径 B 决策依据 |
| P02-003 ADR | `engineering/evidence/P02-003/ADR.md` | 架构决策记录 |
| P02-007 Gate 验证 | `engineering/evidence/P02-007/TASK_REPORT.md` | 单次检测 1.0 次/帧 + 5/5 门限 PASS |
| P02-007 汇总 JSON | `engineering/evidence/P02-007/path_b_results.json` | 710 帧逐帧结果 |
| P02-007 逐帧结果 | `engineering/evidence/P02-007/results/frame_*.json` | 710 帧原始数据 |
| P02-007 Gate 验证 JSON | `engineering/evidence/P02-007/gate_verification.json` | 结构化验证结果 |

## 6. 控制文件状态

| 文件 | 状态 |
|---|---|
| `engineering_v1.2/control/PROJECT_STATE.yaml` | 更新（current_task → P09-003, last_completed_task → P09-002） |
| `engineering_v1.2/control/CURRENT_TASK.md` | 更新（指向 P09-003） |
| `engineering_v1.2/control/DECISION_REGISTER.md` | 更新（记录 P09-002 完成） |

## 7. 通过条件核对

| 通过条件 | 状态 | 证据 |
|---|---|---|
| 1. 参考 Spec 和 Gate checklist 全部强制项满足 | PASS | TASK_REPORT §1-5 |
| 2. 没有未声明的 fallback、skip 或数据范围缩减 | PASS | TASK_REPORT §禁止捷径核对 |
| 3. 四件套完整 | PASS | 本文档 §2 |
| 4. 独立复核最后一行 VERDICT: PASS | PASS | REVIEW_REPORT.md 最后一行 |

## 8. 红线核对

- ✅ 没有用旧路径 A/B 一致替代"标准 WCS 可回投真实星点"的闭环验证（本任务不涉及 WCS 闭环）
- ✅ 没有在 Photometric 内部偷偷翻转 Y 来掩盖错误的 WCS 生产端（本任务不涉及 WCS 符号）
- ✅ 没有把 `F_syn` 有效等同于测光成功（本任务不涉及测光）
- ✅ 没有把同一 HISS 的副本当成大尺度马赛克验证（本任务不涉及马赛克）
- ✅ 没有把 `has_snr=false` 的真实数据等权叠加称为 SNR² 加权验证（本任务不涉及 SNR）
- ✅ 没有通过降低分辨率、转 uint8、关闭 STF 或减少显示区域来宣称浏览器性能通过（本任务不涉及浏览器）
- ✅ 没有让浏览器 I/O、解压、LOD 构建在 GUI/渲染线程同步执行（本任务不涉及浏览器）
- ✅ 没有修改 HCSD 格式（本任务不涉及 HCSD）

## 9. 任务边界核对（禁止捷径）

- ✅ 没有重写 PlateSolve 算法
- ✅ 没有改回外部预检测
- ✅ 仅修改文本（capabilities + 日志 + 注释），未触及算法逻辑
- ✅ 保留所有 C ABI 函数签名（ABI 兼容）
- ✅ 保留所有内部 C++ 类型名（源码兼容）
- ✅ 保留历史 evidence JSON 文件名（P09-001 锁定基线不可篡改）

## 10. 最终判定

**PASS**
