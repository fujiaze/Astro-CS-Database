# 13 性能、内存与稳定性

## 1. 基线指标

每帧记录 stage wall time、CPU time、线程数、峰值 RSS、I/O bytes、临时内存和输出尺寸。

## 2. 去重预期

- 路径 A：消除 PlateSolve 内部的一次 detector 调用及其图像转换，由上游唯一检测供 PlateSolve/PSF 共用；
- 路径 B：保留 PlateSolve 原始内部检测，但消除 Orchestrator 的第二次 detector 调用和第二次检测图像转换；
- 两条路径都通过 float32 PSF 再消除一份全图 uint16 缓冲。

性能不是正确性的替代。即使路径 A 更快，只要全量 TestData 任一案例科学退化，也必须选择路径 B。

## 3. 内存预算

Orchestrator 应在最后消费者完成后删除大块；Stage2 按叶/分块流式处理。内存预算超过阈值时应提前失败或降低并发，不得由系统 OOM 终止。

## 4. 稳定性

- Stage1 canonical 批次至少连续 100 帧或可用全部真实帧；
- Stage2 重复运行至少 10 次；
- 取消、失败后再次运行无锁文件、脏缓存和句柄泄漏；
- 输出 hash/统计的非确定性必须在容差内并解释来源。
