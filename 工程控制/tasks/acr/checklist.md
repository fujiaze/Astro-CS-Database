# ACR底层支线检查表

## 单一实现

- [ ] 使用唯一 `feature/astrocompute-runtime`
- [ ] 无版本分支、新仓库或第二套ACR
- [ ] 控制包稳定名，无V1/V2
- [ ] 有效代码增量保留

## 范围

- [ ] 算法目录零修改
- [ ] OpenMP未删除/注释
- [ ] Pipeline/Orchestrator/正常CLI未改变
- [ ] path guard通过

## 架构迁移

- [ ] 旧routes.json停止生成
- [ ] 无per-kernel preferred_backend
- [ ] HardwareProfile schema生效
- [ ] TaskDescriptor/TaskTraits生效
- [ ] API真实进入CostEstimator/Dispatcher/backend
- [ ] 无CPU/GPU share API或配置
- [ ] 无画像CPU-only+警告
- [ ] profile只读

## 开源复用

- [ ] oneTBB
- [ ] hwloc
- [ ] cpu_features
- [ ] Google Benchmark或ADR批准等价物
- [ ] GPU层工具链ADR
- [ ] 成熟FFT/BLAS/scan adapter
- [ ] dependency lock和许可证

## HardwareProfile

- [ ] CPU ISA/线程/FP32/FP64
- [ ] STREAM CPU内存
- [ ] BabelStream GPU显存
- [ ] H2D/D2H/pinned
- [ ] reduction
- [ ] direct/separable/FFT卷积
- [ ] gather/scatter/atomic/histogram
- [ ] branch/work variance
- [ ] submit/launch/event/alloc/merge
- [ ] 模型拟合和留出误差
- [ ] 指纹和stale/partial/corrupt

## 动态执行

- [ ] CPU baseline和ISA真实执行
- [ ] 至少一个真实GPU backend
- [ ] CPU/GPU共享工作池
- [ ] 多GPU可用时验证
- [ ] guided尾部收缩
- [ ] coverage恰好一次
- [ ] 驻留/传输计入成本
- [ ] 故障回收未开始块
- [ ] profile hash运行前后不变

## 资源与可靠性

- [ ] 95%是利用率目标
- [ ] 所有CPU线程可参与
- [ ] 利用率是真实或明确估算
- [ ] RAM/VRAM限制
- [ ] ASan/UBSan实际开启
- [ ] TSan适用路径
- [ ] 持续运行、取消、泄漏和故障注入

## 合并与交付

- [ ] CPU-only和主线回归
- [ ] 普通启动无ACR副作用
- [ ] Evidence同一干净HEAD
- [ ] summary/JSON/log/manifest一致
- [ ] 完整源码快照，不只diff
- [ ] `--no-ff`合并main备用
- [ ] 合并后再次测试
