# Threading Model

> 上游：`docs/ASTROCS_DESIGN.md` §8（软件架构）、§9（CPU 后端与资源）

## 分层

- **生产执行层（唯一）**：`scheduler` + `pipeline`（typed DAG 调度、统一线程预算、
  执行与取消；最高设计 §7.1/§8）。一个进程只有一个全局执行顺序与线程预算源。
- 科学模块：内部并行 region 由宿主按预算注入；每模块文档化 parallel/shared/
  thread-local/reduction/determinism/float accumulation order。
- 编排层由唯一 CLI 入口承担：按命令拉起对应阶段的调度器，一次调用只驱动一个阶段；唯一入口 = `acsd.exe`（Windows）/ `acsd`（Linux）（最高设计 §7.1/§8.1）。
- ACR：work_pool + device_executor 调度；CPU reference 与 GPU 等价契约。
  **DORMANT**：保留源码与隔离测试，**不进生产构建/加载/路由/benchmark/发布**
  （最高设计 §1.4）。
- 浏览器：Qt 主线程 + 后台 I/O 线程；renderer 只读共享数据。
  **工具分类（非发布）**：HiPS Browser 是可视化组件，**不进产品 manifest**
  （最高设计 §1.4 非目标、§8.4 顶层结构）。

## 约定

- 全局 OpenMP 设置只由宿主进程持有；线程数由 run context 配置。
- 计数器：atomic 或 thread-local 聚合（计数一律经同步原语）。
- 浮点累积顺序固定（确定性输出）；归约顺序文档化于本文件「确定性锚点」节。
- cache（dense UPM、Gaia 查询缓存）必须线程安全或单线程互斥访问。

### 并行轴分配（冻结口径）

P1 各节点有两个可独立分配的并行轴：**帧级**（同时处理几帧，受内存闸门约束）与
**帧内 OpenMP**（每帧几条线程，受线程预算约束）。两轴按**乘积**分配：**两轴之积 = 同时真正
在算的线程数，必须 ≤ Runtime lease 给出的线程预算**（最高设计 §9），实现落点 = `p1_parallel_for(workers, n, thread_budget, body)`
（`lib/infrastructure/scheduler/src/module_adapters.cpp`）。

```
in_flight = min(n, frame_workers)                 // frame_workers = min(__workers, p1_memory_cap)
inner_omp = max(1, thread_budget / in_flight)     // thread_budget = __workers（lease）
总并行度  = in_flight × inner_omp ≤ thread_budget
```

**为什么两轴都要动**：`p1_memory_cap`（`cap = floor(MemAvailable × 0.75 / (W×H×B/px))`）
可能把帧级宽度压到远低于 lease（大帧 + 保守 `B` 标定下 `cap` 常只有个位数，而 lease 可到 16）。
此时若帧内轴仍钉在 1，CPU 预算大部分空转（受控读数见
`实验/engineering-evidence/l2_performance/`，G-RES-01 判据 ④⑤⑥ 即针对该形态）。
**帧级被内存压低时，剩余预算必须转给帧内轴。**

**不变式**：帧级宽度未被内存压低时（`frame_workers == thread_budget`）本式退化为
`inner_omp = 1` ⇒ 该分配只改「预算怎么用」，不改任何节点的数值路径。
并行宽度与归约顺序无关（各节点归约顺序见下节确定性锚点）。

**有效宽度的第三个因子**：drizzle 投影内部的
scratch 池上限 `K`（`drizzle_engine.cpp` 的 `kScratchPoolCap`）会把帧内轴再截一次：

```
W_eff = in_flight × min(inner_omp, K)        // 有效宽度（PERFORMANCE_MODEL.md §2）
```

⇒ **要 `W_eff` 达到帧内轴宽度必须 `K ≥ inner_omp`**；而 `K = inner_omp = num_threads` 时
同时在飞的 scratch 份数 `= in_flight × inner_omp ≤ lease`（上式不变式）
⇒ **`K = num_threads` 是达成满宽的唯一最小取值，且总份数与轴形态无关**。
故 `kScratchPoolCap` 由帧内轴派生（现行形态 = `drizzle_engine.cpp` 的 per-stripe scratch 池：强制按 stripe 索引升序左折叠归约，见 `:1857-1859` 与归约分支 `:2117-2142`）。

**口径统一（R-30：两轴并行 vs 内核不嵌套 · 唯一预算源 · 三命令进程边界）**

- **唯一预算源**：上文两轴式与 `THREAD_BUDGET_ARCH.md` §1 的 `Σ(活动 worker) ≤ budget` 是
  **同一预算的两种陈述**，不是两个预算：lease（= 可用 CPU 交 `execution_options_contract.md`
  的对象）是唯一来源，帧轴与帧内轴都从它派生。
