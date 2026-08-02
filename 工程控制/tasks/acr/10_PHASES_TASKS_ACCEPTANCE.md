# 底层开发阶段、任务与验收

## Phase A：现有分支和依赖审计

- 继续使用现有`feature/astrocompute-runtime`；不存在才创建；
- 检查当前实现、Evidence和控制包差异；
- 检查`astro_toolkit.py`并设置所有外部命令超时；
- 建立算法路径guard；
- 形成开源依赖ADR和锁定方案；
- 删除/废弃固定CPU/GPU比例schema和测试。

验收：base/current commit、差异报告、path guard、ADR、纠正清单。

## Phase B：公共API和CPU baseline

- TaskClass/TaskTraits；
- Buffer/Event；
- parallel_for/tiles/reduce/batch；
- oneTBB CPU runtime；
- baseline scalar；
- CPU-only构建。

验收：API实际进入dispatcher，不得忽略OperationId/traits；单测和主线回归通过。

## Phase C：拓扑、ISA和CPU画像

- hwloc、cpu_features；
- baseline/SSE/AVX/AVX2/AVX-512多版本；
- STREAM式内存、算术、归约、线程和NUMA曲线；
- ISA安全门禁。

验收：不支持ISA不执行；画像可区分ISA/线程/尺寸；baseline始终可用。

## Phase D：GPU backend与GPU画像

- alpaka或经ADR批准的可替换backend；
- 至少一个真实GPU；
- device/queue/buffer/event/copy；
- BabelStream式显存；
- H2D/D2H/launch/原子/分支/卷积曲线；
- CPU-only无GPU SDK。

验收：真实硬件日志；同一经典kernel CPU/GPU正确；无硬件不得虚报。

## Phase E：Qualification和Hardware Profile

- acr-benchmark/status/report/invalidate；
- 空载提示；
- 原始数据、模型拟合、指纹和schema；
- log尺寸分段曲线；
- missing/stale/corrupt处理；
- 运行时只读。

验收：画像不是总分和固定比例；可重复生成；中断明确恢复/作废；无画像CPU-only。

## Phase F：Cost Estimator和动态Dispatcher

- 按任务类别选择画像能力族；
- 估算queue/launch/transfer/compute/merge；
- 推算最小有效块和初始块；
- CPU、单GPU、多GPU共享工作池；
- guided尾部收缩；
- coverage恰好一次；
- 故障回收未开始块。

验收：不使用用户比例；不同任务类别选择不同设备/块；CPU/GPU真实并发；设备忙时其他设备继续工作。

## Phase G：95%资源控制

- CPU/GPU利用率软目标；
- RAM/VRAM限制；
- 所有CPU线程可参与；
- 资源控制不修改画像。

验收：50/80/95/100利用率目标测试；95%不等于少线程；系统可响应。

## Phase H：经典实验和持续可靠性

执行`17_CLASSIC_EXPERIMENT_SUITE.md`：算术、内存、归约、卷积、重采样、稀疏、原子、scan、FFT/GEMM adapter、动态混合、故障和持续运行。

验收：必选全通过；不可用明确SKIPPED；ASan/UBSan等实际开启；无伪测试。

## Phase I：统一Evidence和main合并

- 从同一干净HEAD一次生成Evidence；
- summary、JSON、日志和commit完全一致；
- 更新最新main并回归；
- 按合并规范`--no-ff`合并；
- 合并后CPU-only、主线测试和无副作用检查。

验收：ACR进入main但dormant；算法目录零改动；merge report完整。

## 共同要求

- 不创建版本分支或新仓库；
- 不推倒已有有效代码，增量修正；
- 不提交空壳/TODO冒充完成；
- 每阶段原子提交；
- 所有外部进程有明确超时；
- 无真实硬件不宣称运行通过；
- 不覆盖用户未提交改动。
