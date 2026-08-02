# 基于硬件画像的成本推算与动态混合执行

## 1. 不是固定比例路由

画像不保存：

```text
CPU 18%
GPU 82%
```

也不要求用户指定比例。它保存的是设备在不同能力族、精度、尺寸和驻留条件下的性能曲线。

## 2. 路由输入

运行时输入包括：

- TaskTraits和OperationId；
- 工作域、Tile或批次数量；
- 输入输出字节数、shape、stride和halo；
- 数据当前驻留位置；
- 当前可用设备及健康状态；
- 当前CPU/GPU队列；
- RAM/VRAM预算；
- 资源利用率目标。

## 3. 成本模型

每个设备和候选块大小估算：

```text
T_device(chunk) = queue_wait
                + launch_or_submit
                + transfer_if_needed
                + compute_from_profile
                + local_merge_or_sync
```

其中：

- 连续elementwise使用内存/算术画像和计算强度等级；
- reduction使用对应精度和操作曲线；
- convolution按direct/separable/FFT、核尺寸、stride和驻留曲线；
- sparse/atomic/branch使用对应惩罚曲线；
- 小任务必须计入固定启动成本。

## 4. 初始块大小

根据画像推算满足以下条件的最小有效块：

- 计算时间明显大于提交/launch开销；
- GPU传输可被收益覆盖；
- 不超过RAM/VRAM预算；
- Tile边界和halo合法。

GPU通常领取较大块，CPU领取较小块，但具体值来自画像，不写死。

## 5. 动态工作保持

1. 创建共享未开始工作池；
2. 各设备worker按画像领取适合自己的批次；
3. 设备完成后继续领取；
4. 剩余工作减少时采用guided尾部收缩；
5. GPU忙而CPU有空时，CPU领取其预计能更早完成的块；
6. CPU忙而GPU有队列空间时，GPU继续领取；
7. 多GPU独立领取；
8. 已开始块不迁移；
9. 所有块用coverage ID保证恰好一次。

这会自然形成动态CPU/GPU工作量分布，不需要事先计算固定百分比。

## 6. 当前状态不是在线学习

调度器可读取：队列长度、设备忙闲、剩余任务、驻留和容量。设备完成后请求下一块属于正常调度。

不得：

- 用本次完成时间修改hardware-profile；
- 保存新的设备吞吐参数；
- 静默改变Qualification结果；
- 把运行时偶然负载变成长期模型。

## 7. 数据驻留

- 连续GPU任务尽量保留数据；
- CPU参与前比较下载/上传成本；
- 只有收益为正才跨设备迁移；
- Buffer写入后使其他副本失效；
- 同步只发生在依赖需要时；
- 多GPU间如无P2P则通过画像选择host staging或避免迁移。

## 8. Reduction和合并

CPU和每张GPU产生局部结果。最终按NumericPolicy合并。FP32默认允许末位差异；FP64 accumulator和deterministic merge由任务声明。

## 9. 未标定与过期

- 无画像：纯CPU多线程、安全保守ISA、非阻断警告；
- 过期画像：警告但默认允许继续使用；
- 用户可配置过期时CPU-only；
- 不自动重跑Benchmark。

## 10. 故障

- GPU失败：未开始块回到共享池；该设备隔离；
- 显存不足：缩小块，仍失败则其他设备/CPU；
- 插件加载失败：排除；
- CPU ISA由cpu_features加载前门禁；
- 任何回退必须记录，不得重复执行已完成块。
