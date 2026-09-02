# ACR聚焦版检查表

## 范围

- [ ] 只使用`feature/astrocompute-runtime`
- [ ] 现有算法、OpenMP、Pipeline、Stage1/2和正常CLI零修改
- [ ] 低负载、解析、元数据和稀疏小任务未接入ACR
- [ ] 当前生产目标仅为积分/Drizzle类重负载像素算法
- [ ] 旧20—26号计划未继续作为执行依据

## 执行链

- [ ] CPU与真实CUDA DeviceExecutor可用
- [ ] 同一Operation注册CPU实现和GPU launcher
- [ ] host lambda不会被误当GPU kernel
- [ ] PartitionContract明确输出所有权或私有partial合并
- [ ] WorkToken稳定且每块恰好一次
- [ ] actual统计来自真实completion

## 精简Benchmark

- [ ] launch/event、H2D、D2H已测
- [ ] Dense accumulate FP32已测
- [ ] Dense accumulate FP64 accumulator已测
- [ ] Pixel reduction FP64 accumulator已测
- [ ] Drizzle-like scatter/accumulate已测
- [ ] Resident chain已测
- [ ] CPU/GPU工作量完全等价
- [ ] quick不生成生产Profile
- [ ] standard Profile通过schema和简单留出验证
- [ ] 未继续扩张无关通用Benchmark族

## 混合路由

- [ ] 无cpu_share/gpu_share或固定比例
- [ ] CPU/GPU使用独立块大小
- [ ] 共享pending工作池动态claim
- [ ] host与resident使用不同GPU收益阈值
- [ ] 边际收益门可停止慢设备新claim
- [ ] 尾段动态缩块，无固定70/30
- [ ] 至少一个测试中CPU和GPU真实完成量均大于0
- [ ] AutoMixed中位耗时距实测最佳模式不超过10%
- [ ] Mixed无收益时Auto允许自然退化

## 驻留与传输

- [ ] Buffer跟踪Host/Device/Both与dirty状态
- [ ] 共享输入不为每个GPU块重复整帧上传
- [ ] 连续GPU算子中间结果保持resident
- [ ] resident chain只有必要的一次上传和最终下载
- [ ] pinned staging/双缓冲行为可审计
- [ ] transfer字节与实际操作一致

## 内存预算

- [ ] 无CPU/GPU精确利用率控制残留
- [ ] RAM、pinned、每GPU VRAM独立记账
- [ ] 比例上限和固定reserve均生效
- [ ] claim前峰值估算包含workspace/staging/partial/merge
- [ ] Shrink后循环重估
- [ ] Reuse/Release/Wait/Fallback/Fail路径真实执行
- [ ] OOM注入无泄漏、漏算和死锁

## 测试与交付

- [ ] CPU-only构建通过
- [ ] CUDA真实设备测试通过
- [ ] CPU/GPU/Mixed结果符合数值容差
- [ ] ASan/UBSan真实开启
- [ ] compute-sanitizer memcheck/racecheck通过
- [ ] 外部进程与硬件等待均有超时
- [ ] 全量测试0 failed、0 timeout，SKIPPED准确
- [ ] path guard PASS
- [ ] Evidence来自单一干净HEAD
- [ ] 包含完整源码快照、原始日志、manifest和SHA256
- [ ] 合并后普通AstroCS零ACR副作用
