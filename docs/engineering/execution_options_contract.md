# 全局 worker 预算与执行架构合同（ExecutionOptions）

> 上游：`docs/ACSD_DESIGN.md` §8.1（唯一入口、阶段独立调度器）、§8.3（三个阶段调度器）、
> §9（CPU 后端与资源）
> 性能模型与标定常数正本：`docs/engineering/PERFORMANCE_MODEL.md`
> 异步 I/O 细则：`docs/engineering/ASYNC_IO_CONTRACT.md`

本文件是**唯一执行预算对象**的合同面，并承载并行轴分配、每阶段执行画像、异步与取消、
确定性锚点与并发正确性。并行 / I/O / 确定性 / 内存预算以此为唯一来源；嵌套模块只能从该预算借用，
线程池规模同源于该预算。异步队列必须有界，取消、错误传播与关闭顺序明确。

**阶段与命令的对应**：本文件表格中的 normalize / mosaic / export 三行对应设计 §8.1 的三个
独立调度器；代码中的 `phase1` / `phase2` / `phase3` 是同一三者的内部指代，两者一一对应。

## 1. 预算唯一来源

- worker 数**只**来自 benchmark 生成的机器 profile（安装目录）与全局预算对象，
  默认值一律取自 profile。合法来源 = 机器 profile + 预算对象
  （可用 CPU = 亲和性 ∩ cgroup ∩ Job Object 的交集，设计 §9）；
- 预算基量 `available_cpus = affinity ∩ cgroup ∩ Job Object`（不是机器总核数）；
- 预算按 `phase → stage → kernel` 层级显式分配，任何时刻 Σ(活动 worker) ≤ budget；
- `ExecutionOptions` **不是** worker 数的第二来源，只承载调用方从预算对象借到的值；
- backend 线程池与模块内 OpenMP 线程数一律经 host callback 注入运行时（由预算派生），
  取值与 `omp_set_num_threads` 无关；**全仓线程数由预算派生**；
- GPU 路由开关与第二个可执行入口不在合同面内（设计 §1.4）；配置 schema 与 CLI 不含
  `gpu_route`，也不提供兼容别名。

**分配策略**：串行 I/O 与控制面恒 1 线程；CPU 内核获得 `min(budget, kernel_block_hint)`；
异步 I/O pipeline 恒 1 专用线程；后台服务（watchdog / 资源监控 / progress 日志）
恒 1 线程 + 独立小预算（不入科学预算池）。

## 2. 预算对象定义

`lib/algorithms/coverage/include/astro/phase2/execution_options.h`：

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `cpu_workers` | int | **无默认**（由 benchmark profile / 全局预算对象注入） | CPU worker 数；`0` = auto ⇒ 由 profile 与预算对象决定，兜底面排除 `hardware_concurrency` |
| `io_workers` | int | **无默认**（同上） | IO worker 数；`0` = auto ⇒ 由 profile 与预算对象决定，兜底面排除 `cpu_workers/2` |
| `deterministic` | bool | true | 固定 seed / 顺序 / 归并 ⇒ 可复现结果 |
| `memory_budget_bytes` | uint64 | 0 | 内存预算字节；0 ⇒ 由 `memory_limit_mb` 决定 |

### 2.1 配置

`stage2.json` 顶层可选 `execution` 块（不含任何 GPU 路由键）：

```json
{
  "execution": {
    "cpu_workers": 4,
    "io_workers": 2,
    "deterministic": true,
    "memory_budget_bytes": 0
  }
}
```

约束：`cpu_workers` / `io_workers` 属于 [0,1024]（`0` = auto ⇒ 由 profile 与预算对象决定）。
违反即 `p2_stage2_parse_config` 返回 false（带错误信息）。约束的机器校验在
`lib/algorithms/coverage/src/stage2_common.cpp`；phase_config schema 不承载 `execution` 键
（硬件字段禁令，见 `eng/contracts/schemas/phase_config_mosaic.schema.json` 的 description）。

### 2.2 CLI 面

