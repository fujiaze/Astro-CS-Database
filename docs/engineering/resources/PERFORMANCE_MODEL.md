# 性能模型与资源判据

上游：最高设计的软件架构、CPU 后端与资源两章[6]；并行轴分配见 `../architecture/DATA_FLOW.md`。

结构原则、编排参数标定、有效并行宽度模型、冻结标定常数与合成性能判据的正本。
每个科学浮点量的量纲与标度口径见 `../standards/NUMERIC.md`[2]；受影响构建目标的反查依据见
`../build/BUILD_GRAPH.md`[3]。
实测读数与逐档墙钟落实验域的工程证据面，本分册只承载结构结论与冻结参数。

## 结构原则与来源

- 热路径内每像素零 alloc/log/fs/clock；per-pixel 数学用连续 buffer。
- 科学精度优先：FP64 reference；FP32 仅显式精度等价路径。
- 关键 fast path 与各自 reference（逐条给定义与验证方式）：
  - **Drizzle candidate conservative test**：候选判定取保守上界（宁可多算、不可漏算）；
    验证方式 = 与穷举候选集比对时 **false negative 恒为 0**（本仓 oracle）。
  - **UPM dense cache**：稠密缓存与逐点稀疏求值必须给出同一结果；
    验证方式 = 缓存命中域上的最大相对偏差不超过第 4 节冻结的双精度非归约档容差，
    适用量级域 = 缓存命中域上的缓存值量级 `scale`，超出该域按同值的相对形式判[4]。
    定义见 `../../science/sky/UPM.md`；容差数值不在本篇复述。
  - **NoiseWeightModelV1**：权重模型以 oracle 矩阵与 Monte Carlo 双向核对；
    定义见 `../../science/noise_snr/NOISE_SNR.md`[5]。
  - **Gaia 极区 prune**：剪枝用**可证明保守**的球面判据（不丢候选）；cache 键精确匹配，
    实现锚 = `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` 的 `query_cache_lookup`。

- 编排与内存的实测读数、拟合与逐档墙钟见 `实验/engineering-evidence/l2_performance/`；
  本文件只承载结构结论与冻结参数。

## 编排参数标定（探针驱动）

> 上游条款：编排参数的**最终取值**由探针基于实测数据确定，合同只保证机制正确与探针齐全；
> `../../ACSD_DESIGN.md` 的 CPU 后端与资源一章（探针驱动优化）。
> 并行轴语义（含帧内 OpenMP 度公式与轴不变式）唯一正本 = `../architecture/DATA_FLOW.md`[1]；
> 本篇只给编排参数的结构结论与冻结参数，不复制轴公式。

### 结构性根因

帧级并发受**内存闸门**限制：Runtime lease 给出的 CPU 预算在 4096² 级帧上不能全量转成帧级并发；
实际帧级宽度由内存预算与单帧驻留字节数决定。后果是**预算剩余**：CPU 用量远低于 lease，
剩余预算必须转给**帧内轴**才能被利用。

帧内尚存的**串行段**：`hips_write` 在帧内与 `drizzle_run` 串行；`wcs-platesolve` 无帧级并行。
逐节点瀑布、CPU 曲线与串行段占比读数见 `实验/engineering-evidence/l2_performance/`。

### 冻结参数

冻结参数的两个数值（每像素字节、安全系数）与内存预算百分比的**唯一数值源** =
`eng/packaging/config/runtime_resources.json`；本篇只登记参数名、落点与取值口径，不声明第二套数值。

| 参数 | 数值键 | 落点 | 依据 |
|---|---|---|---|
| 帧内 OpenMP 度 | `../architecture/DATA_FLOW.md` 的并行轴分配[1] | `p1_parallel_for`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 帧级被内存压低时把剩余预算转给帧内轴；帧级未压低时退化为 1 |
| `kP1FrameBytesPerPixel`（`B`） | `frame_memory_gate.bytes_per_pixel` | `p1_memory_cap`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 闸门系数的标定口径与实测拟合见内存模型形态 |
| `kP1FrameMemSafetyFrac` | `frame_memory_gate.safety_frac` | 同上 | 留基础占用与运行波动；取值随帧几何与精度模式重标定 |
| 内存预算百分比 | `memory_budget_percent` | `eng/packaging/config/runtime_resources.json` | 单一来源；准入预算 = `MemAvailable` 按该比例的派生值 |

