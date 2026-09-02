# ACR底层支线检查表

## 单一实现与范围

- [ ] 只使用 `feature/astrocompute-runtime`
- [ ] 无版本分支、新仓库或第二套ACR
- [ ] 算法目录、OpenMP、Pipeline和正常CLI零修改
- [ ] path guard状态PASS且exit code=0

## Commit F事实纠正

- [ ] 提交能力描述已降级为中间基础
- [ ] predicted与actual字段分离
- [ ] actual primary backend来自真实done工作量
- [ ] coverage含PENDING/CLAIMED/DONE/FAILED
- [ ] 失败/取消/未开始块不标DONE
- [ ] 固定70/30实验不再称为guided

## 真实调用链

- [ ] API进入TaskDescriptor
- [ ] CostEstimate.per_device参与每次claim
- [ ] Dispatcher维护Shared Pending Pool
- [ ] Backend completion回传coverage和统计
- [ ] 无画像CPU-only+警告
- [ ] HardwareProfile运行时只读

## 动态执行

- [ ] CPU baseline和ISA真实执行
- [ ] 至少一个真实GPU backend
- [ ] CPU/GPU动态claim，无固定份额
- [ ] guided依据remaining/画像/队列动态缩块
- [ ] GPU预忙时CPU继续，CPU预忙时GPU继续
- [ ] 故障只回收未完成块
- [ ] 每块恰好一次
- [ ] actual report与backend日志一致

## 资源与内存

- [ ] 95%是利用率目标，不是任务比例
- [ ] 所有CPU线程可参与
- [ ] CPU/GPU真实采样或明确estimated
- [ ] 控制器输出并执行submit/queue/batch/yield动作
- [ ] RuntimeConfig注入MemoryBudget
- [ ] StopNewSubmit实际执行
- [ ] ShrinkBlock实际执行
- [ ] ReleaseCache实际执行
- [ ] LowMemoryPath实际执行或明确不支持
- [ ] FallbackOtherDevice实际执行
- [ ] Fail保留准确coverage
- [ ] 50/80/95/100持续负载报告

## Benchmark和可靠性

- [ ] CPU ISA/线程/FP32/FP64
- [ ] STREAM/BabelStream/H2D/D2H
- [ ] reduction/卷积/gather/scatter/atomic/branch
- [ ] 模型留出验证
- [ ] ASan/UBSan实际开启
- [ ] TSan适用路径
- [ ] 持续运行、取消、泄漏和故障注入

## Evidence与合并

- [ ] Evidence来自单一干净HEAD
- [ ] result/source snapshot/git log tip一致
- [ ] 每项保存完整命令、环境、超时、exit code和原始日志
- [ ] 无一行摘要替代原始日志
- [ ] SKIPPED原因准确
- [ ] 完整源码快照，不只diff
- [ ] 最新main回归通过
- [ ] 合并后普通启动无ACR副作用
- [ ] 全部完成后才允许 `--no-ff` 合并main


## Fix Review新增硬门禁

- [ ] 动态池不返回指向并发增长容器的裸指针
- [ ] block id与稳定槽位永久一致
- [ ] 强制线程交错和高并发压力测试通过
- [ ] CPU与每个GPU均有真实DeviceExecutor
- [ ] 每设备使用自己的CostEstimate和claim块大小
- [ ] GPU数量不再只影响CPU块大小
- [ ] actual设备ID来自真实completion event，无`cuda:0`硬编码
- [ ] submit gate可关闭、等待并恢复，不遗留未完成块
- [ ] cached batch/queue决策实际进入执行链
- [ ] MemoryBudget每种声明动作均实际执行或明确unsupported
- [ ] 50/80/95/100持续负载每档至少30秒
- [ ] Fix Evidence无旧HEAD混装
- [ ] `git status --porcelain`为空
- [ ] 无TIMEOUT被统计为PASS
- [ ] UTF-8路径SHA-256可完整复核