**唯一产品 CLI 入口 = `acsd`**（`normalize` / `mosaic` / `export` 三个子命令，设计 §7.1），
其命令面**不接受** worker 数或确定性旗标——预算由机器 profile 与全局预算对象给出，
不由命令行覆盖（命令树正本 = `lib/infrastructure/cli/command_tree.h`，
接口面 = `docs/engineering/CLI_PROTOCOL_V1.md`）。

`--cpu-workers` / `--io-workers` / `--deterministic` 属**工具面 `acsd-stage2` 的旗标**
（`lib/algorithms/coverage/tools/stage2.cpp`），供分阶段调试与基准使用，不属产品 CLI 面，
也不进入发布安装面。合同面不存在 `--gpu-route`。

## 3. 并行轴分配与轴不变式

normalize 各节点有两个可独立分配的并行轴：**帧级**（同时处理几帧，受内存闸门约束）与
**帧内 OpenMP**（每帧几条线程，受线程预算约束）。两轴按**乘积**分配：
**两轴之积 = 同时真正在算的线程数，必须 ≤ Runtime lease 给出的线程预算**（设计 §9），
实现落点 = `p1_parallel_for(workers, n, thread_budget, body)`
（`lib/infrastructure/scheduler/src/module_adapters.cpp`）。

```
in_flight = min(n, frame_workers)                 // frame_workers = min(__workers, p1_memory_cap)
inner_omp = max(1, thread_budget / in_flight)     // thread_budget = __workers（lease）
总并行度  = in_flight × inner_omp ≤ thread_budget
```

**为什么两轴都要动**：`p1_memory_cap`（`cap = floor(MemAvailable × 0.75 / (W×H×B/px))`，
`B` 的标定值见 `docs/engineering/PERFORMANCE_MODEL.md`）可能把帧级宽度压到远低于 lease
（大帧 + 保守 `B` 标定下 `cap` 常只有个位数，而 lease 可显著更高）。
此时若帧内轴仍钉在 1，CPU 预算大部分空转。**帧级被内存压低时，剩余预算必须转给帧内轴。**

**不变式**：帧级宽度未被内存压低时（`frame_workers == thread_budget`）本式退化为
`inner_omp = 1` ⇒ 该分配只改「预算怎么用」，不改任何节点的数值路径。
并行宽度与归约顺序无关（各节点归约顺序见 §6 确定性锚点）。

**有效宽度的第三个因子**：drizzle 投影内部的 scratch 池上限 `K`
（`drizzle_engine.cpp` 的 `kScratchPoolCap`）会把帧内轴再截一次：

```
W_eff = in_flight × min(inner_omp, K)        // 有效宽度（PERFORMANCE_MODEL.md §2）
```

⇒ **`W_eff` 达到帧内轴宽度必须 `K ≥ inner_omp`**；而 `K = inner_omp = num_threads` 时
同时在飞的 scratch 份数 `= in_flight × inner_omp ≤ lease`（上式不变式）
⇒ **`K = num_threads` 是达成满宽的唯一最小取值，且总份数与轴形态无关**。
故 `kScratchPoolCap` 由帧内轴派生（现行形态 = `drizzle_engine.cpp` 的 per-stripe scratch 池：
强制按 stripe 索引升序左折叠归约，见其左折叠归约注释与归约分支，同文件内）。

### 3.1 口径统一

- **唯一预算源**：本节两轴式与 §1 的 `Σ(活动 worker) ≤ budget` 是**同一预算的两种陈述**，
  不是两个预算：lease（可用 CPU 交 `ExecutionOptions` 的对象）是唯一来源，帧轴与帧内轴都从它派生；
- **单个科学内核内部的并行度 = 帧内轴**：`inner_omp` 就是该内核的并行度；内核内只有这一个
  parallel region（见 §5「嵌套并行」）。两轴相乘 = 同时真正在算的线程数，
  其积仍是**同一预算内的分配**，不是预算翻倍；
- **三命令进程边界**：`normalize` / `mosaic` / `export` 各自独立进程、一次调用只驱动一个阶段
  （设计 §1.2、`docs/engineering/CLI_PROTOCOL_V1.md` §1）⇒ 线程预算**不跨命令、不跨阶段共享**；
  「两轴相乘」只在同一次调用内的同一预算上成立。