- **实现侧字面量的现状**：`module_adapters.cpp` 当前仍以 `static constexpr double` 持有这两个字面量，
  未消费 CMake `configure_file` 生成的配置头。该登记与实现侧现状不一致，属代码侧待订正项；
  在订正前，**唯一数值源是上表所列 JSON 键**，实现侧字面量只作镜像，不得反向覆盖。

### 逐位一致性（帧内轴不进数值路径）

帧内 OpenMP 轴对全部科学产物**逐位中性**：同帧并发、lease 恒定、仅改帧内轴宽度的 A/B 对照下，
全部 `.fits` 与目录/星表类 JSON（`p1_sources` / `p1_flux` / `p1_phot` / `p1_snr` /
`p1_psf` / `p1_products`）逐字节相同；遥测与路径类工件
（`alloc_*` / `resource_*` / `worker_balance` / `graph/*` / `p1_final` / HiPS `properties`）
仅差 output_dir 路径串 / 时间戳 / run_id。
该性质与 `../../science/algorithms/DRIZZLE_GEOMETRY.md` 的 `DRIZZLE-DET-001`
（1..16 线程预算逐位恒等）同口径；对照实验与逐档读数见 `实验/engineering-evidence/l2_performance/`。

## 有效并行宽度

**符号**：`A` = 运行时 `MemAvailable`；`P` = 单帧像素数；`B` = `kP1FrameBytesPerPixel`；
`L` = lease（`granted_workers`）；`K` = scratch 池上限（`drizzle_engine.cpp` 的
`kScratchPoolCap`）；`F` = 帧级并发上限；`n` = 本次运行的输入帧数；
`in_flight` = 帧级实际在飞数；`I` = 帧内 OpenMP 度；`W_eff` = 同时真正在算的线程数。

```
F = min(L, floor(kP1FrameMemSafetyFrac·A / (P·B)))   // 帧级内存闸门上限
in_flight = min(n, F)                                // 帧级实际在飞数
I = max(1, L / in_flight)                            // 帧内轴（正本 = ../architecture/DATA_FLOW.md 的并行轴分配）
W_eff = in_flight × min(I, K)                        // drizzle 节点；K 为每帧 scratch 槽上限
```

