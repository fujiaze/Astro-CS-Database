# 测试与验证矩阵

## 1. Commit F纠正测试

- `actual_primary_backend`来自真实done工作量；
- predicted与actual字段分开；
- CostEstimate.per_device改变真实claim；
- coverage包含pending/claimed/done/failed；
- 执行失败时不得全部mark_done；
- 固定70/30尾段实验改名，不作为guided通过证据；
- MemoryBudget正式配置注入；
- 每种MemoryAction有执行测试。

## 2. 构建

- Windows/MSVC CPU-only；
- Linux/GCC、Clang CPU-only；
- 无GPU SDK；
- 至少一个真实GPU backend；
- Debug/Release；
- ASan/UBSan实际构建；
- TSan适用CPU路径。

## 3. HardwareProfile与CostEstimator

- CPU ISA、线程、FP32/FP64；
- STREAM、BabelStream和H2D/D2H；
- reduction、卷积、gather/scatter/atomic/branch；
- 固定开销；
- 模型留出误差；
- 小任务CPU、大device-resident任务GPU；
- 低置信度、RAM/VRAM和queue成本；
- profile运行前后hash不变。

## 4. Shared Pool与Mixed

- CPU与真实GPU同时完成不同唯一块；
- 无GPU时SKIPPED；
- 多GPU独立claim；
- GPU预忙时CPU继续；
- CPU预忙时GPU继续；
- 动态尾部收缩；
- 故障回收未完成块；
- 不遗漏、不重复；
- actual report与backend日志一致。

## 5. 资源控制

- 50/80/95/100利用率目标；
- 真实采样或明确估算；
- 控制动作实际执行；
- 所有CPU线程可参与；
- GPU队列水位；
- RAM/VRAM所有动作；
- 状态和取消响应；
- 控制器不修改画像。

## 6. Evidence

- path guard命令、完整输出、退出码；
- `git rev-parse HEAD`等于manifest、git log tip和源码快照HEAD；
- 工作树干净；
- 每个测试日志包含命令、环境、开始结束、退出码；
- SKIPPED有明确原因；
- 不能只保存“PASSED N tests”一行摘要；
- Evidence生成后不再提交到实现分支造成HEAD漂移。

## 7. 主线

- 算法目录零diff；
- 主线测试通过；
- CPU-only默认构建；
- 普通启动不初始化ACR、不探测GPU、不发warning；
- 合并后重复验证。