**分配形态的结论：「一帧独占全部核心」不采纳。**
在 `W_eff` 恒定的同二进制受控 A/B 下（各档产品**逐位相同**；逐档墙钟 / 峰值 RSS / CPU p50 读数见
`实验/engineering-evidence/l2_performance/`），**帧在飞数越少：墙钟单调变差、CPU 占用单调变低、
内存单调变省**。根因是**每帧都有不可并行的串行段**（FITS 读、`hips_write` 与 `drizzle_run`
帧内串行、星表查询、`wcs-platesolve` 无帧级并行），帧级并发正是把这些串行段互相重叠的手段；
压到 1 帧会让其余核在串行段上空转而退化为单核。
⇒ **保留「帧轴优先摊开、剩余预算转帧内轴」的分配**；分配式的改动依据 = `实验/` 收益记录。

**观测面**：`ACSD_LEASE_TRACE=1` 给租约（`[lease] ... cap=`）、`ACSD_NODE_TRACE=1`
给节点执行窗口、`ACSD_P1CAP_TRACE=1` 给本分配快照（`[p1cap] ...`）。三者由
`eng/tools/monitoring/node_waterfall.py` 合成为节点级瀑布 + 逐节点并行宽度表。
标定常数与实测依据见 `docs/engineering/PERFORMANCE_MODEL.md`。

**轴形态的受控 A/B 旋钮**：`ACSD_P1_AXIS_FRAME_WORKERS` / `ACSD_P1_AXIS_INNER_OMP`
（`module_adapters.cpp` 的 `p1_parallel_for`）与 `ACSD_P1_AXIS_SCRATCH_CAP`
（`drizzle_engine.cpp`）。缺省 `0` = 用策略值；**`frame_w × inner_omp > lease` 时一律
拒绝覆盖并打 `[p1axis] 拒绝越界标定…`** ⇒ 「总并行度 ≤ Runtime lease」不因标定而破。
`taskset` 单靠 CPU 掩码无法在 `W_eff` 恒定的前提下产生多种轴形态，故须用这三个旋钮。

### 3.2 帧并发度上限的受控化

`frame_workers` **不是自由配置项**：它由 `min(lease, p1_memory_cap)` 派生，而 `p1_memory_cap`
随**运行时刻的 `MemAvailable`** 浮动 ⇒ 同一份配置文件在不同时刻跑，生效并发度可能不同。
编排参数受控化后，帧并发度**上限**可由受控配置键 `p1_max_frames_in_flight` 收紧
（唯一数值源 = `eng/packaging/config/runtime_resources.json` → 生成头 →
`module_adapters.cpp#p1_parallel_for`）；缺省 `0` = 不设上限，派生口径与本节不变，
且实现侧只收紧不放大（fail-closed：收紧后 `in_flight × inner_omp ≤ budget` 不变）。
依据设计 §8.3（帧并发度属编排参数，基于探针实测迭代）与
`docs/engineering/SCHEDULER_CONTRACT.md`（最终取值由性能门定，合同只保证机制正确）。
同批受控化的还有预取线程上限 `scheduler_prefetch_threads_max`。

**工作窃取策略**：生产不实现工作窃取；相关键位在实现前不进入配置面，
方案登记于 `config_registry.json#orchestration_params.gaps`，待实现后同批落键。

### 3.3 节点内并行度的解析面

节点线程预算的唯一解析面 = `lib/infrastructure/scheduler/src/cpu_budget.h#node_thread_budget`
（消费面 `module_adapters.cpp#node_thread_budget_of`，全部 `__workers` 读取点共用）：

1. 调度器 `execute` 注入的 lease（节点 config 的 `__workers`）**存在** ⇒ 以它为准，只收紧不放大
   （`1` 显式表示串行 reference；非数值 fail-closed 视为显式串行）；
2. lease **不存在**（直接调用 op 的非调度路径：门禁 / 单元 / 集成）⇒ 取「进程有效 CPU 预算」=
   进程注入的可用核（亲和性 ∩ cgroup），未注入则现算同一口径
   `min(可用核, 配置上限)`（配置键 `cpu_budget_max`，`0` = 不设上限）⇒ **缺省不是 1**。

