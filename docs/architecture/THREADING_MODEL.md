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

**有效宽度的第三个因子（P1-PARALLEL-AXIS-REDESIGN-01 补）**：drizzle 投影内部的
scratch 池上限 `K`（`drizzle_engine.cpp` 的 `kScratchPoolCap`）会把帧内轴再截一次：

```
W_eff = in_flight × min(inner_omp, K)        // 有效宽度（PERFORMANCE_MODEL.md §5）
```

⇒ **要 `W_eff` 达到帧内轴宽度必须 `K ≥ inner_omp`**；而 `K = inner_omp = num_threads` 时
同时在飞的 scratch 份数 `= in_flight × inner_omp ≤ lease`（上式不变式）
⇒ **`K = num_threads` 是达成满宽的唯一最小取值，且总份数与轴形态无关**。
故 `kScratchPoolCap` 的取值不再是独立旋钮，而由帧内轴派生。

**「一帧独占全部核心」形态已被实测否决（P1-PARALLEL-AXIS-REDESIGN-01）**：
在 `lease=16`、8 帧 4096²/FP64、`W_eff` 恒为 16 的同二进制四档受控 A/B 下
（证据 `run/P1-PARALLEL-AXIS-REDESIGN-01/REPORT.md`；产品四档**逐位相同**）：

| 形态 | 墙钟 (s) | 峰值 RSS (GB) | CPU p50 |
|---|---|---|---|
| **8 帧 × 2 线程（现状）** | **390.7** | 14.12 | **1321%** |
| 8 帧 ×「单帧高并行令牌」（同时只有一帧持 9 线程，其余帧 1 线程） | 459.0 | 12.43 | 720% |
| 2 帧 × 8 线程 | 532.9 | 5.25 | 701% |
| **1 帧 × 16 线程（「一帧独占」）** | **583.7** | **3.57** | **98.9%** |

**帧在飞数越少：墙钟单调变差、CPU 占用单调变低、内存单调变省。** 根因是**每帧都有
不可并行的串行段**（FITS 读、`hips_write` 与 `drizzle_run` 帧内串行且占 drizzle 帧时 24–30%、
星表查询、`wcs-platesolve` 无帧级并行），帧级并发正是把这些串行段互相重叠的手段；
压到 1 帧会让其余核在串行段上空转（CPU p50 98.9% = 真的变成单核）。
⇒ **保留「帧轴优先摊开、剩余预算转帧内轴」的分配**；不得以「显然更优」为由改回一帧独占。

**观测面**：`ASTROCS_LEASE_TRACE=1` 给租约（`[lease] ... cap=`）、`ASTROCS_NODE_TRACE=1`
给节点执行窗口、`ASTROCS_P1CAP_TRACE=1` 给本分配快照（`[p1cap] ...`）。三者由
`eng/tools/monitoring/node_waterfall.py` 合成为节点级瀑布 + 逐节点并行宽度表。
标定值（`kP1FrameBytesPerPixel` 等）与实测依据见 `docs/architecture/PERFORMANCE_MODEL.md`。

**轴形态的受控 A/B 旋钮（P1-PARALLEL-AXIS-REDESIGN-01）**：
`ASTROCS_P1_AXIS_FRAME_WORKERS` / `ASTROCS_P1_AXIS_INNER_OMP`
（`module_adapters.cpp` 的 `p1_parallel_for`）与 `ASTROCS_P1_AXIS_SCRATCH_CAP`
（`drizzle_engine.cpp`）。缺省 `0` = 用策略值；**`frame_w × inner_omp > lease` 时一律
拒绝覆盖并打 `[p1axis] 拒绝越界标定…`** ⇒ 「总并行度 ≤ Runtime lease」不因标定而破
（AGENTS §6）。它们是为满足上文「并发度 A/B 对照的可复现性」第 1 条而存在的：
`taskset` 单靠 CPU 掩码无法在 `W_eff` 恒为 16 的前提下产生 `(8,2)/(2,8)/(1,16)` 三种形态。

### 并发度 A/B 对照的可复现性（P1-CONCURRENCY-CALIB-01 冻结口径）

`frame_workers` **不是配置项**：它由 `min(lease, p1_memory_cap)` 派生，而 `p1_memory_cap`
随**运行时刻的 `MemAvailable`** 浮动 ⇒ 同一份配置文件在不同时刻跑，生效并发度可能不同。
故任何「并发度 A vs B」的对照必须同时满足下列四条，缺一即不成立：

