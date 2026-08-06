# 业务接入前冻结检查表

## 范围

- [ ] 仅`feature/astrocompute-runtime`
- [ ] Phase1、真实积分、Drizzle、HISS、Pipeline、CLI零修改
- [ ] 简单任务仍使用原OpenMP/CPU
- [ ] 未扩展无关通用Benchmark

## 架构冻结

- [ ] KernelInvocation/PartitionContract语义固定
- [ ] IndependentOutputTiles和PrivatePartialThenMerge可用
- [ ] 每GPU一个executor，stream仅为内部通道
- [ ] CPU launcher无嵌套OpenMP
- [ ] 无固定CPU/GPU份额
- [ ] makespan claim与尾段停止逻辑可解释
- [ ] RAM/staging/每GPU VRAM独立预算
- [ ] 未残留CPU/GPU精确利用率控制

## 驻留与传输

- [ ] frames/weights在worker前真实预取
- [ ] resident launcher使用device view
- [ ] 每token不重复上传帧栈
- [ ] generation变化正确重新上传
- [ ] CPU/GPU输出范围独占
- [ ] GPU范围正确D2H并覆盖完整输出
- [ ] H2D/D2H次数和字节真实报告
- [ ] 1/2 stream共享同一GPU队列和VRAM预算

## 加权积分样例

- [ ] OperationId为`synthetic.weighted_integration.fp64acc`
- [ ] FP32输入/权重、FP64累加、FP32输出
- [ ] SerialReference
- [ ] OpenMPBaseline
- [ ] AcrCpuOnly
- [ ] AcrGpuOnly host/resident
- [ ] ForcedMixedCorrectness
- [ ] AcrAutoMixed
- [ ] quick/standard/full矩阵
- [ ] single-shot与resident-reuse
- [ ] 数据生成不计时且固定seed

## 正确性

- [ ] 所有输出有限
- [ ] max_abs_error≤2e-5
- [ ] relative_l2_error≤2e-6
- [ ] coverage完整且无重叠
- [ ] 非整除尾块通过
- [ ] retry/attempt无重复写
- [ ] 1/2 stream结果一致
- [ ] 无GPU时准确SKIPPED

## 性能与路由

- [ ] 2 warmup、7 repeats
- [ ] 报告median/min/p90
- [ ] 报告speedup_vs_openmp
- [ ] 报告CPU/GPU items和chunks
- [ ] 报告recommended/actual chunks
- [ ] 报告stream/in-flight
- [ ] 报告H2D/D2H与resident reuse
- [ ] Auto距最佳合理模式≤10%或阻止业务接入
- [ ] 至少一个中/大case相对OpenMP有正加速，否则PERFORMANCE_NOT_QUALIFIED
- [ ] ForcedMixed不冒充Auto性能

## 工程与Evidence

- [ ] CPU-only和CUDA构建
- [ ] CTest 0 failed、0 timeout
- [ ] compute-sanitizer通过
- [ ] CPU sanitizer真实开启或准确说明限制
- [ ] 所有脚本和外部进程有超时
- [ ] path guard PASS
- [ ] 最终单一干净HEAD
- [ ] Schema验证通过
- [ ] SHA可复核
- [ ] 审核包不含build缓存和无关大文件
- [ ] `READY_FOR_BUSINESS_ADAPTER`结论真实
