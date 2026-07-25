# 18 PlateSolve 全量 TestData 路径决策规范

## 1. 目的

决定 AstroCS 当前版本是否可以把 PlateSolve 从原始“图像输入、内部检测”路径迁移为“外部 detections 输入”路径。该决策只依据完整、冻结、可复现的 TestData 结果，不依据架构偏好。

## 2. TestData 冻结

在编译或运行候选路径前，Agent 必须：

1. 搜索 PlateSolve 模块、仓库测试目录、已登记真实数据目录和历史回归脚本；
2. 建立 `platesolve_testdata_manifest.csv`；
3. 每项记录：case_id、文件路径、SHA-256、尺寸、目标天区、配置、期望状态、真值来源、历史用途；
4. 去重只能依据相同文件哈希；
5. 损坏或缺失数据必须在候选运行前登记，不能在看到结果后排除；
6. 清单冻结后计算 manifest SHA-256，并写入 A/B 报告。

“全量 TestData”指冻结清单中的所有案例，包括预期成功、预期失败、边界、畸变、稀疏星场、密集星场和历史缺陷回归案例。

## 3. 旧路径重复性基线

使用同一代码、依赖、配置和机器，对每个案例至少运行旧路径 3 次，记录：

- 退出状态、错误码、超时、崩溃；
- solve success/failure；
- WCS 中心、像元尺度、旋转、handedness；
- SIP 阶数和验证残差；
- 匹配数、内点数、RMS；
- detector 输出数；
- wall time、CPU time、峰值内存。

先根据旧路径重复运行确定非确定性抖动，再冻结比较门限。门限必须在运行候选路径前写入报告并计算哈希。

## 4. 门限来源

按以下优先级确定每个指标的门限：

1. 仓库现有 TestData/科学验收门限；
2. 已批准的历史回归门限；
3. `max(3 × 旧路径重复性抖动, 预先登记的工程最小容差)`。

不得依据候选结果反向放宽门限。若某指标没有真值且旧路径完全确定，则候选应与旧结果在浮点合理误差内一致。

## 5. 每案例硬判定

路径 A 只有在每个案例均满足以下条件时才算通过：

### 5.1 状态保持

- 旧路径成功的案例，候选不得失败、超时或崩溃；
- 旧路径预期失败的案例，候选不得产生未经真值验证的错误成功；
- 错误类别不得从明确可诊断错误退化为崩溃、挂起或无输出。

### 5.2 WCS 正确性

- 有外部真值时，两条路径均需通过真值门限，且候选不得超过预登记退化容差；
- 无外部真值时，候选与旧路径的中心、尺度、旋转、handedness、SIP 验证误差均需在冻结门限内；
- 不允许只比较 CRVAL 而忽略尺度、旋转、投影方向或畸变。

### 5.3 求解质量

- RMS、内点数、匹配数、残差分布不能出现超门限退化；
- 不能只以总体均值掩盖单例退化；
- 任一旧路径稳定通过的案例发生显著退化，路径 A 即 REJECT。

### 5.4 性能与稳定性

- 不得新增内存错误、句柄泄漏、超时或非确定性失败；
- 显著性能回退必须作为单独 Gate 失败或有明确 ADR，不可静默接受。

## 6. 决策输出

生成 `PLATESOLVE_PATH_DECISION.md`，只能以以下之一结束：

```text
DECISION: UPSTREAM_SHARED_DETECTIONS
```

或：

```text
DECISION: PRESERVE_INTERNAL_DETECTION_EXPORT
```

规则：

- 全部案例通过：选择 `UPSTREAM_SHARED_DETECTIONS`；
- 任一案例未通过：选择 `PRESERVE_INTERNAL_DETECTION_EXPORT`；
- 数据缺失导致全量测试不能完成：不得选择上游共享路径，当前版本默认选择保守内部导出路径，同时登记数据缺失风险。

这项决策不需要用户逐项确认，Agent 按规则自动执行。

## 7. 保守路径验证

若选择内部导出路径，必须额外证明：

- 原 PlateSolve 输入和算法路径未改变；
- 导出 callback 不修改 detections、排序或生命周期；
- callback 开关前后的 PlateSolve 输出在原路径重复性容差内一致；
- 每帧 detector 仅在 PlateSolve 内部调用一次；
- Orchestrator 不再二次检测；
- PSF 消费导出的同一星表。

## 8. 证据文件

至少包含：

```text
evidence/P02-003/
├── platesolve_testdata_manifest.csv
├── manifest.sha256
├── baseline_runs.jsonl
├── candidate_runs.jsonl
├── frozen_thresholds.json
├── per_case_comparison.csv
├── PLATESOLVE_PATH_DECISION.md
├── TASK_REPORT.md
├── TEST_REPORT.md
├── EVIDENCE_INDEX.md
└── REVIEW_REPORT.md
```
