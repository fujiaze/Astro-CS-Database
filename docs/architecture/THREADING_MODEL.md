# Threading Model

> 上游：ASTROCS_DESIGN.md §8（软件架构）

## 分层

- **生产执行层（唯一）**：`scheduler` + `pipeline`（typed DAG 调度、统一线程预算、
  执行与取消；最高设计 §7.1/§8）。一个进程只有一个全局执行顺序与线程预算源。
- 科学模块：内部并行 region 由宿主按预算注入；每模块文档化 parallel/shared/
  thread-local/reduction/determinism/float accumulation order。
- 编排层由 CLI 的 pipeline driver 承担；唯一入口 = `ACSD Cli` / `acsd_cli`（最高设计 §7.1）。
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

## 确定性锚点（ARC-004）

- Phase2 UPM 权重归一：`lib/algorithms/coverage/src/upm.cpp:495` `compute_raw` — `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一（`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 固定顺序；确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Phase2 sampler：`lib/algorithms/coverage/src/sampler.cpp` **std::thread worker 池**（`:924-954`）——worker 数只来自 Runtime lease（`cfg.cpu_workers = budget.max_workers`，模块不取 `hardware_concurrency`）；`workers == 1` 走同一 `pass1_cell` 的串行 reference 分支（`init_shared` 复用 setup 句柄）。cell 由 `next_c.fetch_add(1)` 动态领取，结果写回 `cells[c*64+off]` 固定槽位 ⇒ 归约顺序与线程调度无关，1 worker 与 N worker 逐位一致。`P2_ENABLE_OPENMP`（`lib/algorithms/coverage/CMakeLists.txt:28` 默认 OFF）只保留 compile/link 接线，代码内无 OpenMP 并行区。**读路径（PERF-401）无进程级锁**：每 worker 自己的 `AioHipsDataset` 句柄（`:938` `rdr.init_own`），每次 tile 读各自 open→read→close，句柄线程私有、不跨线程转移（`critical(aio_read)` / `g_aio_mu` 已撤销，见 `docs/architecture/EXECUTION_MODEL.md` §2/§3）。确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Drizzle 浮点归约：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1662` `reduction(+:nSourcePixels,prof_geom_s,prof_wcs_s)`；`1751` tile 合并 `sumFlux/sumArea/sumVarNum/nContrib` 经 thread-local `TileAccumulator` 后串行合并（`t=1..num_threads` 固定顺序）；`1834`/`1843` `parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 与 `atomic` 计时累加 — 浮点累积顺序固定，reduction 顺序已文档化。

## 契约

ENG-THREAD-001..003（S2 注册）。
