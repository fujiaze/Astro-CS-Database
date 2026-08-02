# 现有ACR分支纠正任务

本文件针对已存在的`feature/astrocompute-runtime`和此前Evidence。Agent必须先审计实际代码，再按下列项目增量修正，不新建第二套工程。

## 1. 删除固定比例概念

搜索并处理：

- `cpu_share`、`gpu_share`；
- device `weight`；
- `0/25/50/75/100% mixed`作为路由；
- 每kernel `routes.json`固定份额；
- 用户可编辑比例配置。

测试中允许记录实际CPU/GPU完成量，但不得作为输入参数或长期模型。

## 2. Route Profile改为Hardware Profile

原有类似：

```json
{"devices":[{"id":"cpu","weight":0.18},{"id":"gpu","weight":0.82}]}
```

必须改为：设备能力曲线、固定开销、传输、归约、卷积、稀疏和分支画像。运行时根据TaskTraits推算。

## 3. 接通公共调用链

此前若`parallel_for`忽略OperationId或直接调用CPU runtime，必须改为：

```text
Public API → TaskDescriptor → CostEstimator → Dispatcher → CPU/GPU backend
```

无画像时才明确CPU fallback。不得让路由器、dispatcher和backend只作为孤立单测组件。

## 4. 真实CPU ISA

- 提供baseline和实际可构建ISA变体；
- cpu_features加载前门禁；
- Benchmark分别计时；
- 不用“检测到了AVX2”代替真正AVX2实现证据。

## 5. 扩展Benchmark

若当前只有Copy/AXPY/Triad，必须补齐：

- FP32/FP64算术；
- CPU STREAM式曲线；
- GPU BabelStream式曲线；
- H2D/D2H；
- reduction；
- direct/separable卷积；
- irregular/atomic/branch；
- submit/launch/event/alloc/merge；
- 模型拟合与留出验证。

## 6. 真实Mixed

任何`enable_gpu=false`、全部由CPU执行却命名为mixed的测试必须重写。必须在真实GPU上同时观察CPU和GPU完成不同唯一chunk。无GPU则SKIPPED，不能PASS。

## 7. 真实95%控制

不能只给控制器人工输入0.92/0.99并验证数学函数。必须在持续负载下读取实际CPU/GPU利用率或明确受限平台的可审计估算，控制提交并报告结果。

## 8. Sanitizer

Evidence声称ASan/UBSan/TSan时，构建选项和日志必须证明实际开启。普通生命周期测试不能命名为sanitizer验证。

## 9. 开源复用落地

- 已有效使用oneTBB/hwloc的代码保留；
- Google Benchmark或等价成熟框架用于微基准；
- portable GPU层按PoC选择并记录ADR；
- GEMM/FFT/scan使用成熟库adapter，不以自写naive实现宣称生产适配完成；
- 依赖不可用时明确SKIPPED。

## 10. Evidence统一

从一个干净HEAD一次生成：manifest、SHA256、test summary、原始日志、设备报告、画像、模型拟合、路径guard、源码快照和merge report。所有commit字段必须一致。

## 11. 合并门禁

上述纠正、全套经典实验、真实GPU、CPU-only、主线回归和无副作用均通过前，不得合并main。