理由：「快慢」不得建立在使用方是否传参上。缺省取 1 会使 P1 帧轴 `budget=1`，
`p1_parallel_for` 的 `frame_w<=1` 分支把帧内 ICV 置 1，节点内 `omp parallel for` 整批单线程
（PSF 域的对照读数见 `实验/engineering-evidence/l2_performance/`）。

机器门 = `CHK-NODE-BUDGET-WIRING`（静态：调度器内不得存在「缺省即串行」的 `__workers` 读取；
动态：探针链接**真实**解析面，断言无 lease 时预算 = 进程有效 CPU 预算、`inner_omp > 1`、
lease 存在时恒等且不被放大；`--self-test` 注入未接线状态必须判红）。

## 4. 每阶段执行画像

| 阶段 | 串行 I/O | CPU task（并行粒度） | async pipeline | backpressure |
|---|---|---|---|---|
| normalize 各节点 | aio 读（每帧一次，句柄线程私有） | 两轴：帧级 × 帧内 | — | 内存闸门限帧在飞数 |
| normalize Drizzle / HiPS | tile 原子写 | overlap / accumulate / 归并 | — | tile 写为原子事务 |
| mosaic coverage / UPM | UPM 模型读 / 写（串行） | coverage 重叠图 union；UPM solve | — | 同步链 |
| mosaic sampler | 控制采样点读（per-cell） | per-control-cell | — | 池满即阻塞 |
| mosaic block / reject / integrate | tile 原子写 | per-tile | — | 每任务只写自己的结果槽 |
| export | HiPS tile 读（cache） | 反向映射 + 采样 | — | 内存 ∝ 子块大小 |

各行的并行粒度与同步点：

- **normalize 各节点**（校准 / 检测 / PSF / 解算 / 测光 / 入库）：`p1_parallel_for` 两轴——
  帧级 `frame_w = min(lease, p1_memory_cap)` × 帧内 `inner_omp = max(1, lease / in_flight)`；
  不变式 `in_flight × inner_omp ≤ lease`；
- **normalize Drizzle / HiPS**：overlap / accumulate 按确定性 stripe 分片，
  归并按 stripe 索引升序左折叠；失败或取消的单元不落正式路径；
- **mosaic coverage / UPM**：coverage 重叠图 union 为 tile 级；UPM solve = IRLS 主迭代串行 +
  `compute_raw` / per-obs 权重 per-call 池（`granted_workers`）；
- **mosaic sampler**：per-control-cell，`next_c.fetch_add(1)` 动态认领，
  `cpu_workers = budget.max_workers`；
- **mosaic block / reject / integrate**：`p2_parallel_for`（std::thread + 原子计数动态认领，
  无 OpenMP、无 barrier），跨任务零浮点归约；
- **export**：子块流式 + 有界队列背压，内存占用 ∝ 子块大小、与总图大小无关。

每阶段在 run manifest 记录 `budget_alloc`（分配快照）；资源监控以同一对象为唯一事实来源
（见 `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md`）。

## 5. 异步与取消

- **async 仅两类**：I/O pipeline（读 / 写双缓冲）与后台服务；科学计算无 async / future
  （消除嵌套并行与不可预算并发）；
- **取消**：CLI JSONL cancel → 全局取消标志（原子）→ 各内核取消点
  （逐内核取消点见 `docs/science/algorithms/` 各算法文档）；取消后预算立即回收，取消单元不落盘；
- **嵌套并行**：外层已并行则内层串行（科学内核只在 parallel region 外开并行；
  I/O 线程与科学内核分属不同线程）；唯一豁免 = watchdog（独立预算）。

## 6. 确定性锚点与并发正确性

**浮点归约顺序冻结**——下列锚点全部有效：

- **mosaic UPM 权重归一**：`lib/algorithms/coverage/src/upm.cpp` 的 `compute_raw`——
  `raw_w = quality_factor * control_ivar` 冻结后按 control `sums[ck]` 归一
  （`raw_w[i]/sums[ck]*reliability`），遍历顺序为观测索引 `i` 的固定顺序；
  确定性契约见 `docs/detail/` 的 mosaic coverage 注册面（`SCI-UPM-WEIGHT-001`）。
