# 冻结需求

## 1. 产品定位

ACR 是 AstroCS 的独立底层异构计算黑箱。本支线只建设底层，不改造任何真实算法。未来只有经过实测确认的密集模块才会以极小接口改动接入。

## 2. 唯一路由原则

> 离线通用硬件画像 Benchmark + TaskDescriptor 成本推算 + 运行时工作保持动态派发。

禁止：

- 用户配置 CPU/GPU 任务百分比、权重或首选设备；
- 在公共 API 中出现 `cpu_share`、`gpu_share`、`device_weight`；
- 生成每业务 kernel 的固定 `preferred_backend`；
- 将 AXPY、GEMM 或任意单一总分直接映射为设备路由；
- 正式运行在线训练、更新或写回 HardwareProfile。

允许：

- Benchmark 对设备、ISA、线程、精度、尺寸和驻留进行有限系统测量；
- 运行时读取当前队列、设备健康、数据驻留、容量和利用率；
- 空闲设备动态领取未开始块；
- 根据剩余工作量缩小尾部块；
- 资源控制器调节提交节奏，但不得修改性能画像。

## 3. 本分支必须完成

- `OperationId`、`TaskTraits`、`TaskDescriptor`、Buffer、Event；
- oneTBB CPU runtime 与 baseline；
- CPU ISA 发现、门禁和多版本实现；
- 至少一个真实 GPU backend；
- HardwareProfile 的原始数据、schema、拟合、指纹、验证和只读加载；
- CostEstimator；
- CPU、单 GPU、多 GPU 的动态共享工作池；
- 数据驻留、传输成本与局部结果合并；
- 默认约 95% 的 CPU/GPU 利用率控制及 RAM/VRAM 上限；
- 经典实验、故障注入、Sanitizer 和长期运行；
- CPU-only 构建和 dormant main 合并。

## 4. 本分支禁止

- 修改任何现有算法；
- 删除、替换、注释现有 OpenMP；
- 修改 PipelineFrame、Orchestrator 或正常 CLI；
- 用真实 Drizzle/积分代码作为底层 POC；
- 普通 AstroCS 启动时初始化 ACR；
- 创建版本分支、新仓库或第二套实现。

## 5. 未标定与画像状态

### Missing

- 正常运行；
- 保守纯 CPU 多线程；
- 非阻断警告提示空载执行 benchmark；
- 不启用 GPU 自动路由。

### Valid

- 只读加载画像；
- 允许画像驱动的 CPU/GPU 动态派发。

### Stale

- 发出警告；
- 默认允许继续使用旧画像，或按配置回退 CPU-only；
- 不强制重新 benchmark。

### Partial/Corrupt

- 不使用失效能力族；
- 相关任务回退安全路径；
- 记录原因，不静默伪装完整。

普通 AstroCS 未调用 ACR 时不得显示以上警告。

## 6. Benchmark

- 开始前只提示用户保持空载，不扫描或关闭程序；
- 重点 FP32，同时覆盖 FP64 和 FP32 输入+FP64累加；
- 测试 CPU ISA/线程、RAM/缓存/NUMA、VRAM、H2D/D2H、算术、归约、卷积、不规则访问、原子、分支和固定开销；
- 使用经典有限组合和粗扫→补点，不做维度笛卡尔积；
- 保存原始样本、统计量、模型和留出验证；
- 所有外部进程、编译、测试和硬件等待必须有超时；
- 正式运行不写回画像。

## 7. 资源控制

- CPU/GPU 利用率目标默认 `0.95`；
- 所有 CPU 线程均可参与；
- 95% 不是少开线程，也不是任务份额；
- RAM/VRAM `0.95` 是容量上限；
- 控制器通过队列深度、批次、提交节奏和让步控制；
- 利用率 API 不可用时允许可审计估算，但必须明确标注。

## 8. 数值策略

- 默认 IEEE 754 FP32；
- CPU/GPU 正常末位差异可接受，不要求逐位一致；
- 默认关闭破坏语义的 fast-math；
- 特殊任务可声明 FP64、FP64 accumulator、确定性合并；
- 底层只做正确性门禁，不建立沉重逐设备精度认证。

## 9. 开源复用

优先复用 oneTBB、hwloc、cpu_features、Google Benchmark、成熟 FFT/BLAS/scan 原语；GPU portable 层必须通过 ADR 和目标工具链 PoC 决定。公共 API 不得暴露第三方类型。

## 10. 合并

全部验收通过后才允许 `--no-ff` 合并到 `main`。合并后 ACR 仅备用，现有算法不调用，普通启动无副作用。任何真实算法改造必须另开后续分支。