- **单个科学内核内部的并行度 = 帧内轴**：`inner_omp` 就是该内核的并行度；内核内只有这一个
  parallel region（`EXECUTION_MODEL.md` §6「外层已并行则内层串行」、
  `THREAD_BUDGET_ARCH.md` §1「消除嵌套并行与不可预算并发」）。两轴相乘 = 同时真正在算的线程数，
  其积仍是**同一预算内的分配**，不是预算翻倍。
- **三命令进程边界**：`normalize`/`mosaic`/`export` 各自独立进程、一次调用只驱动一个阶段
  （`docs/api/CLI_PROTOCOL_V1.md` §1「三个命令平级独立：各自独立进程…」、
  `docs/ASTROCS_DESIGN.md` §1.2）⇒ 线程预算**不跨命令、不跨阶段共享**；
  「两轴相乘」只在同一次调用内的同一预算上成立。

**分配形态的结论：「一帧独占全部核心」不采纳。**
在 `W_eff` 恒定的同二进制受控 A/B 下（四档产品**逐位相同**；逐档墙钟 / 峰值 RSS / CPU p50 读数见
`实验/engineering-evidence/l2_performance/`），**帧在飞数越少：墙钟单调变差、CPU 占用单调变低、
内存单调变省**。根因是**每帧都有不可并行的串行段**（FITS 读、`hips_write` 与 `drizzle_run`
帧内串行、星表查询、`wcs-platesolve` 无帧级并行），帧级并发正是把这些串行段互相重叠的手段；
压到 1 帧会让其余核在串行段上空转而退化为单核。
⇒ **保留「帧轴优先摊开、剩余预算转帧内轴」的分配**；分配式的改动依据 = `实验/` 收益记录。

**观测面**：`ASTROCS_LEASE_TRACE=1` 给租约（`[lease] ... cap=`）、`ASTROCS_NODE_TRACE=1`
给节点执行窗口、`ASTROCS_P1CAP_TRACE=1` 给本分配快照（`[p1cap] ...`）。三者由
`eng/tools/monitoring/node_waterfall.py` 合成为节点级瀑布 + 逐节点并行宽度表。
标定值（`kP1FrameBytesPerPixel` 等）与实测依据见 `docs/architecture/PERFORMANCE_MODEL.md`。

**轴形态的受控 A/B 旋钮**：
`ASTROCS_P1_AXIS_FRAME_WORKERS` / `ASTROCS_P1_AXIS_INNER_OMP`
（`module_adapters.cpp` 的 `p1_parallel_for`）与 `ASTROCS_P1_AXIS_SCRATCH_CAP`
（`drizzle_engine.cpp`）。缺省 `0` = 用策略值；**`frame_w × inner_omp > lease` 时一律
拒绝覆盖并打 `[p1axis] 拒绝越界标定…`** ⇒ 「总并行度 ≤ Runtime lease」不因标定而破
（AGENTS §6）。它们是为满足上文「并发度 A/B 对照的可复现性」第 1 条而存在的：
`taskset` 单靠 CPU 掩码无法在 `W_eff` 恒为 16 的前提下产生 `(8,2)/(2,8)/(1,16)` 三种形态。

### 并发度 A/B 对照的可复现性（冻结口径）

`frame_workers` **不是自由配置项**：它由 `min(lease, p1_memory_cap)` 派生，而 `p1_memory_cap`
随**运行时刻的 `MemAvailable`** 浮动 ⇒ 同一份配置文件在不同时刻跑，生效并发度可能不同。
（DYN-740 / R-27 补充：编排参数受控化后，帧并发度**上限**可由受控配置键
`p1_max_frames_in_flight` 收紧（唯一数值源 = `eng/packaging/config/runtime_resources.json`
→ 生成头 → `module_adapters.cpp#p1_parallel_for`）；缺省 `0` = 不设上限，派生口径与本节不变，
且实现侧只收紧不放大（fail-closed：收紧后 `in_flight × inner_u ≤ budget` 不变）。
依据 `docs/ASTROCS_DESIGN.md` §8.3:652（帧并发度属编排参数，基于探针实测迭代）与
`docs/contracts/SCHEDULER_CONTRACT.md`:38（最终取值由性能门定，合同只保证机制正确）。
同批受控化的还有未接线调度器的预取线程上限 `scheduler_prefetch_threads_max`。
**工作窃取策略**：生产**无实现**（全仓唯一命中在 ACR，而 ACR 生产不可达）⇒ 键位方案已登记于
`config_registry.json#orchestration_params.gaps`，不落无读取面的死键，待实现后同批落键。）
故任何「并发度 A vs B」的对照必须同时满足下列四条，缺一即不成立：

