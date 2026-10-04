# 性能模型与资源判据

上游：最高设计的软件架构、CPU 后端与资源两章；并行轴分配见 `../architecture/DATA_FLOW.md`。

结构原则、编排参数标定、有效并行宽度模型、冻结标定常数与合成性能判据的正本。
实测读数与逐档墙钟落实验域的工程证据面，本分册只承载结构结论与冻结参数。

## 结构原则与来源

- 热路径内每像素零 alloc/log/fs/clock；per-pixel 数学用连续 buffer。
- 科学精度优先：FP64 reference；FP32 仅显式精度等价路径。
- 关键 fast path 与各自 reference（逐条给定义与验证方式）：
 - **Drizzle candidate conservative test**：候选判定取保守上界（宁可多算、不可漏算）；
 验证方式 = 与穷举候选集比对时 **false negative 恒为 0**（本仓 oracle）。
 - **UPM dense cache**：稠密缓存与逐点稀疏求值必须给出同一结果；
 验证方式 = 缓存命中域上的最大相对偏差 ≤ 1e-12。定义与适用域见
 `../../science/PHASE2_UPM.md`。
 - **NoiseWeightModelV1**：权重模型以 oracle 矩阵与 Monte Carlo 双向核对；
 定义见 `docs/science/noise_snr/NOISE_MODEL`。
 - **Gaia 极区 prune**：剪枝用**可证明保守**的球面判据（不丢候选）；cache 键精确匹配，
 实现锚 = `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` 的 `query_cache_lookup`。

- 性能基线与回归判据见 `PERFORMANCE_MODEL.md`；超过基线回归阈值（<5%）即判红。
- 编排与内存的实测读数、拟合与逐档墙钟见 `实验/engineering-evidence/l2_performance/`；
 本文件只承载结构结论与冻结参数。

## 编排参数标定（探针驱动）

> 上游条款：「scratch 池上限的派生」一节（编排参数的**最终取值**由探针基于
> 实测数据确定，合同只保证机制正确与探针齐全）；`../../ACSD_DESIGN.md` 「CPU 后端与资源」一节（探针驱动优化）。
> 并行轴语义（含帧内 OpenMP 度公式与轴不变式）唯一正本 = `../architecture/DATA_FLOW.md`；
> 本节只给编排参数的结构结论与冻结取值，不复制轴公式。

### 结构性根因

帧级并发受**内存闸门**限制：Runtime lease 给出的 CPU 预算在 4096² 级帧上不能全量转成帧级并发；
实际帧级宽度由内存预算与单帧驻留字节数决定。后果是**预算剩余**：CPU 用量远低于 lease，
剩余预算必须转给**帧内轴**才能被利用。

帧内尚存的**串行段**：`hips_write` 在帧内与 `drizzle_run` 串行；`wcs-platesolve` 无帧级并行。
逐节点瀑布、CPU 曲线与串行段占比读数见 `实验/engineering-evidence/l2_performance/`。

### 冻结参数

| 参数 | 值 | 落点 | 依据 |
|---|---|---|---|
| 帧内 OpenMP 度 | 见 `../architecture/DATA_FLOW.md`「并行轴分配」 | `p1_parallel_for`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 帧级被内存压低时把剩余预算转给帧内轴；帧级未压低时退化为 1 |
| `kP1FrameBytesPerPixel` | **116.0** | 同上 | 单帧驻留字节数的标定值；取保守上界（内存模型见 「测量口径」一节） |
| `kP1FrameMemSafetyFrac` | 0.75 | 同上 | 留基础占用与运行波动 |
| 内存预算百分比 | 95 | `eng/packaging/config/runtime_resources.json` | 单一来源；准入预算 = `MemAvailable` 按该比例的派生值 |

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
`kScratchPoolCap`，派生规则见 「worker_balance 指标」一节）；`F` = 帧级并发；`I` = 帧内 OpenMP 度；
`W_eff` = 同时真正在算的线程数。

```
F = min(L, floor(kP1FrameMemSafetyFrac·A / (P·B))) // 帧级内存闸门
I = max(1, L / min(n, F)) // 帧内轴（正本 = THREADING_MODEL.md）
W_eff = F × min(I, K) // drizzle 节点；K 为每帧 scratch 槽上限
```

**内存模型形态**：单帧驻留 = `base` + `边际字节/像素` × `F`；标定取保守上界 `B = 116.0`。
本机实测的模型系数、逐档峰值 RSS 与墙钟读数见 `实验/engineering-evidence/l2_performance/`。

**结构性结论（与具体读数无关）**：

1. **`W_eff(F)` 在 `F > L/2` 处非单调**：`I = max(1, L / F)` 是整数除法，
 `F` 一越过 `L/2` 就掉到 1 ⇒ `W_eff` 在 `F ∈ (L/2, L)` 形成**死区**（达不到满宽）；
 满宽只在 `F = L/2`（×2）或 `F ≥ L`（×1）取到。故「`B` 越小越好」不成立，
 标定必须实测、不能外推。
