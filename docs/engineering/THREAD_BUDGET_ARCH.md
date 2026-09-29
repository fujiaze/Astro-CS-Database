# 全局 Thread Budget 与执行架构

> 上游：`docs/ASTROCS_DESIGN.md` §8（软件架构）、§9（CPU 后端与资源）

## 1 全局 thread budget

- **预算对象与字段合同**正本 = `docs/engineering/execution_options_contract.md`；
  **并行轴分配与轴不变式**正本 = `docs/engineering/THREADING_MODEL.md`「并行轴分配」。
  本节只写与执行架构相关的正向条款，不复制两处合同文本。
- 预算基量 `available_cpus = affinity ∩ cgroup ∩ Job Object`（不是机器总核数）；
  预算按 `phase → stage → kernel` 层级显式分配，任何时刻 Σ(活动 worker) ≤ budget。
- 分配策略（冻结）：串行 I/O 与控制面恒 1 线程；CPU 内核获得 `min(budget, kernel_block_hint)`；
  异步 I/O pipeline 恒 1 专用线程；后台服务（watchdog / 资源监控 / progress 日志）
  恒 1 线程 + 独立小预算（不入科学预算池）。
- backend 线程池与模块内 OpenMP 线程数一律经 host callback 注入运行时（由预算派生），
  取值与 `omp_set_num_threads` 无关；**全仓线程数由预算派生**。

## 2 每阶段执行画像（串行 I/O · CPU task · async pipeline · backpressure）

| 阶段 | 串行 I/O | CPU task（并行粒度） | async pipeline | backpressure |
|---|---|---|---|---|
| Phase1 各节点 | aio 读（每帧一次，句柄线程私有） | 两轴：帧级 × 帧内 | — | 内存闸门限帧在飞数 |
| Phase1 Drizzle / HiPS | tile 原子写 | overlap / accumulate / 归并 | — | tile 写为原子事务 |
| Phase2 coverage / UPM | UPM 模型读 / 写（串行） | coverage 重叠图 union；UPM solve | — | 同步链 |
| Phase2 sampler | 控制采样点读（per-cell） | per-control-cell | — | 池满即阻塞 |
| Phase2 block / reject / integrate | tile 原子写 | per-tile | — | 每任务只写自己的结果槽 |
| Phase3 | HiPS tile 读（cache） | 反向映射 + 采样 | — | 内存 ∝ 子块大小 |

各行的并行粒度与同步点：

- **Phase1 各节点**（校准 / 检测 / PSF / 解算 / 测光 / 入库）：`p1_parallel_for` 两轴——帧级
  `frame_w = min(lease, p1_memory_cap)` × 帧内 `inner_omp = max(1, lease / in_flight)`；
  不变式 `in_flight × inner_omp ≤ lease`。
- **Phase1 Drizzle / HiPS**：overlap / accumulate 按确定性 stripe 分片，归并按 stripe 索引升序左折叠；
  失败或取消的单元不落正式路径。
- **Phase2 coverage / UPM**：coverage 重叠图 union 为 tile 级；UPM solve = IRLS 主迭代串行 +
  compute_raw / per-obs 权重 per-call 池（`granted_workers`）。
- **Phase2 sampler**：per-control-cell，`next_c.fetch_add(1)` 动态认领，`cpu_workers = budget.max_workers`。
- **Phase2 block / reject / integrate**：`p2_parallel_for`（std::thread + 原子计数动态认领，无 OpenMP、无 barrier），
  跨任务零浮点归约。
- **Phase3**：子块流式 + 有界队列背压，内存占用 ∝ 子块大小、与总图大小无关。

- 每阶段在 run manifest 记录 `budget_alloc`（分配快照）；资源监控以同一对象为唯一事实来源
  （见 `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md`）。

## 3 异步与取消架构

- **async 仅两类**：I/O pipeline（读 / 写双缓冲）与后台服务；科学计算无 async/future
  （消除嵌套并行与不可预算并发）。
- **取消**：CLI JSONL cancel → 全局取消标志（原子）→ 各内核取消点（逐内核取消点见
  `docs/science/algorithms/` 各算法文档的取消点条款）；取消后预算立即回收，取消单元不落盘。
- **嵌套并行**：外层已并行则内层串行（科学内核只在 parallel region 外开并行；
  I/O 线程与科学内核分属不同线程）；唯一豁免 = watchdog（独立预算）。

## 4 并发正确性合同

- 浮点归约顺序冻结（`docs/engineering/THREADING_MODEL.md`「确定性锚点」全部有效）：
  `lib/algorithms/coverage/src/upm.cpp` 的 `compute_raw`、`lib/algorithms/coverage/src/sampler.cpp`
  的固定槽位回写、`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 的 per-stripe 路径；
  tile 合并 = **per-stripe scratch pool 累加 + 按 stripe 索引升序左折叠归约**
  （累加与归约解耦：`merge_cursor` 强制升序、归约分支独立）⇒ 浮点结合树逐位一致，
  **与线程数 / 调度顺序无关**，结果序列由 budget 快照唯一化。
  `drizzle_engine.cpp` 内的 prof 计数器合并与整数计数器 `reduction` 子句不是浮点归约锚。
- 计数器：atomic 或 thread-local 聚合；cache（UPM dense / Gaia / tile）线程安全或单线程互斥；
  无裸 data race。
- ACR 与浏览器层不接入生产：ACR 保留源码与隔离测试，不进生产构建 / 加载 / 路由 / benchmark
  （最高设计 §1.4）；HiPS Browser 属工具分类（非发布）。

## 5 静态 checker 合同（验收）

`eng/tools/arch/check_thread_budget.py`：

1. 扫描 `lib/` 生产源：`std::thread` / `std::async` / `_beginthread` / `CreateThread` 出现处
   必须在 `THREAD_BUDGET_EXEMPT` 登记表内。登记表以 checker 内 `THREAD_BUDGET_EXEMPT` 为
   **唯一事实源**，随实现演进，以实跑输出为准；`eng/tests/` 为扫描面豁免、不占登记条目。
   生产科学模块面的核心条目：`lib/algorithms/coverage/src/upm.cpp` 的 per-call 池
   （`cworkers = Runtime lease`）、`lib/algorithms/coverage/src/sampler.cpp` 的 per-call 池
   （`workers = Runtime lease`）；另有
   `lib/algorithms/integration/phase2_integrate/oracle/weight_chain_selfcheck.cpp`
   （权重链独立 Oracle 自查池）、`lib/algorithms/cosmetic/src/module_entry.cpp`
   （`omp_set_num_threads` 租约注入），以及 `lib/infrastructure/pipeline/orchestrator/` 的
   watchdog 与资源监控文件级豁免。逐条清单以实跑输出为准。
2. `omp_set_num_threads(` / `num_threads(` 字面量零容忍；
3. 未登记即 FAIL（exit 1）——保证「未登记线程创建」机器可查。

## 6 关联

- 文档：`docs/engineering/THREADING_MODEL.md`（分层 + 轴分配 + 确定性锚点）、
  `docs/engineering/EXECUTION_MODEL.md`、`docs/engineering/ASYNC_IO_CONTRACT.md`、
  `docs/engineering/OWNERSHIP_AND_LIFETIME.md`、
  `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md`。
