# 资源占用与内存反压控制

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

CPU/GPU/I/O为软利用率目标；RAM/VRAM为容量上限。任何百分比都不是CPU/GPU任务份额。

## 2. 控制闭环必须包含三层

### 采样

- CPU：进程CPU时间窗口或OS计数器；
- NVIDIA：NVML；
- AMD：ROCm SMI；
- Intel：Level Zero/oneAPI；
- 无真实接口时可用明确标记的估算值。

### 决策

控制器输出可执行动作，不只返回数学结果：

```text
ALLOW_SUBMIT
HOLD_SUBMIT_FOR(duration)
REDUCE_QUEUE_DEPTH
INCREASE_QUEUE_DEPTH
REDUCE_BATCH
INCREASE_BATCH
STAGGER_WORKER_YIELD
PAUSE_DEVICE
RESUME_DEVICE
```

### 执行

Dispatcher/GPU queue必须真正执行动作，并记录动作时间、持续时间和结果。

仅仅把chunk减半不构成95%闭环；它可能增加提交开销而不降低利用率。

## 3. CPU控制

- 所有允许逻辑线程均可参与；
- 禁止永久停用一个线程冒充95%；
- 可采用令牌桶、提交占空窗口、arena并发窗口、错峰worker让步或低优先级；
- 禁止所有线程同步长睡眠；
- 报告worker参与率、平均利用率、p95、目标误差和响应延迟。

## 4. GPU控制

- 控制stream/queue深度、batch、kernel时长和提交节奏；
- 不允许无界排队；
- 多GPU独立控制；
- 显示GPU优先短batch；
- 无GPU采样接口时不得宣称真实95%控制完成。

## 5. MemoryBudget配置注入

`MemoryBudgetConfig` 必须由正式 RuntimeConfig/配置文件注入，禁止在 `Dispatcher::configure()` 中悄悄重新创建默认配置覆盖用户值。

每个动作必须有实际处理路径：

- `StopNewSubmit`：停止该资源新claim；
- `ShrinkBlock`：重新计算未开始块大小；
- `ReleaseCache`：释放可重建缓存并记录字节；
- `LowMemoryPath`：进入已注册低内存实现；
- `FallbackOtherDevice`：重新评估未开始块；
- `Fail`：明确错误并保留准确coverage。

动作枚举只被转成字符串不算实现。

## 6. 验收

持续工作负载测试 50%、80%、95%、100%目标：

- 真实采样或 `estimated=true`；
- 平均、p95、控制窗口和容差；
- 控制动作序列；
- 所有CPU worker参与；
- GPU队列水位；
- RAM/VRAM峰值；
- 状态查询、取消和系统响应；
- HardwareProfile hash不变。

不能只向控制器人工输入0.92/0.99后宣称资源控制通过。

报告schema见 `schemas/resource_control_report.schema.json`。