2. **内核并行效率随宽度衰减**：帧内 stripe 数超过约 4 后进入收益递减区，
 且机器级负载噪声会污染更高档位的测量（须在静默机上复测）。每帧存在**不可并行的串行段**
 （FITS 读、`hips_write`、星表查询、`wcs-platesolve`），见 `../architecture/DATA_FLOW.md`。
3. **内存闸门是准入边界**：若某档的 `F` 使需求 RSS 超过 `kP1FrameMemSafetyFrac·A`，
 该档不采纳 —— 安全边界优先于并行宽度。
4. **`hips_write` 串行**是 drizzle 帧时的固定占比项，不随 `F` 缩小。

**分类结论**：

| 类别 | 条目 |
|---|---|
| **当前实现的结构限制（可改）** | ① `K` 是**策略值**不是物理值（派生规则见 「worker_balance 指标」一节）；② `B` 标定值随内存模型更新；③ 帧级并发受内存闸门限制；④ `hips_write` 在帧内与 `drizzle_run` **串行**；⑤ `wcs-platesolve` 无帧级并行；⑥ `inner_omp = max(1, L/F)` 的整数除法使 `W_eff(F)` 在 `F > L/2` 非单调（死区，见结构结论 1） |
| **物理/算法限制（调度参数不可改）** | ① drizzle 内核并行效率随宽度衰减（stripe 超约 4 后收益递减）；② 每帧**不可压缩的输出本体**：canonical 累加器容量不随 `K` 变；③ 逻辑核 / 物理核 / 物理内存上限共同决定 `W_eff` 的取值上界 |

**结论**：在标定值 `B = 116.0` 下，本机可同时达到帧级与帧内两轴的满宽，且全部科学产品
**逐字节不变**。内核效率随 stripe 数增大的衰减是硬边界；目标 `W_eff` 档位的均值利用率
是否可达，须在该档位实测判定（读数见 `实验/engineering-evidence/l2_performance/`）。
剩余可达路径 = 让 `hips_write` 与 `drizzle_run` 重叠 + 改善内核在高 stripe 数时的效率；
后两项属算法开发，不属编排参数标定。

## scratch 池上限的派生（`K = num_threads`）

`kScratchPoolCap` 取**派生值 `num_threads`**，不硬钉。定理（「测量口径」一节 记号）：
`W_eff = in_flight × min(inner_omp, K)` ⇒ 要 `W_eff` 达到帧内轴宽度必须 `K ≥ inner_omp`；
而 `K = inner_omp = num_threads` 时同时在飞的 scratch 份数 = `in_flight × inner_omp ≤ lease`
（`../architecture/DATA_FLOW.md`「并行轴分配」的轴不变式）
⇒ **`K = num_threads` 是达成满宽的唯一最小取值，且总份数与轴形态无关**；
`K < inner_omp` 会让多余线程在池上空等。
在标定形态（`I = 2`）下 `K = num_threads` 与 `K = 2` 等价（no-op）；
它保证 `I > 2` 的形态可达满宽（例：`n = 2` 帧 ⇒ `F = 2, I = 8`，
`W_eff` 由 `2×min(8,2) = 4` 抬到 `2×8 = 16`）。

**「`K` 不进数值路径」的判据（非只引注释）**：回归锁 `p1drz_merge_pipeline_lock`
（一组在 16 核档下直接比较 `K=2` 与 `K=num_threads` 两条路径的对照实验），
要求逐 leaf 转储 `.canon` 与 `.norm.hiss` 的 sha256 完全一致
（256×1024 高瘦帧 / 64 stripe / FP32+FP64 / `taskset` 预算 1,2,4,8,16 / 每预算两轮重复）；
另有同二进制多档轴形态 A/B 下全部产品**逐位相同**的判据。内存代价：每份额外 scratch 的增量
落在 `实验/engineering-evidence/l2_performance/` 登记的投影区间内。

## 关联锚

- 编排参数语义与轴分配不变式：`../architecture/DATA_FLOW.md`。
- 跨帧零浮点归约（帧结果按下标写各自槽位、join 后帧序归约）：
 `lib/infrastructure/scheduler/src/module_adapters.cpp`。
- 生产实现的 z 映射按 `s` 互斥、祖先归约是互不相交的散射（不存在跨瓦片求和形态）：
 `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（确定性注入面注释）。
- 性能基线与回归判据：`PERFORMANCE_MODEL.md`。
- 编排/内存实测读数与拟合：`实验/engineering-evidence/l2_performance/`。

机器 schema：`eng/contracts/schemas/perf_gate_criteria.schema.json`、`eng/contracts/schemas/monitor_field_semantics.schema.json`；判据阈值正本 = 「L2 冻结判据」一节（L2 冻结判据，四条全部为真判红），阈值唯一数值源 = `eng/contracts/resource_gate_v1.json`。

## L2 冻结判据（四条，全部为**真判红**）

| # | 判据 | 阈值 | 违规 |
|---|---|---|---|
| 1 | 平均 CPU 利用率 | ≥ 0.85 | red |
| 2 | 利用率 p50 | ≥ 0.90 | red |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | red |
| 4 | 无「连续 ≥10 s 且利用率 <60% 的低利用窗」（无就绪积压同样计违规） | 无 | red |

- **enforcement = fail-closed**：任一判据违规 ⇒ `verdict=red`；`record_and_justify` 只是无违规样本的**记录语义**，不参与裁决；
- 判据阈值本身**保持事前冻结值**（唯一数值源 = `eng/contracts/resource_gate_v1.json::compute`）；硬件/算法上限只作证据化上限随判据登记，不 waiver。
- **定义与适用域（自洽说明）**：`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；适用域 = 生产重计算面、`effective_cpus ≥ 2` 且采样区间 > 10 s；**验证方式** = 已归档运行证据回放（`实验/engineering-evidence/l2_performance/gates/`）与红绿双向自测——两者均由人读对抗审核与实验单元执行，**本条不设机器执行面**。