上式的逐字来源 = `../architecture/DATA_FLOW.md` 的并行轴分配：`in_flight = min(n, frame_workers)`
（`frame_workers = min(lease, 内存闸门上限)`）、`inner_omp = max(1, thread_budget / in_flight)`
（`thread_budget = lease`）、总并行度 `in_flight × inner_omp ≤ thread_budget`[1]；
实现侧同式见 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p1_parallel_for`。

**内存模型形态**：单帧驻留 = `base` + `边际字节/像素` × `in_flight`。标定口径与实测拟合如下。

| 项 | 值 | 性质 |
|---|---|---|
| `base` | 0.143 GB | 实测（拟合截距） |
| 边际项 | 1.6724 GB/帧 = 99.68 B/px（`P = 4096² = 16.777e6 px`） | 实测（拟合斜率） |
| 闸门系数 `B` | 取 `frame_memory_gate.bytes_per_pixel` | 标定值，取可行窗口上端 |
| 拟合残差 | ≤ ±2.5%（F = 1/2/4/8 四档） | 实测 |

- **件名**：`run/P1-CONCURRENCY-CALIB-01/`（报告的实测内存模型与可采纳值一节，
  逐档峰值 RSS 见同目录 `summary_F1.json`、`summary_F2a.json`、`summary_F4.json`、`summary_F8.json`）；
  适用域 = 同场 8 帧 4096² FP64、drizzle auto nside、`kScratchPoolCap = 2`、无 swap。
- **拟合口径**：同一二进制、同场 8 帧、`inner_omp = 1`，对峰值 RSS 与 `in_flight` 做最小二乘，
  得 `RSS(F) = 0.143 GB + F × 1.6724 GB`；闸门系数**不取实测边际**（99.68 B/px），
  而取「使闸门放行目标 `F` 且需求 RSS 不越 `kP1FrameMemSafetyFrac·A`」的**可行窗口上端** ——
  该窗口为 `(0.75·A/(9P), 0.75·A/(8P)]`，随 `A` 平移，故 `B` 是**与机器 `A` 绑定**的标定值，
  换机器必须按同一口径重标定。
- 本机实测的逐档峰值 RSS、墙钟与指纹见 `实验/engineering-evidence/l2_performance/`。

**结构性结论（与具体读数无关）**：

1. **整数除法死区**：在 `in_flight = F`（前提 `F ≤ n`）下，`I = max(1, L / F)` 是整数除法，
   `F` 一越过 `L/2` 就掉到 1 ⇒ `W_eff` 在 `F ∈ (L/2, L)` 形成**死区**（达不到满宽 `L`）；
   满宽只在 `F = L/2`（`I = 2`）或 `F ≥ L`（`I = 1`）取到。故「`B` 越小越好」不成立，
   标定必须实测、不能外推。`F > n` 时 `in_flight = n`，`I = max(1, L / n)` 与 `F` 无关，
   `W_eff = n × min(I, K)` 随 `F` 增大**不再下降** —— 死区只在 `F ≤ n` 段成立。
2. **每帧存在不可并行的串行段**（FITS 读、`hips_write`、星表查询、`wcs-platesolve`），
   见 `../architecture/DATA_FLOW.md`；该串行段是「帧级并发是手段」的**结构性**理由，
   并有本仓受控四档墙钟对照支撑（`run/P1-PARALLEL-AXIS-REDESIGN-01/REPORT.md`：
   帧在飞数 8 → 2 → 1 时墙钟 390.7 → 532.9 → 583.7 s，CPU p50 1321% → 701% → 98.9%，
   相邻档区间不重叠）。
3. **内核效率的 stripe 收益递减拐点是待标定的经验拐点，不是已标定的物理常数**：把「帧内 stripe 数超过
   某阈值后进入收益递减区」写成阈值，需要一条固定其它变量、只变 stripe 数的墙钟扫描读数；
   本仓现有工件中无该扫描（`实验/engineering-evidence/l2_performance/` 与
   `run/P1-PARALLEL-AXIS-REDESIGN-01/` 扫的是帧在飞数 × 帧内线程数，不是 stripe 数），
   故本篇**不给该拐点数值**，也不把它列为不可改项。该拐点标定后结论落实验域，本篇只回引。
4. **内存闸门是准入边界**：若某档的 `F` 使需求 RSS 超过 `kP1FrameMemSafetyFrac·A`，
   该档不采纳 —— 安全边界优先于并行宽度。
5. **`hips_write` 串行**是 drizzle 帧时的固定占比项，不随 `F` 缩小。

**分类结论**：

| 类别 | 条目 |
|---|---|
| **当前实现的结构限制（可改）** | ① `K` 是**策略值**不是物理值（派生规则见 scratch 池上限的派生一节）；② `B` 与 `kP1FrameMemSafetyFrac` 随帧几何与精度模式重标定；③ 帧级并发受内存闸门限制；④ `hips_write` 在帧内与 `drizzle_run` **串行**；⑤ `wcs-platesolve` 无帧级并行；⑥ `inner_omp = max(1, L/in_flight)` 的整数除法使 `W_eff` 在 `in_flight ∈ (L/2, L)` 非单调（死区，见结构结论 1）；⑦ 内核效率的 stripe 收益递减拐点**待标定**（见结构结论 3） |
| **物理/算法限制（调度参数不可改）** | ① 每帧**不可压缩的输出本体**：canonical 累加器容量不随 `K` 变；② 每帧**不可并行串行段**（FITS 读、`hips_write`、星表查询、`wcs-platesolve`）的墙钟下界；③ 逻辑核 / 物理核 / 物理内存上限共同决定 `W_eff` 的取值上界 |

**结论**：在冻结的 `B` 与 `kP1FrameMemSafetyFrac` 下，帧级与帧内两轴可同时取到满宽，
且全部科学产品**逐字节不变**（逐位中性判据见上）。目标 `W_eff` 档位的均值利用率是否可达，
须在该档位实测判定（读数见 `实验/engineering-evidence/l2_performance/`）。
剩余可达路径 = 让 `hips_write` 与 `drizzle_run` 重叠 + 改善内核在高 stripe 数时的效率；
后两项属算法开发，不属编排参数标定。

## scratch 池上限的派生（`K = num_threads`）

**充分条件（非定理陈述）**：在 drizzle 由帧级轴驱动时，`K = num_threads` 是达成满宽的一个**充分条件**，
且**不是唯一取值**；其成立依赖四条可核事实：

- **满宽的充要条件**：抽象口径 `W_eff = in_flight × min(inner_omp, K)` 对 `K` 单调不降，
  故 `W_eff` 达到帧内轴宽度 `in_flight × inner_omp` 当且仅当 `K ≥ inner_omp`；`K < inner_omp` 时多余线程在池上空等。
  实现实际分配的池是 `kScratchPool = min(num_threads, kScratchPoolCap)`
  （`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`），把 `num_threads` 也纳入截断后，
  **达成满宽的充要条件是 `min(num_threads, K) ≥ inner_omp`**，等价于 `K ≥ inner_omp` 与 `num_threads ≥ inner_omp` 同时成立。
  因此 `K` **没有唯一最小取值**：落在该可行区间内的任一取值给出同一满宽；提高 `K` 只在 `num_threads` 之上仍有富余时才可能改变结果。
- **充分性来源**：`K = num_threads` 且 `num_threads = inner_omp` 时，同时在飞的 scratch 份数 =
  `in_flight × inner_omp ≤ lease`（`../architecture/DATA_FLOW.md` 的轴不变式[1]），
  故总份数与轴形态无关，不越租约。
- **`num_threads = inner_omp` 的来源链**（逐条可核）：
  ① `drizzle_engine.h` 的 `DrizzleConfig::threads` 缺省为 `0`，其注释写明 `0 = 自动 omp_get_max_threads`；
  ② 帧级轴驱动路径上无任何赋值覆盖该缺省（`module_adapters.cpp` 内无 `cfg.threads = …`）；
  ③ `drizzle_engine.cpp` 取 `num_threads = config.threads > 0 ? config.threads : omp_get_max_threads()`，
  而每个帧级 worker 线程在进入 drizzle 前先执行 `omp_set_num_threads(inner_omp)`；
  `omp_get_max_threads()` 返回当前任务 `nthreads-var` 的值，故此处恰为 `inner_omp`。
  另：实现把 `kScratchPoolCap` 的缺省置为 `num_threads`，故「不设环境变量」本身就取到 `K = num_threads`，
  它是缺省策略值而不是被论证出来的唯一最小值。
- **适用域**：本条只覆盖**由 `p1_parallel_for` 驱动**的 drizzle 路径。不经帧级轴调用 drizzle 时，
  `omp_get_max_threads()` 取线程默认 ICV，`num_threads` 与 `inner_omp` 不必相等，本条不成立；
  此时 `K = num_threads` 成立与否取决于 `num_threads ≥ inner_omp` 是否碰巧成立，不是本条保证的。

**等价形态的算例**（补齐全部参数，使「标定形态」与算例同面）：取 `L = 16`、`n = 2`、`K = 2`
（本机标定形态），则 `F = min(16, 闸门) = 2` ⇒ `in_flight = min(2, 2) = 2` ⇒ `I = max(1, 16/2) = 8`，
`W_eff = 2 × min(8, 2) = 4`。把 `K` 由 `2` 提到 `num_threads = 8` 后 `W_eff = 2 × min(8, 8) = 16`
—— 同一组 `(n, L, F, I)` 下只改 `K` 即取到满宽，总份数 `in_flight × K = 2 × 8 = 16 = lease` 未越界。
反之，若形态为 `I = 2`（例如 `L = 8`、`in_flight = 4`），则 `num_threads = 2`，
`K = 2` 与 `K = 4` 等价（后者被 `min(num_threads, kScratchPoolCap)` 截回），提高 `K` 无效 ——
可见 `K = num_threads` 的收益**依赖 `in_flight` 落在哪个档**，不是单调的。

**「`K` 不进数值路径」的判据**：回归锁 `p1drz_merge_pipeline_lock`
（一组在 16 核档下直接比较 `K=2` 与 `K=num_threads` 两条路径的对照实验），
要求逐 leaf 转储 `.canon` 与 `.norm.hiss` 的 sha256 完全一致
（256×1024 高瘦帧 / 64 stripe / FP32+FP64 / `taskset` 预算 1,2,4,8,16 / 每预算两轮重复）；
另有同二进制多档轴形态 A/B 下全部产品**逐位相同**的判据。
**该回归锁当前无执行器**：其脚本路径在现行树中不存在，仓内只余代码注释与实验报告中的引用，
故本条目前**由人读对抗审核判读**，不得在任何判词或台账中记为「门已执行」。
内存代价：每份额外 scratch 的增量落实验域登记。

## 关联锚

- 编排参数语义与轴分配不变式：`../architecture/DATA_FLOW.md`。
- 跨帧零浮点归约（帧结果按下标写各自槽位、join 后帧序归约）：
  `lib/infrastructure/scheduler/src/module_adapters.cpp`。
- 生产实现的 z 映射按 `s` 互斥、祖先归约是互不相交的散射（不存在跨瓦片求和形态）：
  `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（确定性注入面注释）。
