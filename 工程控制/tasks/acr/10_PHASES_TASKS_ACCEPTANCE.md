# 底层开发阶段、任务与验收

## Phase 0：现有分支冻结审计

- 继续唯一 `feature/astrocompute-runtime`；
- 记录 base、HEAD、remote、工作区和工具链；
- 建立算法路径 guard；
- 核对 AuditPack，形成代码保留/删除/迁移清单；
- 所有命令设置超时。

验收：审计报告、路径guard、保留代码清单、没有新仓库/版本分支。

## Phase 1：数据模型和调用链纠正

- 删除/迁移 `RouteProfile`、`preferred_backend`、`routes.json`；
- 新增 `DeviceProfile/HardwareProfile`；
- 新增 `TaskTraits/TaskDescriptor`；
- 公共 API 接通 `CostEstimator → Dispatcher → Backend`；
- 无画像明确 CPU fallback。

验收：OperationId/traits不再被忽略；无固定业务路由；schema测试通过。

## Phase 2：CPU画像

- oneTBB CPU runtime保留；
- hwloc/cpu_features保留；
- baseline/SSE/AVX/AVX2/AVX-512真实变体；
- 算术、STREAM、reduction、线程、NUMA曲线；
- Google Benchmark或ADR批准等价框架。

验收：每个声明ISA有独立实现和计时；不支持ISA不执行；baseline始终可用。

## Phase 3：GPU backend与画像

- 修复真实目标工具链；
- 至少一个GPU backend构建/运行；
- buffer/queue/event/copy；
- BabelStream、H2D/D2H、算术、reduction、卷积、atomic、branch；
- CPU-only不依赖GPU SDK。

验收：真实设备日志；无GPU时SKIPPED，不得模拟PASS。

## Phase 4：Profile拟合与CostEstimator

- 原始结果、指纹、schema；
- log尺寸曲线；
- 留出验证和置信度；
- queue/launch/transfer/compute/merge成本；
- missing/stale/partial/corrupt处理；
- 正式运行只读。

验收：不是总分或固定比例；预测误差报告完整。

## Phase 5：动态Dispatcher

- CPU/单GPU/多GPU共享未开始工作池；
- 画像推算块大小；
- guided尾部收缩；
- coverage恰好一次；
- 设备忙闲和故障回收；
- 驻留与迁移成本。

验收：真实CPU+GPU并发；无用户比例；profile hash不变。

## Phase 6：资源控制

- CPU/GPU利用率软目标；
- RAM/VRAM限制；
- 真实指标或明确估算；
- 所有CPU线程可参与；
- 控制器不修改画像。

验收：50/80/95/100目标持续负载报告；不能用人工利用率输入代替。

## Phase 7：经典实验与可靠性

执行 `17_CLASSIC_EXPERIMENT_SUITE.md`：正确性、画像、动态Mixed、故障、持续运行和成熟库adapter。

验收：必选通过；硬件不可用SKIPPED；ASan/UBSan实际开启；无伪测试。

## Phase 8：统一Evidence和main合并

- 同一干净HEAD一次生成全部包；
- feature合入最新main并回归；
- `--no-ff` 合并；
- 合并后CPU-only、主线和无副作用测试；
- 完整源码快照与Merge Report。

验收：ACR进入main但dormant；算法目录零改动；所有commit字段一致。

## 共同要求

- 增量修正，不推倒有效代码；
- 不提交空壳/TODO冒充完成；
- 每阶段原子提交；
- 无真实硬件不宣称运行通过；
- 不覆盖用户未提交改动；
- 失败即停止合并，不停止记录和交付证据。
