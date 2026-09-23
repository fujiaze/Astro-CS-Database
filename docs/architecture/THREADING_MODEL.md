# Threading Model

> 上游：ASTROCS_DESIGN.md §8（软件架构）

## 分层

- **生产执行层（唯一）**：`scheduler` + `pipeline`（typed DAG 调度、统一线程预算、
  执行与取消；最高设计 §7.1/§8）。一个进程只有一个全局执行顺序与线程预算源。
- 科学模块：内部并行 region 由宿主按预算注入；每模块文档化 parallel/shared/
  thread-local/reduction/determinism/float accumulation order。
- 编排层由唯一 CLI 入口承担：按命令拉起对应阶段的调度器，一次调用只驱动一个阶段；唯一入口 = `astrocs.exe`（Windows）/ `astrocs`（Linux）（最高设计 §7.1/§8.1）。
- ~~ACR：work_pool + device_executor 调度；CPU reference 与 GPU 等价契约。~~
  **DORMANT**：保留源码与隔离测试，**不进生产构建/加载/路由/benchmark/发布**
  （最高设计 §8）。
- ~~浏览器：Qt 主线程 + 后台 I/O 线程；renderer 只读共享数据。~~
  **工具分类（非发布）**：HiPS Browser 是未来可视化组件，**不进产品 manifest**
  （最高设计 §7.1/§10.1）。

## 约定

- 禁止库内修改全局 OpenMP 设置；线程数由 run context 配置。
- 计数器：atomic 或 thread-local 聚合（禁止裸 data race counter）。
- 浮点累积顺序固定（确定性输出）；reduction 顺序文档化。
- cache（dense UPM、Gaia 查询缓存）必须线程安全或单线程互斥访问。

### 并行轴分配（PERF-501 冻结口径）

P1 各节点有两个可独立分配的并行轴：**帧级**（同时处理几帧，受内存闸门约束）与
**帧内 OpenMP**（每帧几条线程，受线程预算约束）。两轴之和必须 ≤ Runtime lease 给出的
预算（禁止 N×N 超额订阅），实现落点 = `p1_parallel_for(workers, n, thread_budget, body)`
（`lib/infrastructure/scheduler/src/module_adapters.cpp`）。

```
in_flight = min(n, frame_workers)                 // frame_workers = min(__workers, p1_memory_cap)
inner_omp = max(1, thread_budget / in_flight)     // thread_budget = __workers（lease）
总并行度  = in_flight × inner_omp ≤ thread_budget
```

**为什么两轴都要动**：`p1_memory_cap`（`cap = floor(MemAvailable × 0.75 / (W×H×B/px))`）
可能把帧级宽度压到远低于 lease —— 24.6 GB 机器上 4096² 帧在旧标定（358 B/px）下恒得
`cap = 2`，而 lease = 16。此时若帧内轴仍钉在 1，实际并行宽度只有 2/16，14 个核空转
（PERF-501 实测：cpu 恒 202%、p50 利用率 12.6%，G-RES-01 判据 ④⑤⑥ 全违约）。
**帧级被内存压低时，剩余预算必须转给帧内轴。**

**不变式**：帧级宽度未被内存压低时（`frame_workers == thread_budget`）本式退化为
`inner_omp = 1`，与历史行为逐位相同 ⇒ 该分配只改“预算怎么用”，不改任何节点的数值路径。
并行宽度与归约顺序无关（各节点归约顺序见下节确定性锚点）。

**观测面**：`ASTROCS_LEASE_TRACE=1` 给租约（`[lease] ... cap=`）、`ASTROCS_NODE_TRACE=1`
给节点执行窗口、`ASTROCS_P1CAP_TRACE=1` 给本分配快照（`[p1cap] ...`）。三者由
`eng/tools/monitoring/node_waterfall.py` 合成为节点级瀑布 + 逐节点并行宽度表。
标定值（`kP1FrameBytesPerPixel` 等）与实测依据见 `docs/architecture/PERFORMANCE_MODEL.md`。

## 确定性锚点（ARC-004）

- Phase2 UPM 权重归一：`lib/algorithms/coverage/src/upm.cpp:495` `compute_raw` — `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一（`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 固定顺序；确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Phase2 sampler：`lib/algorithms/coverage/src/sampler.cpp` **std::thread worker 池**（`:924-954`）——worker 数只来自 Runtime lease（`cfg.cpu_workers = budget.max_workers`，模块不取 `hardware_concurrency`）；`workers == 1` 走同一 `pass1_cell` 的串行 reference 分支（`init_shared` 复用 setup 句柄）。cell 由 `next_c.fetch_add(1)` 动态领取，结果写回 `cells[c*64+off]` 固定槽位 ⇒ 归约顺序与线程调度无关，1 worker 与 N worker 逐位一致。`P2_ENABLE_OPENMP`（`lib/algorithms/coverage/CMakeLists.txt:28` 默认 OFF）只保留 compile/link 接线，代码内无 OpenMP 并行区。**读路径（PERF-401）无进程级锁**：每 worker 自己的 `AioHipsDataset` 句柄（`:938` `rdr.init_own`），每次 tile 读各自 open→read→close，句柄线程私有、不跨线程转移（`critical(aio_read)` / `g_aio_mu` 已撤销，见 `docs/architecture/EXECUTION_MODEL.md` §2/§3）。确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Drizzle 浮点归约：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1662` `reduction(+:nSourcePixels,prof_geom_s,prof_wcs_s)`；`1751` tile 合并 `sumFlux/sumArea/sumVarNum/nContrib` 经 thread-local `TileAccumulator` 后串行合并（`t=1..num_threads` 固定顺序）；`1834`/`1843` `parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 与 `atomic` 计时累加 — 浮点累积顺序固定，reduction 顺序已文档化。

## 契约

ENG-THREAD-001..003（S2 注册）。