## 测量口径（冻结）

- **采样窗**：固定长度窗口（默认 1 s）内的 CPU 时间占比；`utilization = busy_cpu_seconds / (window_seconds × n_workers)`；
- **测量面**：进程级 `/proc/self/stat`（Linux）/ 等价的进程 CPU 时间（Windows）；不含 I/O 等待；
- **积压定义**：待处理任务队列非空且无 worker 空闲；
- **样本集合**：同一 registry SHA 下的完整运行；跨 SHA 不比较；
- **回放**：已归档运行数据必须能被同一检查器重放并给出与当时一致的判定（`L2-FROZEN-GATE-REPLAY`）。

## worker_balance 指标（正确算法）

```text
utilization_pct = 100 × mean_over_windows( busy_workers_in_window / n_workers )
```

- 判据算法必须与负载相关：「0.5 × 100」式常量与「(min+max)/2」式恒值算法一律判红；
- 判据：合成两组不同负载必须给出**不同**输出（`WORKER-BALANCE-METRIC-REPLAY` 负例）。

## 监控字段语义（二选一落地，冻结为「真强制」）

| 字段 | 语义（冻结） | 执行 |
|---|---|---|
| `requires_monitor` | 声明该检查**必须**有监控证据（CPU/RSS/时长采样） | **真强制**：声明为 true 而监控证据缺失/为空/不可解析 ⇒ **判红**（fail-closed）；判定只走具名分支（`else PASS` 属未登记形态） |
| `mutates_workspace` | 声明该检查**会改写工作区**（如生成产物、改配置） | **真语义**：为 true 时检查前后工作区指纹必须**可解释**（改动面 = 声明的 outputs）；指纹对比是唯一判据（「跳过 git 对比」属未登记形态） |

- `requires_monitor` 声明逐个核对执行语义，对照表见「监控字段语义」一节（监控字段语义）；采样隔离见 `../testing/VALIDATION_EVIDENCE.md` ；
- 语义是**真强制**，不改名为 `monitor_capable`；改名属合同变更，走变更流程。

## fail-closed 普查（全部判据）

每条判据必须对三种情况判红：**缺失证据** / **坏证据（不可解析、空文件）** / **无输出**；普查表见 `实验/engineering-evidence/`。

同一机器、同一数据、同一 config，每 benchmark ≥3 次，记录 median/p95。
基准数据：

```text
Phase1 小真实帧 / 代表性完整帧
Phase2 t4 overlap / GC 3-panel
Browser GC wide / pan / zoom / STF
```

完整数值见 `实验/engineering-evidence/**`（实测类证据的唯一留档区，登记 = `../../ACSD_DESIGN.md` 「I/O 与原子产品」一节（I/O 与原子产品）（:151））。原引 `evidence/performance/*.json`（V14 交付）**在本仓不存在**（死指针，本行原句已订正）⇒ 下文 V14 / V18R2 读数为**历史读数、原始 JSON 未入库**，只作历史参照，不作现行基线证据；补做现行基线须重跑并按 `../../ACSD_DESIGN.md` 「I/O 与原子产品」一节（I/O 与原子产品） 落 `实验/engineering-evidence/`。

V14 首轮结果：

```text
Phase1 panel1 65.0s（3 runs）
Phase2 GC 292.0s -> 234.6s（-20%）
Phase2 t4 87.9s -> 70.8s（-19%）
Browser shot 2.43s；zoom 0.30s/f；pan 0.22s/f
```

规则：

- 优化前后 science 输出 hash/数值等价；
- 无 >5% 无解释总体回退；
- 未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

---

## V18R2 资源驱动轮（性能基线）

> **口径诚实边界**：本节数值为**单次读数**，不满足上文「每 benchmark ≥3 次、记 median/p95」的自订口径 ⇒ 只作历史参照，不构成现行基线结论；原始读数工件未登记入库。

```text
Phase1 ~67.35 s/frame
Drizzle ~64 s/frame（16-frame batch）
资源驱动轮 126.65 s -> 67.35 s
RSS 37.5 GB -> 1.2 GB
PLATESOLVE 15 s -> 0.16 s
```

规则同前：优化前后 science 输出 hash/数值等价；无 >5% 无解释总体回退；未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/architecture/DATA_FLOW.md`，同层相关正本。
[3] 内部文档 `docs/engineering/standards/NUMERIC.md`，同层相关正本。
