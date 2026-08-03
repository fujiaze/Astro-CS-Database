# 资源占用控制

## 1. 配置语义

```yaml
utilization:
  cpu_target: 0.95
  gpu_target: 0.95
  io_target: 0.95
capacity:
  ram_limit: 0.95
  vram_limit: 0.95
```

- CPU/GPU/I/O：软利用率目标；
- RAM/VRAM：容量上限；
- 不表示任务切分比例；
- 不影响 HardwareProfile 数值。

完整示例见 `schemas/compute_config.example.yaml`。

## 2. CPU

- 所有允许的逻辑线程均可进入 ACR arena；
- 不通过永久少开一个线程实现95%；
- 通过任务粒度、队列水位、提交节奏、worker让步和优先级控制；
- 控制窗口建议100～500ms；
- 线程错峰让步，禁止全线程长时间同步睡眠；
- 记录所有 worker 是否实际参与。

## 3. GPU

- 通过 stream/queue深度、batch、kernel时长和提交节奏控制；
- 显示 GPU 优先短 batch；
- 不允许无界排队；
- 多 GPU 独立控制；
- 利用率目标允许稳定波动，不承诺每毫秒精确95%。

## 4. 利用率来源

优先使用平台可用的真实指标：

- CPU：OS/performance counter 或进程 CPU时间窗口；
- NVIDIA：NVML；
- AMD：ROCm SMI；
- Intel：Level Zero/oneAPI可用接口；
- 其他：有文档的队列占用/设备busy估算。

若真实API不可用，可使用可审计估算，但必须在报告中标记 `estimated=true`，不能声称真实设备利用率。

## 5. RAM/VRAM

```text
limit = min(total * ratio, total - fixed_reserve)
```

达到限制时：停止新提交、缩小块、释放可重建缓存、选择低内存路径、回退其他设备或明确失败。

## 6. 控制闭环不是在线学习

控制器可以依据当前利用率调整队列深度和节奏，但不得修改：

- 算术/内存/卷积曲线；
- CostEstimator参数；
- HardwareProfile；
- Qualification原始数据。

## 7. 验收

在持续工作负载下测试 50%、80%、95%、100%目标：

- 报告实际平均、p95、误差和控制窗口；
- 报告是否真实指标或估算；
- 报告所有CPU worker参与；
- 状态查询和取消保持响应；
- 不能仅向控制器输入0.92/0.99并检查数学输出。