1. **钉住**：`taskset -c <掩码>` 钉 CPU 掩码（Runtime 线程预算 = 掩码内的 CPU 数 ⇒ `lease` 确定）；
   需要更高的 `p1_memory_cap` 时下调标定值 `kP1FrameBytesPerPixel`（它只是**闸门旋钮**，
   不进任何数值路径），但改标定值 = 改源码 ⇒ 必须**重新记录构建指纹**。
2. **判同一二进制**：以 `build_source_digest` 相等为前提（`docs/VERSIONING.md` §2.1）；
   `run_context.json.source_sha` 相等**不构成**前提。
3. **记生效值、且按 `in_flight` 判档**：从 `[p1cap] frame_workers … memory_cap=… n_units=…`
   取实际值，档位判据是 **`in_flight = min(n_units, frame_workers)`**，**不是** `frame_workers`。
   ⚠ 只比 `frame_workers` 会把「两档其实同 `in_flight`」误当成并发对照：PERF-501 的
   `w02_2f`/`w04_2f` 是 `frame_workers` 2 vs 4 而 `n_units=2` ⇒ 两档 `in_flight` **都是 2**，
   实际只差了 `inner_omp`（1 vs 2）。**帧轴**与**帧内轴**必须分开立论。
4. **负对照**：同并发、同二进制的重复运行必须逐字节 0 差异；否则该对照无判别力。
   扫描与逐字节比对口径见 `eng/tools/monitoring/concurrency_sweep.py`（`--self-test` 自证）。

**按上述四条执行的结果**（`run/P1-CONCURRENCY-CALIB-01/REPORT.md`）：同一二进制
（`build_source_digest=f86d60e2…`）、同一帧集 8 帧、只变 `in_flight`(2/4/8) 与
`inner_omp`(1/2) ⇒ **FITS 差异 0/5916、必同 JSON 差异 0**；组级星表聚合也逐字节相同
（仅 10 个含 `output_dir` 路径串的 JSON 在**路径掩码后**逐键相同）。
⇒ **帧级并发与帧内并发都不改变科学产品**；据此把 `kP1FrameBytesPerPixel` 由 358 重标定为
**116.0**（实测边际 99.68 B/px、base 0.143 GB），本机 `W_eff` 由 4 抬到 **16**，
同帧集墙钟 **783.5 s → 421.0 s（1.86×）**。
⚠ 残留风险：`p1_memory_cap` 是**瞬时快照**而非预留，无 swap 机器上目标配置峰值 **13.94 GB**
（= 0.75·A 的 89.5%），安全垫不厚。

## 确定性锚点（ARC-004）

- Phase2 UPM 权重归一：`lib/algorithms/coverage/src/upm.cpp:495` `compute_raw` — `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一（`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 固定顺序；确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Phase2 sampler：`lib/algorithms/coverage/src/sampler.cpp` **std::thread worker 池**（`:924-954`）——worker 数只来自 Runtime lease（`cfg.cpu_workers = budget.max_workers`，模块不取 `hardware_concurrency`）；`workers == 1` 走同一 `pass1_cell` 的串行 reference 分支（`init_shared` 复用 setup 句柄）。cell 由 `next_c.fetch_add(1)` 动态领取，结果写回 `cells[c*64+off]` 固定槽位 ⇒ 归约顺序与线程调度无关，1 worker 与 N worker 逐位一致。`P2_ENABLE_OPENMP`（`lib/algorithms/coverage/CMakeLists.txt:28` 默认 OFF）只保留 compile/link 接线，代码内无 OpenMP 并行区。**读路径（PERF-401）无进程级锁**：每 worker 自己的 `AioHipsDataset` 句柄（`:938` `rdr.init_own`），每次 tile 读各自 open→read→close，句柄线程私有、不跨线程转移（`critical(aio_read)` / `g_aio_mu` 已撤销，见 `docs/architecture/EXECUTION_MODEL.md` §2/§3）。确定性契约见 `docs/modules/phase2.md`（SCI-UPM-WEIGHT-001）。
- Drizzle 浮点归约：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1662` `reduction(+:nSourcePixels,prof_geom_s,prof_wcs_s)`；`1751` tile 合并 `sumFlux/sumArea/sumVarNum/nContrib` 经 thread-local `TileAccumulator` 后串行合并（`t=1..num_threads` 固定顺序）；`1834`/`1843` `parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 与 `atomic` 计时累加 — 浮点累积顺序固定，reduction 顺序已文档化。

## 契约

ENG-THREAD-001..003（S2 注册）。
