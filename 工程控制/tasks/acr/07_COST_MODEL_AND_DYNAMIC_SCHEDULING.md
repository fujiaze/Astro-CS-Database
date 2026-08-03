# CostEstimator 与动态异构调度

## 1. 路由不是固定比例

HardwareProfile 不保存：

```text
CPU 18% / GPU 82%
```

也不保存每业务 kernel 的 `preferred_backend`。它保存各设备在不同能力族、尺寸、精度和驻留下的曲线。

## 2. TaskDescriptor输入

- TaskClass/OperationId；
- 工作域、Tile、批次数和剩余块；
- 精度和累加策略；
- 字节读写、shape、stride、halo；
- 连续/局部/随机/scatter；
- 稀疏度、原子冲突、分支均匀性；
- 输入输出驻留；
- 是否可拆、是否允许混合；
- 当前设备、队列、健康和容量。

## 3. 候选成本

```text
T_finish(device, chunk) =
    queue_wait
  + submit_or_launch
  + required_transfer
  + profile_compute(task_class, precision, size, residence)
  + merge_or_sync
  + low_confidence_penalty
```

### 映射原则

- elementwise：内存曲线+算术强度；
- reduction：对应 operation/precision 曲线；
- convolution：direct/separable/FFT、核、stride、halo和驻留；
- sparse/atomic/branch：对应不规则能力族；
- library：成熟 adapter曲线；
- 小任务必须计入固定启动和传输成本。

## 4. 候选块大小

对 CPU/GPU 各自生成有限候选块：

- 计算时间明显大于提交开销；
- 不超过 RAM/VRAM；
- GPU传输收益为正；
- Tile/halo合法；
- 尾部可继续收缩。

不通过穷举全部百分比决定任务份额。

## 5. 动态工作保持

1. 将工作域拆成可追踪的未开始块；
2. CPU 和每张 GPU 根据预计完成时间领取块；
3. 设备完成后继续领取；
4. 设备忙而另一设备空闲时，空闲设备可领取其预计更早完成的块；
5. 剩余工作减少时 guided收缩；
6. 已开始块不迁移；
7. coverage bitmap/ID确保每块恰好一次；
8. 设备失败只回收未开始块；
9. 实际工作量分布仅写入诊断，不持久为模型。

## 6. 数据驻留

- 连续 GPU任务尽量保持 device-resident；
- CPU参与前必须比较迁移成本；
- 写入后使其他副本失效；
- 只在依赖要求时同步；
- 多GPU优先 P2P；不可用时比较 host staging 成本；
- 不为提高表面利用率执行负收益迁移。

## 7. Reduction

每设备产生局部结果，最终依据 NumericPolicy 合并。FP32默认允许末位差异；FP64 accumulator和deterministic merge由任务声明。

## 8. Profile状态

- missing：CPU-only+警告；
- stale：警告，按配置使用或CPU-only；
- partial：只使用有效能力族；
- corrupt：拒绝加载并回退；
- runtime：只读，不写回。

## 9. 调度验证

必须验证：

- 同一任务类别随尺寸/驻留选择发生合理变化；
- 小任务因开销保留 CPU；
- device-resident大任务优先 GPU；
- GPU预忙时 CPU继续领取；
- CPU预忙时 GPU继续领取；
- 多GPU独立领取；
- 尾部不出现长时间单设备拖尾；
- profile文件运行前后hash不变；
- 无真实 GPU 时 Mixed 为 SKIPPED，不得 PASS。

## 10. 不属于在线学习

读取队列、忙闲、容量和剩余任务属于运行时调度。禁止用本次实际完成时间修改画像、拟合参数或长期权重。
