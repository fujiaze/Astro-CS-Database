# ACR聚焦版检查表

## 范围

- [ ] 仅使用`feature/astrocompute-runtime`
- [ ] Phase1、现有算法、OpenMP、Pipeline和CLI零修改
- [ ] 低负载、解析、元数据和稀疏小任务未接入
- [ ] 未扩展无关通用Benchmark

## 执行链

- [ ] CPU/CUDA DeviceExecutor真实可用
- [ ] WorkToken每块恰好一次
- [ ] actual统计来自真实completion
- [ ] ForcedMixed与Auto语义分离

## OperationProfile

- [ ] 使用对象层级JSON解析
- [ ] CPU/GPU同名字段不会混淆
- [ ] GPU数组和完整fingerprint可读取
- [ ] 完整roundtrip逐字段一致
- [ ] CPU/GPU/host/resident曲线语义分开
- [ ] fixed开销未重复计入
- [ ] 推荐块来自候选块实测
- [ ] holdout误差真实写回
- [ ] 每个Operation独立qualified
- [ ] 无交叉点时eligible=false且阈值=null
- [ ] 指纹来自真实环境和kernel hash

## 混合路由

- [ ] 无固定cpu_share/gpu_share
- [ ] Auto在worker启动前执行eligibility
- [ ] 小任务加载Profile后不会错误启动GPU
- [ ] Auto不强制每设备首块
- [ ] 慢设备停止后不会定时强制重入
- [ ] CPU/GPU独立实测块
- [ ] 共享pending域动态claim
- [ ] 尾段缩块
- [ ] Auto性能距实测最佳≤10%

## Reduction与Drizzle

- [ ] CPU worker不并发写共享bins
- [ ] CPU/GPU不无协议写同一累计数组
- [ ] reduction partial按稳定token容量分配
- [ ] retry attempt安全
- [ ] 明确merge且成本可报告
- [ ] CPU/GPU/ForcedMixed/Auto结果均正确

## 真实驻留

- [ ] Buffer有stable key、generation、bytes和access mode
- [ ] ResidencyManager连接真实device allocation
- [ ] mark_uploaded/downloaded来自实际传输
- [ ] 只读共享输入不重复上传
- [ ] 中间GPU输出保持resident
- [ ] CPU需要时才D2H
- [ ] cache释放同步更新状态和VRAM
- [ ] report传输次数/字节与桥接一致

## 内存

- [ ] RAM、pinned ledger、每GPU VRAM独立
- [ ] claim前峰值含workspace/partial/merge
- [ ] resident复用不重复计费
- [ ] Shrink后重估
- [ ] Release/Wait/Fallback/Fail真实执行
- [ ] OOM无泄漏、漏算和死锁

## 测试与交付

- [ ] CPU-only和CUDA构建通过
- [ ] focused全路径正确性通过
- [ ] ASan/UBSan真实开启或准确说明工具链限制
- [ ] compute-sanitizer通过
- [ ] 0 failed、0 timeout
- [ ] path guard PASS
- [ ] Evidence来自最终单一干净HEAD
- [ ] 包含源码、原始日志、manifest和SHA
- [ ] dormant合并后普通AstroCS零副作用