- **mosaic sampler**：`lib/algorithms/coverage/src/sampler.cpp` 的 **std::thread worker 池**——
  worker 数只来自 Runtime lease（`cfg.cpu_workers = budget.max_workers`，
  模块不取 `hardware_concurrency`）；`workers == 1` 走同一 `pass1_cell` 的串行 reference 分支
  （`init_shared` 复用 setup 句柄）。cell 由 `next_c.fetch_add(1)` 动态领取，
  结果写回 `cells[idx]`（`idx = c·grid² + g`）固定槽位 ⇒ 归约顺序与线程调度无关，
  1 worker 与 N worker 逐位一致。`P2_ENABLE_OPENMP`
  （`lib/algorithms/coverage/CMakeLists.txt` 默认 OFF）只保留 compile / link 接线，
  代码内无 OpenMP 并行区。**读路径无进程级锁**：每 worker 自己的 `AioHipsDataset` 句柄
  （`rdr.init_own`），每次 tile 读各自 open → read → close，句柄线程私有、不跨线程转移
  （见 `docs/engineering/EXECUTION_MODEL.md` §2 / §3）。
- **drizzle 浮点归约**：`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` 主并行区
  `#pragma omp parallel num_threads(num_threads)`（per-stripe scratch 累积，无浮点 reduction 子句）；
  tile 合并 = **per-stripe scratch pool + 按 stripe 索引升序左折叠归约**（累加与归约解耦：
  `merge_cursor` 强制升序、归约分支，与主并行区的 per-stripe 左折叠完全同序）
  ⇒ 浮点结合树逐位一致，**与线程数 / 调度顺序无关**。
  `drizzle_engine.cpp` 的 prof 计数器合并区与
  `#pragma omp parallel reduction(+:n_quick,n_fully,n_dropin,n_sh)` 是整数计数器统计（非浮点归约），
  不作浮点归约锚。

**并发正确性**：

- 计数器：atomic 或 thread-local 聚合，计数一律经同步原语；无裸 data race；
- cache（UPM dense / Gaia / tile）必须线程安全或单线程互斥访问；
- 全局 OpenMP 设置只由宿主进程持有；线程数由 run context 配置；
- ACR 与浏览器层不接入生产：ACR 保留源码与隔离测试，不进生产构建 / 加载 / 路由 / benchmark /
  发布；HiPS Browser 属工具分类（非发布），不进产品 manifest（设计 §1.4、§8.4）。

## 7. 并发度对照的可复现性

任何「并发度 A vs B」的对照必须同时满足下列四条，缺一即不成立：

1. **钉住**：`taskset -c <掩码>` 钉 CPU 掩码（Runtime 线程预算 = 掩码内的 CPU 数 ⇒ `lease` 确定）；
   需要更高的 `p1_memory_cap` 时下调标定值 `kP1FrameBytesPerPixel`（它只是**闸门旋钮**，
   不进任何数值路径），但改标定值 = 改源码 ⇒ 必须**重新记录构建指纹**；
2. **判同一二进制**：以 `build_source_digest` 相等为前提（`docs/engineering/VERSIONING.md` §2.1）；
   `run_context.json.source_sha` 相等**不构成**前提；
3. **记生效值、且按 `in_flight` 判档**：从 `[p1cap] frame_workers … memory_cap=… n_units=…`
   取实际值，档位判据是 **`in_flight = min(n_units, frame_workers)`**，**不是** `frame_workers`。
   只比 `frame_workers` 会把「两档其实同 `in_flight`」误当成并发对照：当 `n_units` 小于
   `frame_workers` 时两档 `in_flight` 相同，实际只差 `inner_omp`。
   **帧轴**与**帧内轴**必须分开立论；
4. **负对照**：同并发、同二进制的重复运行必须逐字节 0 差异；否则该对照无判别力。
   扫描与逐字节比对口径见 `eng/tools/monitoring/concurrency_sweep.py`（`--self-test` 自证）。

**按上述四条执行的结果**：同一二进制（同一 `build_source_digest`）、同一帧集，
只变 `in_flight` 与 `inner_omp` ⇒ 全部 FITS 与必同 JSON **逐字节相同**
（仅含 `output_dir` 路径串的 JSON 在**路径掩码后**逐键相同），组级星表聚合也逐字节相同。
⇒ **帧级并发与帧内并发都不改变科学产品**（逐档读数见
`实验/engineering-evidence/l2_performance/`）。