- 编排/内存实测读数与拟合：`实验/engineering-evidence/l2_performance/`。

机器 schema：`eng/contracts/schemas/perf_gate_criteria.schema.json`、`eng/contracts/schemas/monitor_field_semantics.schema.json`；判据阈值唯一数值源 = `eng/contracts/resource_gate_v1.json` 的 `compute` 块。

## L2 冻结判据（四条；裁决强度逐条不同）

| # | 判据 | 阈值 | enforcement（唯一数值源逐键给出） | 违规处置 |
|---|---|---|---|---|
| 1 | 平均 CPU 利用率 | ≥ 0.85 | `record_and_justify` | 记录 + 超标登记 |
| 2 | 利用率 p50 | ≥ 0.90 | `record_and_justify` | 记录 + 超标登记 |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | `record_and_justify` | 记录 + 超标登记 |
| 4 | 无「连续 ≥10 s 且利用率 <60% 的低利用窗」（无就绪积压同样计违规） | 无 | `hard_fail` | red |

- **enforcement 取自唯一数值源，不得在文档侧升级或降级**：`eng/contracts/resource_gate_v1.json` 的
  `compute.mean_utilization_enforcement`、`compute.p50_utilization_enforcement`、
  `compute.per_sample_enforcement` 三键同为 `record_and_justify`，只有
  `compute.queue_low_window_enforcement = hard_fail`。数值源给出的理由是：85% 均值门在
  16-worker 真负载上实测仅 65.09%，未标定前不得硬失败。
  **本篇不得把这四条整体表述为 fail-closed**：判据 1–3 不改变裁决，判据 4 才进硬失败清单
  （数值源的 `hard_fail_criteria` 含 `queue_low_window_with_queued_work`）。
