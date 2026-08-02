# ACR底层支线检查表

## 单一实现

- [ ] 继续使用唯一`feature/astrocompute-runtime`
- [ ] 未创建版本分支、新仓库或第二套ACR
- [ ] 控制包无发布版本号
- [ ] 现有有效代码增量保留

## 范围

- [ ] 算法目录零修改
- [ ] OpenMP未删除/注释
- [ ] Pipeline/Orchestrator/正常CLI未改变
- [ ] path guard通过

## API与路由

- [ ] TaskClass/TaskTraits
- [ ] Public API真实进入CostEstimator/Dispatcher/backend
- [ ] 无CPU/GPU share API或配置
- [ ] Hardware Profile替代固定weight route
- [ ] 无画像CPU-only+警告
- [ ] Profile只读、无在线学习

## 开源复用

- [ ] oneTBB或已有CPU runtime
- [ ] hwloc
- [ ] cpu_features
- [ ] Google Benchmark或经ADR批准等价物
- [ ] portable GPU backend PoC和选择ADR
- [ ] 成熟FFT/BLAS/scan adapter
- [ ] dependency lock和许可证

## Benchmark画像

- [ ] CPU ISA和线程曲线
- [ ] FP32/FP64算术
- [ ] STREAM式CPU内存
- [ ] BabelStream式GPU显存
- [ ] H2D/D2H/pinned
- [ ] reduction
- [ ] direct/separable/FFT卷积
- [ ] gather/scatter/atomic/histogram
- [ ] branch/work variance
- [ ] submit/launch/event/alloc/merge
- [ ] 模型拟合和留出误差

## 动态执行

- [ ] CPU baseline和ISA变体真实执行
- [ ] 至少一个真实GPU backend
- [ ] CPU/GPU共享工作池
- [ ] 多GPU可用时验证
- [ ] guided尾部收缩
- [ ] coverage恰好一次
- [ ] 数据驻留计入决策
- [ ] 设备失败回收未开始块

## 资源和可靠性

- [ ] 95%是利用率目标，不是比例或少线程
- [ ] 所有CPU线程可参与
- [ ] RAM/VRAM限制
- [ ] ASan/UBSan实际开启
- [ ] TSan适用路径
- [ ] 持续运行、取消、泄漏和故障注入

## 合并与交付

- [ ] CPU-only和主线回归
- [ ] 普通启动无ACR副作用
- [ ] Evidence从同一干净HEAD生成
- [ ] summary/JSON/log/manifest commit一致
- [ ] 完整源码快照，不只diff
- [ ] `--no-ff`合并main备用
- [ ] 合并后再次测试
