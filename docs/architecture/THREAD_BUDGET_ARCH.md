# 全局 Thread Budget 与执行架构

> 上游：`docs/ASTROCS_DESIGN.md` §8（软件架构）、§9（CPU 后端与资源）

## 1 全局 thread budget

- **预算对象与字段合同**正本 = `docs/architecture/execution_options_contract.md`；
  **并行轴分配与轴不变式**正本 = `docs/architecture/THREADING_MODEL.md`「并行轴分配」。
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
| Phase1 读入 | aio 顺序读（1 线程） | 校准 / 检测 / PSF（逐帧行带） | 读→算双缓冲（深度=2） | 队列满时读阻塞 |
| Phase1 WCS / 测光 | header KV 读写 | ipv 三角 / 投票（帧内） | — | 同步（见 §3） |
| Phase1 Drizzle / HiPS | tile 原子写 | overlap / accumulate（候选） / normalize（归并） | tile 写异步（深度=1） | 落盘完成才 release tile |
| Phase2 | UPM 模型读 / 写（串行） | sampler（串行 reference）→ rejection（行带）→ integration（行带） | — | 同步链 |
| Phase3 | HiPS tile 读（cache） | 反向映射 + 采样（行带） | tile cache 预取（深度=1） | cache 上界 O(cache_tiles·W²) |

- 每阶段在 run manifest 记录 `budget_alloc`（分配快照）；资源监控以同一对象为唯一事实来源
  （见 `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`）。

## 3 异步与取消架构

- **async 仅两类**：I/O pipeline（读 / 写双缓冲）与后台服务；科学计算无 async/future
  （消除嵌套并行与不可预算并发）。
- **取消**：CLI JSONL cancel → 全局取消标志（原子）→ 各内核取消点（逐内核取消点见
  `docs/science/algorithms/` 各算法文档的取消点条款）；取消后预算立即回收，取消单元不落盘。
- **嵌套并行**：外层已并行则内层串行（科学内核只在 parallel region 外开并行；
  I/O 线程与科学内核分属不同线程）；唯一豁免 = watchdog（独立预算）。

## 4 并发正确性合同

- 浮点归约顺序冻结（`docs/architecture/THREADING_MODEL.md`「确定性锚点」全部有效）：
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

- 文档：`docs/architecture/THREADING_MODEL.md`（分层 + 轴分配 + 确定性锚点）、
  `docs/architecture/EXECUTION_MODEL.md`、`docs/architecture/ASYNC_IO_CONTRACT.md`、
  `docs/architecture/OWNERSHIP_AND_LIFETIME.md`、
  `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`。
