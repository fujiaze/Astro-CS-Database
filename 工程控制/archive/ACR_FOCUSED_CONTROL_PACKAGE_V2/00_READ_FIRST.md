# AstroCS ACR 工程控制包

唯一开发分支：`feature/astrocompute-runtime`  
包名固定：`AstroCS_ACR_Control_Package.zip`  
状态：聚焦底层支线收尾；Phase1业务算法接入不在本轮范围。

## 1. 唯一目标

ACR只为积分、Drizzle及后续少数已证明高耗时、规则并行、逐像素/逐样本的核心算法提供CPU+GPU动态混合执行能力。

解析、元数据、稀疏低负载查询和其他简单任务继续使用现有OpenMP/CPU实现，不接入ACR。

## 2. 当前已保留能力

- KernelRegistry / KernelInvocation / DeviceExecutor；
- 真实CPU与CUDA执行；
- 动态共享工作池、稳定WorkToken和attempt；
- 无固定CPU/GPU比例；
- RAM/VRAM预算骨架；
- CUDA容量与多块卷积修复；
- 业务算法零修改。

## 3. 当前尚未完成

本轮评审发现以下硬门禁：

1. 落盘OperationProfile重新读取时CPU/GPU同名字段会混淆；
2. GPU固定开销、收益阈值和qualified判定不可信；
3. resident曲线不是真实显存驻留测量；
4. Auto模式没有在worker启动前严格执行GPU收益门；
5. reduction和drizzle-like Mixed缺少私有partial与明确merge；
6. pinned staging未进入真实生产reservation；
7. Evidence不是单一最终HEAD。

完整依据见`audits/ACR_FOCUSED_REVIEW.md`。

## 4. 本轮只做

- 修正OperationProfile读取、资格和交叉点计算；
- 实测CPU、GPU resident、GPU host roundtrip及候选块；
- Auto路由只让真正有边际收益的设备进入共享池；
- 将驻留状态接到真实device allocation与传输；
- 修正reduction/drizzle私有partial和merge；
- 完成RAM/VRAM/pinned容量约束；
- 从单一干净HEAD生成Evidence。

## 5. 明确不做

- 不修改Phase1、积分、Drizzle、HISS、Pipeline和正常CLI；
- 不扩展通用gather、branch、atomic、FFT、BLAS画像；
- 不做CPU/GPU精确利用率控制；
- 不做在线学习；
- 不使用固定CPU/GPU份额；
- 不为测试Mixed而在Auto模式强制慢设备参与。

## 6. 阅读顺序

1. `01_SCOPE_AND_DECISIONS.md`
2. `02_TARGET_ARCHITECTURE.md`
3. `03_PUBLIC_API_AND_PARTITION_CONTRACT.md`
4. `04_BENCHMARK_AND_OPERATION_PROFILE.md`
5. `05_MIXED_ROUTING_AND_CHUNKING.md`
6. `06_DATA_RESIDENCY_AND_MEMORY.md`
7. `07_TEST_AND_ACCEPTANCE.md`
8. `08_CURRENT_EXECUTION_PLAN.md`
9. `09_GIT_AND_DELIVERY.md`
10. `CHECKLIST.md`

## 7. 工程启动词

```text
继续feature/astrocompute-runtime分支，读00_READ_FIRST和08号计划；修正画像判定、真实驻留与归约合并，不改算法。
```
