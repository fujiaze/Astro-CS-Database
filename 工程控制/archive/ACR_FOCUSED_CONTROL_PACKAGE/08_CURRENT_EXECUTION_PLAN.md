# 当前执行计划

本文件是Agent当前唯一任务计划。旧20—26号计划全部失效。

## 0. 开始前

- 继续现有`feature/astrocompute-runtime`；
- 记录当前HEAD和clean status；
- 将本包复制到仓库控制目录并记录包SHA；
- 先运行现有CPU/CUDA/Mixed测试，保存基线；
- 不创建新仓库、新分支或新版本包。

## 1. 保留已通过能力

不得回退：

- KernelRegistry / KernelInvocation / DeviceExecutor；
- 真实CPU/CUDA执行；
- 动态工作池、稳定WorkToken和attempt；
- CUDA buffer独立capacity；
- 多块卷积全局偏移修复；
- RAM/VRAM预算骨架；
- CUDA memcheck/racecheck与现有Sanitizer；
- 业务算法零修改。

## 2. 清理错误方向

- 从生产路径删除CPU/GPU精确利用率目标、controller、permit和目标档测试；
- 停止扩充通用gather/branch/atomic/全部ISA线程画像；
- 旧通用Profile字段可保留兼容，但不得成为路由和合并门禁；
- 删除或隔离会把未合格通用曲线送入当前CostEstimator的路径；
- MemoryBudget必须独立工作。

## 3. 建立目标Operation合成套件

实现或整理五个独立测试Operation：

1. Dense pixel accumulate FP32；
2. Dense pixel accumulate FP32+FP64 accumulator；
3. Pixel reduction FP64 accumulator；
4. Drizzle-like scatter/accumulate；
5. Resident two-stage chain。

CPU和GPU必须使用同一WorkloadDescriptor和同一数值语义。

## 4. 精简Benchmark/Profile

- 统一输出`OperationProfile`；
- 只测本包04号规范中的基础传输和目标Operation；
- quick仅诊断，standard生成qualified profile；
- 修正旧Evidence中的工作量、驻留和holdout问题；
- Profile不合格时使用最近点保守估算或CPU回退，不再扩张为通用模型项目。

## 5. 接通MixedRoutePlanner

- Cost只读取当前OperationProfile；
- CPU/GPU使用独立块大小；
- 共享pending域动态claim；
- 实现边际收益门和尾段停止慢设备claim；
- resident与host输入使用不同GPU阈值；
- 运行时完成时间只用于本次队列/尾部判断，不持久写回。

## 6. 完成驻留与内存路径

- 验证共享输入只上传一次并复用；
- 验证resident chain；
- RAM/pinned/每GPU VRAM独立reservation；
- Shrink→Reuse/Release→Wait→Fallback→Fail链路可执行；
- report记录实际传输、驻留和内存动作。

## 7. 性能与资格

分别运行CPU-only、GPU-only、AutoMixed和强制Mixed测试：

- 找出host输入与resident输入的GPU收益阈值；
- 验证至少一个真实Mixed工作负载；
- AutoMixed中位耗时不比三种实测最佳值差超过10%；
- 若Mixed无收益，Auto不得强制Mixed，但底层Mixed正确性仍需通过。

## 8. 最终Evidence

从一个干净实现HEAD在仓库外生成：

- 源码快照与git信息；
- 控制包SHA；
- CPU/CUDA构建；
- standard benchmark和OperationProfile；
- correctness、Mixed、residency、memory、fault测试；
- ASan/UBSan和compute-sanitizer；
- 完整命令、超时、exit code和原始日志；
- path guard PASS。

## 9. 完成定义

满足`07_TEST_AND_ACCEPTANCE.md`和`CHECKLIST.md`即可认为ACR底层完成，可dormant合并`main`。不等待Phase1、积分或Drizzle真实接入。
