# Commit F 专项纠正计划

审计对象：`AstroCS_ACR_CommitF_Review_2026-08-03(1).zip`  
审计结论：Commit F仅为中间基础，不允许合并main。

## 1. 保留项

- CPU利用率采样器；
- CostEstimate、CurrentState、Dispatcher数据结构；
- MemoryBudget接口；
- `Fail/ShrinkBlock`初步代码；
- CPU固定尾段缩块实验；
- 已通过的CPU-only单元测试。

固定尾段实验必须改名为 `fixed_tail_chunking_experiment`，不得继续称为动态guided。

## 2. Commit F-fix 1：事实、coverage与Evidence

建议提交：

```text
fix(acr): report actual execution and regenerate single-head evidence
```

任务：

- actual backend由真实done元素/块统计生成；
- predicted字段单独保留；
- coverage状态机；
- 失败、取消、未开始块不标DONE；
- 修复path guard；
- 完整命令日志；
- 从目标HEAD干净worktree生成Evidence；
- 不把Evidence提交后再引用旧HEAD。

验收：`runtime_execution_report.schema.json`通过；backend日志与report一致；path guard PASS；所有HEAD一致。

## 3. Commit F-fix 2：CostEstimator驱动Shared Pool

建议提交：

```text
refactor(acr): drive shared work claims from per-device cost estimates
```

任务：

- Dispatcher维护pending池；
- CPU/GPU worker每次claim计算候选完成时间；
- `CostEstimate.per_device`真正影响设备和chunk；
- queue、transfer、residence、capacity进入估算；
- 无固定CPU/GPU份额；
- 设备失败回收未完成块。

验收：构造两组画像可改变真实设备领取；actual统计证明执行路径改变。

## 4. Commit F-fix 3：动态guided

建议提交：

```text
feat(acr): add profile guided dynamic chunk claiming
```

任务：

- 删除最终路径中的固定70%阈值；
- 根据remaining、活跃设备和画像吞吐生成块；
- 尾部逐步收缩；
- 保持提交开销可控；
- profile运行时只读。

验收：不同比例设备速度下，拖尾明显收敛；不重复、不遗漏。

## 5. Commit F-fix 4：资源闭环与MemoryBudget

建议提交：

```text
feat(acr): enforce utilization targets and memory backpressure
```

任务：

- CPU/GPU真实采样；
- submit gate、队列深度、batch和worker错峰让步；
- RuntimeConfig注入MemoryBudget；
- 每种MemoryAction实际执行；
- 记录控制动作和效果。

验收：50/80/95/100持续负载报告；不能用人工样本代替。

## 6. Commit G：真实GPU与Mixed

- 使用官方支持的MSVC+CUDA或ADR批准后端；
- 真实buffer/copy/kernel/event；
- GPU HardwareProfile；
- CPU+GPU同时claim；
- 工具链不可用为SKIPPED，但阻断最终合并。

## 7. Commit H：Sanitizer与统一交付

- ASan/UBSan实际开启；
- 完整源码快照；
- 同一干净HEAD Evidence；
- 最新main回归；
- 合并门禁报告。

## 8. 禁止事项

- 不得只修改测试让其通过；
- 不得把推荐设备写成actual；
- 不得用固定70/30宣称guided；
- 不得用人工利用率数字宣称95%控制；
- 不得模拟GPU并标PASS；
- 不得修改真实算法；
- 不得在门禁未通过时合并main。