- **适用范围**：产品进程内的资源面**不设门**（数值源 `enforcement.in_process_default = record_only`、
  `in_process_hard_fail = retired`）；上述 enforcement 属**外部裁决面**（重计算资源门检查与发布验收），
  其退出码是外部程序的退出码，不是产品命令行退出码。内存、CPU、线程均不在产品面设门。
- 判据阈值本身**保持事前冻结值**（唯一数值源同上）；硬件/算法上限只作证据化上限随判据登记，不 waiver。
- **定义与适用域（自洽说明）**：`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；
  适用域 = 生产重计算面、`effective_cpus ≥ 2` 且采样区间 > 10 s（数值源 `applicability` 的
  `min_effective_cpus = 2`、`min_active_window_seconds_exclusive = 10`）；低于该域记
  `not_applicable`，且该分类**不是豁免**。分母取 `granted_workers_peak`，哨兵 0 不得用配置预算回填。
  **验证方式** = 已归档运行证据回放与红绿双向自测，均由人读对抗审核与实验单元执行，
  **本条不设机器执行面**；`L2-FROZEN-GATE-REPLAY` 在现行树中**无执行器**，判据由人读判读。

## 测量口径（冻结）

- **采样窗**：固定长度窗口（默认 1 s）内的 CPU 时间占比；`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；
- **测量面**：进程级 `/proc/self/stat`（Linux）/ 等价的进程 CPU 时间（Windows）；不含 I/O 等待；
- **积压定义**：待处理任务队列非空且无 worker 空闲；就绪线程判据必须同时满足
  `> 已分配容量核数` 与 `≥ 2`（数值源 `queued_work_predicate`、`queued_work_min_runnable_threads`）；
