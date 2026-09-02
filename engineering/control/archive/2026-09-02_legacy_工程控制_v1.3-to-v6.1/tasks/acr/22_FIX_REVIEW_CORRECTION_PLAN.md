# Fix Review 专项纠正计划

本文件是当前最高优先级执行入口，覆盖此前对Commit F“已完成”的错误描述。继续使用唯一分支`feature/astrocompute-runtime`，不改任何AstroCS业务算法。

## 一、冻结事实

当前实现仍是CPU执行器：GPU数量只影响动态块大小，工作块仍以`"cpu"`领取。不得宣称真实Mixed、GPU动态claim、95%闭环或可合并main。

## 二、必须保留

- TaskDescriptor、HardwareProfile、CostEstimator的数据结构；
- oneTBB CPU backend、ISA探测和CPU基线；
- predicted/actual字段分离方向；
- CPU/GPU利用率采样器、MemoryBudget接口；
- 经典实验、Benchmark框架与故障回退测试骨架。

## 三、必须立即重构

### F-fix 5：稳定并发工作池

1. 禁止动态增长`std::vector<WorkBlock>`后返回裸指针；
2. `block_id`必须与稳定槽位一一对应，不能在锁外分配ID、锁内乱序插入；
3. claim返回值令牌：`WorkToken{id, begin, end, generation, device_id}`；
4. 状态存储采用预分配槽位、分段池、deque稳定节点或受控索引表；
5. 所有容器结构修改和遍历必须满足C++并发规则；
6. 添加强制线程交错测试，覆盖“线程B先插入、线程A后插入”；
7. 100轮以上高并发压力测试验证无重叠、无遗漏、每块恰好完成一次。

提交建议：`fix(acr): make dynamic work pool concurrency-safe`

### F-fix 6：统一真实DeviceExecutor

定义内部接口，不泄漏第三方类型：

```cpp
class DeviceExecutor {
public:
    virtual DeviceId id() const = 0;
    virtual bool available() const = 0;
    virtual QueueState queue_state() const = 0;
    virtual SubmitResult submit(WorkToken, KernelInvocation) = 0;
};
```

必须实现：

- CPU executor：oneTBB/ISA；
- CUDA executor：真实kernel提交、event完成、错误回传；
- 后续HIP/SYCL以插件方式扩展。

Dispatcher不得直接假定`cuda:0`，实际设备ID来自executor完成事件。

提交建议：`refactor(acr): introduce real per-device executors`

### F-fix 7：CostEstimator驱动每设备claim

1. 为每个设备保留自己的预测吞吐、启动、传输、驻留和推荐块；
2. 不能从preferred device取一个统一chunk给所有设备；
3. 每个空闲executor根据自己的预计完成时间领取不同大小工作块；
4. 设备忙时不等待：其他空闲设备继续领取；
5. 尾部根据remaining、设备速度和活跃worker动态收缩；
6. 禁止用户提供CPU/GPU比例。

建议API：

```cpp
WorkToken claim_next(DeviceId device,
                     std::size_t requested_items,
                     const ClaimContext& state);
```

提交建议：`feat(acr): drive work claims from per-device cost estimates`

### F-fix 8：接通真实Mixed与报告

必须证明同一任务中：

- CPU完成部分工作块；
- 至少一个真实GPU完成部分工作块；
- 每块恰好一次；
- 实际设备统计由completion event生成；
- 推荐GPU但GPU失败时，报告如实显示CPU fallback；
- 多GPU时按真实device_id分别统计。

无兼容GPU工具链时只能SKIPPED，不得完成本阶段，也不得合并main。

提交建议：`feat(acr): execute mixed workloads on real cpu and gpu workers`

### F-fix 9：资源闭环重做

CPU/GPU目标利用率默认0.95，所有CPU线程均可参与。控制动作必须实际改变执行：

- 调整提交窗口；
- 调整每设备队列深度；
- 调整下一次claim块大小；
- 错峰yield或短等待；
- GPU stream/批次节流；
- 带迟滞的gate close/open，禁止永久停门后直接返回未完成任务。

`cached_batch_size`必须实际使用。MemoryBudget动作必须有真实行为：

- StopNewSubmit：暂停并可恢复；
- ShrinkBlock：改变后续claim大小；
- ReleaseCache：调用注册的缓存释放hook；
- LowMemoryPath：进入明确实现的低内存路径，否则报告unsupported；
- FallbackOtherDevice：把未开始块交给其他合格设备；
- Fail：准确保留coverage和错误。

提交建议：`feat(acr): implement recoverable utilization and memory backpressure`

### F-fix 10：真实验收与唯一Evidence

必须执行：

1. CPU目标50/80/95/100%，每档持续至少30秒；
2. GPU目标50/80/95/100%，每档持续至少30秒；
3. 报告平均、P95、最大偏差、响应性和控制动作；
4. CPU-only、GPU-only、真实Mixed、设备预忙、设备故障；
5. ASan/UBSan实际开启；适用时TSan；
6. 所有外部命令设置明确超时，长测试不得以TIMEOUT计PASS；
7. 清空旧Evidence，从同一干净HEAD一次生成；
8. `git status --porcelain`必须为空；
9. result/source snapshot/git log tip/evidence HEAD完全一致；
10. 使用UTF-8安全的Python SHA-256生成与复核工具。

提交建议：`test(acr): produce single-head real mixed and resource-control evidence`

## 四、强制测试案例

### 1. 动态池交错测试

使用barrier强制：线程A先取得ID后暂停，线程B取得下一ID并先完成插入。验证ID、槽位、状态和完成块完全对应。

### 2. 多设备claim模型测试

使用CPU executor和可控mock executor验证每个设备按自身推荐块领取；mock只验证调度，不能替代真实GPU验收。

### 3. 真实Mixed覆盖

至少10000个工作项，CPU和GPU均完成非零工作；总覆盖恰好一次；输出与标量参考一致。

### 4. 设备忙碌测试

人为占用GPU队列，验证CPU继续领取；占用CPU worker，验证GPU继续领取。不得固定比例。

### 5. 资源闭环测试

每档持续负载，保存原始采样序列、控制动作、平均和P95。只检查“采样非空”不算通过。

### 6. Evidence一致性测试

自动检查：

- 工作树干净；
- HEAD字段唯一；
- 无旧HEAD文件；
- 无TIMEOUT被归类为PASS；
- SHA清单可在含中文路径的环境复核。

## 五、合并门禁

以下任一未完成，禁止合并main：

- 动态工作池并发安全；
- 真实GPU executor进入Dispatcher；
- 真实CPU+GPU Mixed；
- 95%持续负载闭环；
- ASan/UBSan；
- 单一干净HEAD Evidence；
- 全部测试无失败、无未处理TIMEOUT。
