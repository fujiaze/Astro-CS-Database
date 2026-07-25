# 15 路线图与 Gate

- G0 基线可复现：源码、依赖、真实数据、旧结果可追溯。
- G1 数据契约稳定：PipelineFrame 唯一归属、block schema 与 contract tests。
- G2 PlateSolve 无退化与单次检测：全量 TestData 决定共享上游或保守内部导出路径；PlateSolve/PSF 共用同一星表；float32 PSF。
- G3 真实校准与参数链：Master、Gaia、filter/QE、Drizzle 参数全部生效。
- G4 CLI 协议：机器事件、严格错误、capabilities、inspect。
- G5 Stage1 真实数据：canonical 全通过，HISS 可独立验证。
- G6 Stage2 真实数据：梯度、剔除、SNR²、HCSD 全通过。
- G7 稳定性：性能、内存、取消、故障、长批次通过。
- G8 CLI Core v1 发布：自包含运行包与版本清单。

任何 Gate 未通过，后续任务可以调查但不得宣称相应里程碑完成。