- **样本集合**：同一 registry SHA 下的完整运行；跨 SHA 不比较；
- **回放**：已归档运行数据必须能被同一检查器重放并给出与当时一致的判定。
  `L2-FROZEN-GATE-REPLAY` 当前**无执行器**，本条由人读判读，不写成可复跑命令。

## worker_balance 指标（正确算法）

```text
utilization_pct = 100 × mean_over_windows( busy_workers_in_window / n_workers )
```

- 判据算法必须与负载相关：「0.5 × 100」式常量与「(min+max)/2」式恒值算法一律判红；
- 判据：合成两组不同负载必须给出**不同**输出（`WORKER-BALANCE-METRIC-REPLAY` 负例）。
  该判据当前**无执行器**，由人读判读。

## 监控字段语义（二选一落地，冻结为「真强制」）

| 字段 | 语义（冻结） | 执行 |
|---|---|---|
| `requires_monitor` | 声明该检查**必须**有监控证据（CPU/RSS/时长采样） | **真强制**：声明为 true 而监控证据缺失/为空/不可解析 ⇒ **判红**（fail-closed）；判定只走具名分支（`else PASS` 属未登记形态） |
| `mutates_workspace` | 声明该检查**会改写工作区**（如生成产物、改配置） | **真语义**：为 true 时检查前后工作区指纹必须**可解释**（改动面 = 声明的 outputs）；指纹对比是唯一判据（「跳过 git 对比」属未登记形态） |

- `requires_monitor` 声明逐个核对执行语义；采样隔离见 `../testing/VALIDATION_EVIDENCE.md`；
- 语义是**真强制**，不改名为 `monitor_capable`；改名属合同变更，走变更流程。

## fail-closed 普查（全部判据）

每条判据必须对三种情况判红：**缺失证据** / **坏证据（不可解析、空文件）** / **无输出**；
普查表见 `实验/engineering-evidence/release-05/FAILCLOSED_SURVEY.md`。

## 基准口径

同一机器、同一数据、同一 config，每 benchmark ≥3 次，记录 median/p95。
基准数据：

```text
Phase1 小真实帧 / 代表性完整帧
Phase2 t4 overlap / GC 3-panel
Browser GC wide / pan / zoom / STF
```

实测读数与逐档墙钟落 `实验/engineering-evidence/`（实测类证据的唯一留档区，
登记口径见 `../../ACSD_DESIGN.md` 的 I/O 与原子产品一章）。

优化类结论必须同时满足：

- 优化前后 science 输出 hash/数值等价；
- 无 >5% 无解释总体回退；
- 未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

**现行基线由实验域给出**：本篇不复制任何基线数值；引用基线必须给出 `实验/engineering-evidence/`
下的落位件名与该件的重复次数口径（≥3 次、记 median/p95）。未落位件名的数值不得写入本篇。

## 参考文献

[1] 内部文档 `docs/engineering/architecture/DATA_FLOW.md`，并行轴分配与轴不变式，帧内 OpenMP 度公式的正本。

[2] 内部文档 `../standards/NUMERIC.md`，数值标准，科学浮点量的量纲与标度词表。

[3] 内部文档 `../build/BUILD_GRAPH.md`，生产构建图，受影响构建目标的反查依据。

[4] 内部文档 `docs/engineering/testing/TEST.md`，测试标准，通用浮点容差与 NaN/Inf 语义的唯一正本。
[5] 内部文档 `docs/science/noise_snr/NOISE_SNR.md`，跨帧绝对信噪比，权重模型的科学定义。
[6] 内部文档 `docs/ACSD_DESIGN.md`，最高设计，CPU 后端与资源一章与 I/O 与原子产品一章，上位来源。
