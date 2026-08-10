# AstroCS ACR 工程控制包

唯一开发分支：`feature/astrocompute-runtime`  
包名固定：`AstroCS_ACR_Control_Package.zip`  
状态：聚焦底层支线最后纠正；Phase1、真实积分和Drizzle接入不在本轮范围。

## 1. 唯一目标

ACR只为积分、Drizzle及后续少数已证明高耗时、规则并行、逐像素/逐样本的核心算法提供CPU+GPU动态混合执行。

解析、元数据、稀疏低负载查询及其他简单任务继续使用现有OpenMP/CPU实现，不接入ACR。

## 2. 已经确认可保留

- KernelRegistry、KernelInvocation和CPU/CUDA DeviceExecutor；
- 真实CUDA执行、动态共享工作池、稳定WorkToken与attempt；
- 对象层级OperationProfile读取与完整roundtrip；
- 候选块实测、每Operation独立误差记录；
- Auto在worker创建前执行GPU路径门；
- reduction/drizzle私有partial的合成正确性路径；
- RAM/VRAM预算骨架、pinned容量记账、compute-sanitizer；
- Phase1及现有业务算法零修改。

## 3. 当前仍未完成

ACRFocusedV2评审确认以下硬门禁：

1. 收益交叉点将`fixed_us`与`ns_per_item`直接相除，阈值约缩小1000倍；
2. `qualified`与“GPU有收益”错误绑定，顶层Profile又在部分Operation不合格时仍写`qualified`；
3. GPU推荐块被收益阈值错误压缩，4M推荐块实际可退化到131K；
4. 当前慢设备参与条件等价于“吞吐差不超过5%”，不能实现真正的异速CPU+GPU混合；
5. Dispatcher生产CUDA launcher仍逐块H2D/D2H，resident接口只在独立桥接测试中使用；
6. 所有host路径均不合格时，Auto无法自动建立首个resident输入，形成驻留启动死路；
7. Auto性能测试使用了未qualified的Dense Operation，实际没有证明OperationProfile驱动的Auto Mixed；
8. partial容量由调用者按固定4096槽猜测，运行时没有公开/托管真实槽位容量；
9. pinned ledger只有记账，没有真实pinned staging分配与复用；
10. 源码HEAD为`e107061`，Evidence HEAD为`c82013e`，最终源码未被完整测试覆盖。

完整依据见`audits/ACR_FOCUSED_V2_REVIEW.md`。

## 4. 本轮只做

- 统一成本单位并重算真实收益阈值；
- 分离“Operation画像合格”与“GPU路径有收益”；
- 用预测完工时间差决定CPU/GPU是否继续claim，而不是5%速率门；
- 将recommended chunk、profitability threshold和tail chunk彻底分离；
- 把真实resident device buffer接入Dispatcher与CUDA launcher；
- 建立可显式预取/保留/最终物化的最小驻留契约；
- 让ACR托管或明确返回partial槽位容量并保证retry清零；
- 增加真实Dispatcher驻留、Auto Mixed和传输次数验收；
- 从最终单一干净HEAD重做Evidence。

## 5. 明确不做

- 不修改Phase1、积分、Drizzle、HISS、Pipeline和CLI；
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
继续feature/astrocompute-runtime分支，读00_READ_FIRST和08号计划；修复收益单位、真实驻留执行与混合完工时间分块，不改算法。
```