1. **钉住**：`taskset -c <掩码>` 钉 CPU 掩码（Runtime 线程预算 = 掩码内的 CPU 数 ⇒ `lease` 确定）；
   需要更高的 `p1_memory_cap` 时下调标定值 `kP1FrameBytesPerPixel`（它只是**闸门旋钮**，
   不进任何数值路径），但改标定值 = 改源码 ⇒ 必须**重新记录构建指纹**。
2. **判同一二进制**：以 `build_source_digest` 相等为前提（`docs/VERSIONING.md` §2.1）；
   `run_context.json.source_sha` 相等**不构成**前提。
3. **记生效值、且按 `in_flight` 判档**：从 `[p1cap] frame_workers … memory_cap=… n_units=…`
   取实际值，档位判据是 **`in_flight = min(n_units, frame_workers)`**，**不是** `frame_workers`。
   ⚠ 只比 `frame_workers` 会把「两档其实同 `in_flight`」误当成并发对照：当 `n_units` 小于
   `frame_workers` 时两档 `in_flight` 相同，实际只差 `inner_omp`。
   **帧轴**与**帧内轴**必须分开立论。
4. **负对照**：同并发、同二进制的重复运行必须逐字节 0 差异；否则该对照无判别力。
   扫描与逐字节比对口径见 `eng/tools/monitoring/concurrency_sweep.py`（`--self-test` 自证）。

**按上述四条执行的结果**：同一二进制（同一 `build_source_digest`）、同一帧集，
只变 `in_flight` 与 `inner_omp` ⇒ 全部 FITS 与必同 JSON **逐字节相同**
（仅含 `output_dir` 路径串的 JSON 在**路径掩码后**逐键相同），组级星表聚合也逐字节相同。
⇒ **帧级并发与帧内并发都不改变科学产品**（逐档读数见
`实验/engineering-evidence/l2_performance/`）。
现行标定 `kP1FrameBytesPerPixel` = **116.0**、`kP1FrameMemSafetyFrac` = **0.75**（冻结）。
⚠ 残留风险：`p1_memory_cap` 是**瞬时快照**而非预留，无 swap 机器上目标配置的峰值 RSS
接近 `kP1FrameMemSafetyFrac · A` 的安全垫上限（余量读数见
`实验/engineering-evidence/l2_performance/`）。

## 确定性锚点

- Phase2 UPM 权重归一：`lib/algorithms/coverage/src/upm.cpp:605` `compute_raw` — `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一（`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 固定顺序；确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Phase2 sampler：`lib/algorithms/coverage/src/sampler.cpp` **std::thread worker 池**（`:924-954`）——worker 数只来自 Runtime lease（`cfg.cpu_workers = budget.max_workers`，模块不取 `hardware_concurrency`）；`workers == 1` 走同一 `pass1_cell` 的串行 reference 分支（`init_shared` 复用 setup 句柄）。cell 由 `next_c.fetch_add(1)` 动态领取，结果写回 `cells[idx]`（`idx = c·grid² + g`，`sampler.cpp:744-745`）固定槽位 ⇒ 归约顺序与线程调度无关，1 worker 与 N worker 逐位一致。`P2_ENABLE_OPENMP`（`lib/algorithms/coverage/CMakeLists.txt:28` 默认 OFF）只保留 compile/link 接线，代码内无 OpenMP 并行区。**读路径无进程级锁**：每 worker 自己的 `AioHipsDataset` 句柄（`:938` `rdr.init_own`），每次 tile 读各自 open→read→close，句柄线程私有、不跨线程转移（见 `docs/architecture/EXECUTION_MODEL.md` §2/§3）。确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Drizzle 浮点归约：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1923` 主并行区 `#pragma omp parallel num_threads(num_threads)`（per-stripe scratch 累积，无浮点 reduction 子句）；tile 合并 = **per-stripe scratch pool + 按 stripe 索引升序左折叠归约**（累加与归约解耦：`:1857-1859` 注释「合并仍由 merge_cursor 强制按 stripe 索引升序左折叠」、`:1889-1891` `pendingStripe`/`merge_cursor`、归约分支 `:2117-2142`；与 drizzle 主并行区的 per-stripe 左折叠完全同序 ⇒ 浮点结合树逐位一致，**与线程数/调度顺序无关**）。注：`:2270` 现为 prof 计数器合并（非浮点 tile 合并锚）；`:2279` `#pragma omp parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 为整数计数器统计（非浮点归约）与 `atomic` 计时累加 — 浮点累积顺序固定，归约顺序已文档化。

## 契约

线程模型契约 = `ENG-THREAD-001`（登记面 = `docs/TRACEABILITY.csv`）；静态 checker 合同见 `THREAD_BUDGET_ARCH.md` §5。