**残留风险**：`p1_memory_cap` 是**瞬时快照**而非预留，无 swap 机器上目标配置的峰值 RSS
接近 `kP1FrameMemSafetyFrac · A` 的安全垫上限（余量读数见
`实验/engineering-evidence/l2_performance/`）。

## 8. 分层与使用约定

**执行分层**：

- **生产执行层（唯一）**：`scheduler` + `pipeline`（typed DAG 调度、统一线程预算、
  执行与取消；设计 §7.1 / §8）。一个进程只有一个全局执行顺序与线程预算源；
- 科学模块：内部并行 region 由宿主按预算注入；每模块文档化
  parallel / shared / thread-local / reduction / determinism / float accumulation order；
- 编排层由唯一 CLI 入口承担：按命令拉起对应阶段的调度器，一次调用只驱动一个阶段；
  唯一入口 = `acsd.exe`（Windows）/ `acsd`（Linux）（设计 §7.1 / §8.1）；
- ACR：`work_pool` + `device_executor` 调度，CPU reference 与 GPU 等价契约；不进生产；
- 浏览器：Qt 主线程 + 后台 I/O 线程；renderer 只读共享数据；属工具分类。

**使用约定**：

- 模块仅通过 `ExecutionOptions` 读取已分配预算；`effective_cpu_workers(exec)` /
  `effective_io_workers(exec)` 返回生效值；
- 嵌套模块复用该预算；`omp_set_num_threads(hc)` 属新建等规模线程池，越界即判红；
- 异步队列容量由 `memory_budget_bytes` 推导（见 `ASYNC_IO_CONTRACT.md`）；
- 实现面与设计条款的偏差登记于 `docs/KNOWN_LIMITATIONS.md`。

## 9. 静态 checker 合同

线程预算静态判据（载体见 门禁注册面（G08-10 重建））：

1. 扫描 `lib/` 生产源：`std::thread` / `std::async` / `_beginthread` / `CreateThread` 出现处
   必须在 `THREAD_BUDGET_EXEMPT` 登记表内。登记表以 checker 内 `THREAD_BUDGET_EXEMPT` 为
   **唯一事实源**，随实现演进，以实跑输出为准；`eng/tests/` 为扫描面豁免、不占登记条目。
   生产科学模块面的核心条目：`lib/algorithms/coverage/src/upm.cpp` 的 per-call 池
   （`cworkers = Runtime lease`）、`lib/algorithms/coverage/src/sampler.cpp` 的 per-call 池
   （`workers = Runtime lease`）；另有
   `lib/algorithms/integration/phase2_integrate/oracle/weight_chain_selfcheck.cpp`
   （权重链独立 Oracle 自查池）、`lib/algorithms/cosmetic/src/module_entry.cpp`
   （`omp_set_num_threads` 租约注入），以及 `lib/infrastructure/pipeline/orchestrator/`
   的 watchdog 与资源监控文件级豁免。逐条清单以实跑输出为准；
2. `omp_set_num_threads(` / `num_threads(` 字面量零容忍；
3. 未登记即 FAIL（exit 1）——保证「未登记线程创建」机器可查。

## 10. 测试

`lib/algorithms/coverage/tests/execution_options_test.cpp`（目标 `phase2_execution_options`）：
配置覆盖、缺省由 profile / 预算注入、非法值拒绝、effective 计数器。

## 11. 关联

- `docs/engineering/PERFORMANCE_MODEL.md`：性能模型、标定常数与内存闸门口径；
- `docs/engineering/EXECUTION_MODEL.md`：执行合同面与句柄线程私有纪律；
- `docs/engineering/ASYNC_IO_CONTRACT.md`：异步 I/O 细则与有界队列；
- `docs/engineering/SCHEDULER_CONTRACT.md`：调度机制正确性；
- `docs/engineering/OWNERSHIP_AND_LIFETIME.md`：生命周期与所有权；
- `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md`：资源监控面；
- `docs/TRACEABILITY.csv`：线程模型契约 `ENG-THREAD-001` 的登记面。
