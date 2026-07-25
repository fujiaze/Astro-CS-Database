# 当前任务：P02-001 PlateSolve 全量 TestData 与旧路径基线

读取 `tasks/P02-001.md` 并执行。冻结全部 PlateSolve TestData 清单、文件哈希、参数和旧路径结果。

## 执行步骤

1. 冻结全部 PlateSolve TestData 清单（所有可用于板解算测试的真实帧）
2. 计算每个文件的 SHA-256
3. 记录旧路径参数和旧路径结果（WCS、星数、RMS、耗时）
4. 记录旧路径 detector 调用次数及 PlateSolve 重复运行抖动

完成独立复核后,更新状态并进入依赖满足的下一任务。
