# Performance Model

> 上游：ASTROCS_DESIGN.md §8（软件架构）

- 热路径禁止 per-pixel alloc/log/fs/clock；per-pixel 数学用连续 buffer。
- 科学精度优先：FP64 reference；FP32 仅显式精度等价路径。
- 关键 fast path 及其 reference：
  - Drizzle candidate conservative test（false negative=0 oracle）；
  - UPM dense cache（sparse evaluate 等价，1e-12）；
  - NoiseWeightModelV1（Monte Carlo/oracle 矩阵）；
  - Gaia 极区 prune（provably-conservative，cache 键精确）。
- 已知性能基线见 docs/performance/BASELINE.md；benchmark 指标挂
  METRIC-* ID（S2 注册）；G-QA 阈值 <5% 回归。

## PERF-501 编排参数标定（探针驱动，2026-09-23）

> 权威：`docs/contracts/SCHEDULER_CONTRACT.md` §3「编排参数（窗口大小、预取深度、帧并发度、
> 工作窃取策略）的最终取值由 PERF-501 基于探针实测数据确定」；`ASTROCS_DESIGN.md` §9（探针驱动优化）。
> 并行轴语义见 `docs/architecture/THREADING_MODEL.md` §并行轴分配；
> 完整证据索引见 `run/PERF-501/EVIDENCE.md`（不入库）。

### 1 实测根因（一手证据）

M42 T2（16 帧 4096²，`run/RELEASE-05/vis/out/m42_p1_t2/resource_timeseries.csv`）的 CPU 曲线
**全程钉在 202%**：4053 个样本中 77.8% 落在 200–299% 桶，`per_thread_cpu_max_pct = 102`
（只有 ~2 条线程在算），而 `workers_peak = granted_workers = 16`。即 **16 核预算只用了 2 核**。

归因（`run/PERF-501/logs/*.child.stderr` 的 `[lease]` / `[p1cap]` 行，均为实测）：

| 量 | 值 | 来源 |
|---|---|---|
| Runtime lease（预算权威） | `cap=16` | `[lease] astrocs.phase1.drizzle host_workers=16 acquired=1 cap=16` |
| 内存闸门 `p1_memory_cap` | `2` | `[p1cap] frame_workers node=drz lease=16 memory_cap=2` |
| 帧级宽度 `p1_frame_workers` | `2` | 同上 `frame_workers=2` |
| 帧内 OpenMP（修复前） | `1` | `p1_parallel_for` 旧式 `(n>=workers)?1:...`，n=16≥2 |

⇒ 实际并行宽度 = 2×1 = **2**。节点瀑布（2 帧冒烟，`run/PERF-501/evidence/before_w16_2f/`）：
calibration 3.23 s / cosmetic 0.0001 s / wcs 6.12 s / star-psf 14.06 s / photometry 1.37 s /
noise-snr 3.66 s / **drizzle 240.30 s（89.4%）** / writer 0.07 s = 268.8 s。
**drizzle 是绝对关键路径，且它以 2 条线程运行。**

### 2 冻结参数（本次改动）

| 参数 | 值 | 落点 | 依据 |
|---|---|---|---|
| 帧内 OpenMP 度 | `max(1, thread_budget / min(n, frame_workers))` | `p1_parallel_for`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 帧级被内存压低时把剩余预算转给帧内轴；帧级未压低时退化为 1（与历史逐位相同） |
| `kP1FrameBytesPerPixel` | **358.0（维持不变）** | 同上 | PERF-501 实测边际 ≈107 B/px，但**抬帧并发会改变 HiPS 产品**（见 §4）⇒ 本次不抬 |
| `kP1FrameMemSafetyFrac` | 0.75（不变） | 同上 | 留基础占用与运行波动 |
| 内存预算百分比 | 95（不变） | `eng/packaging/config/runtime_resources.json` | 单一来源；24.6 GB 机器 MemAvailable 20.77 GB ⇒ 准入预算 19.73 GB |

### 3 前后对照（同一输入、同一注册表、同一代码基线，仅差本次改动）

| 用例 | 修复前 | 修复后 | 加速 | 证据 |
|---|---|---|---|---|
| 2 帧冒烟总墙钟 | 280.6 s | 167.5 s | 1.67× | `run/PERF-501/logs/{before,after}_w16_2f.load` |
| 2 帧冒烟 drizzle 节点 | 240.30 s | 146.28 s | 1.64× | `run/PERF-501/evidence/*/node_waterfall.md` |
| 2 帧冒烟 star-psf 节点 | 14.06 s | 4.02 s | 3.50× | 同上 |
| 2 帧冒烟 wcs 节点 | 6.12 s | 4.87 s | 1.26× | 同上 |
| 2 帧冒烟 CPU 均值 / p50 | 184.5% / 198.2% | 325.8% / 385.6% | 1.77× / 1.95× | `run/PERF-501/{before,after}_w16_2f.monitor.json` |
| 2 帧冒烟峰值 RSS | 3.34 GB | 3.47 GB | +0.13 GB | 同上 |
| M42 T2 16 帧（**仅本次改动**，358 B/px） | 2026.5 s | **1612.2 s** | **1.26×** | `run/RELEASE-05/vis/out/m42_p1_t2/` vs `run/PERF-501/t2_16f_fixA.monitor.json` |
| M42 T2 16 帧 CPU 均值 / p50（仅本次改动） | 190.2% / 202.0% | **304.3% / 387.0%** | 1.60× / 1.92× | 同上 |
| M42 T2 16 帧峰值 RSS（仅本次改动） | 3.47 GB | 3.56 GB | +0.09 GB | 同上 |
| M42 T2 16 帧（叠加试标定 200 B/px，**未采纳**） | 2026.5 s | 1208.3 s | 1.68× | `run/PERF-501/t2_16f_after.monitor.json`（负载 9–13，仅参考） |

**逐位一致性（本次改动在帧并发=2 下的 A/B）**：`before_w16_2f`（旧代码，inner=1）与
`after_w16_2f`（新代码，inner=8）—— 两者帧并发均为 2、lease 均为 16：

| 类型 | 共有 | 逐字节相同 | 不同 |
|---|---|---|---|
| `.fits` | 1464 | **1464** | 0 |
| 目录/星表类 JSON（`p1_sources` / `p1_flux` / `p1_phot` / `p1_snr` / `p1_psf` / `p1_products`） | 6 | **6** | 0 |
| 遥测与路径类（`alloc_*` / `resource_*` / `worker_balance` / `graph/*` / `p1_final` / HiPS `properties`） | — | — | 仅差 output_dir 路径串 / 时间戳 / run_id |

⇒ 帧内 OpenMP 轴对全部科学产物**逐位中性**（与 `p1drz` 的 P15a DRIZZLE-DET-001
「1..16 线程预算逐位恒等」一致）。

### 4 未采纳项与剩余上限（发现但未改）

1. **抬帧并发（`kP1FrameBytesPerPixel` 358 → 200）本次不采纳。**
   实测：抬到 200 后 `[p1cap]` 变为 `memory_cap=4 frame_workers=4 inner_omp=4`，CPU p50 由
   202% 升到 637%、T2 墙钟降到 1208 s；但**同一输入的 HiPS 产品大面积不等价** ——
   16 帧 T2 的 11820 个 FITS 中 **5846 个 DATASUM 变化**（低阶 `Norder0..4` 与叶 `Norder9`
   皆有，抽样可见整幅替换；`p1_sources.json` / `p1_phot.json` 等输入侧产物**完全相同**）。
   帧并发 2 下的同一代码**可重复**（两次独立运行 1464/1464 逐字节相同），故这不是随机性，
   而是**帧并发改变 HiPS hierarchy 归并序**：PERF-PROFILE-01 §9.3 已预警
   `AncestorAcc::add` 按 **tile 到达序** `+=`。修好归并序（按祖先/像素分片或固定合并序）
   之前不得抬本值 —— 否则就是「改科学口径换速度」。
2. **并行宽度的剩余天花板 = 8 条线程**（不是 16）。`drizzle_engine.cpp` 的
   `static constexpr int kScratchPoolCap = 2;` ⇒ `kScratchPool = min(num_threads, 2)`：
   **drizzle 每帧同时只有 2 条线程能持有 scratch map**，其余 OMP 线程在池上等待。
   该上限是 PERF-MEM-FIX-01（F1）为压峰值内存所设（每份 scratch ≈ 每 stripe 触达 tile 数 × 8 MiB）。
   本次修复后实测 CPU p50 = 385.6%（2 帧 × 2 有效线程）；即使把帧并发抬到 4，也只有 8 条有效线程。
   ⇒ 要把 Phase1 推到 G-RES-01 的 85% 均值门，下一步必须动 `kScratchPoolCap`（或改帧内分块），
   并按「每 +1 份 scratch 的实测 RSS 增量」重新标定内存闸门。
3. **`wcs-platesolve` 无帧级并行**（节点内串行 over frames），2 帧冒烟占 3.0%、16 帧 T2 占 3.7%；
   随帧数增长会线性变重，是下一步的次级目标。
4. **HiPS 写出（`hips_write`）在同一帧内与 `drizzle_run` 串行**：16 帧 T2 实测
   `drizzle_profile` 每帧 `drizzle_run≈109 s + hips_write≈34 s`，写出占 drizzle 节点 24%。
### 5 有效并行宽度的可复核推导（G-RES-01 ④⑤⑥ 未达标的量化边界）

**符号**：`A` = 运行时 `MemAvailable`；`P` = 单帧像素数；`B` = `kP1FrameBytesPerPixel`；
`L` = lease（观测到的 `granted_workers`）；`K` = `kScratchPoolCap`；`F` = 帧级并发；
`I` = 帧内 OpenMP 度；`W_eff` = 同时真正在算的线程数。

```
F     = min(L, floor(0.75·A / (P·B)))      // p1_frame_workers（内存闸门）
I     = max(1, L / min(n, F))              // PERF-501 冻结公式
W_eff = F × min(I, K)                      // drizzle 节点；K 为每帧 scratch 槽上限
```

本机实测输入：`A = 20,773,257,216 B`、`P = 4096² = 16,777,216`、`L = 16`。

| B (B/px) | K | F | I | `min(I,K)` | **W_eff** | 实测 CPU p50 | 实测 ④ 平均利用率 |
|---|---|---|---|---|---|---|---|
| 358（当前） | 2 | 2 | 8 | 2 | **4** | 387.0% | 0.190 |
| 200 | 2 | 4 | 4 | 2 | **8** | 637.0% | 0.306 |

**要到 `W_eff = 16`（85% 均值门的必要条件）只有两条路**：

1. 抬 `K`：`K=4` 时 `B=200, F=4, I=4` ⇒ `W_eff = 16`。内存代价按 PERF-MEM-FIX-01 §3 的
   「每份额外 scratch ≈ 0.2–0.3 GB」投影：`4 × (1.8 + 2×0.25) ≈ 9.2 GB` = P1 子预算
   （15.58 GB）的 59% ⇒ **有余量**（该 0.2–0.3 GB/槽是投影值，**未实测**）。
2. 压 `B` 到 ≤116：`F = 8, I = 2, W_eff = 8×2 = 16`。内存 `≈ 8 × 1.8 GB = 14.4 GB`
   = 子预算的 92% ⇒ **无余量**，且 `B` 的抬升被 §4.1 的 HiPS 归并序问题阻塞。

**第二条（更硬的）边界：内核并行效率随宽度衰减。** 实测每帧 `drizzle_run`（同一日志口径）：

| 配置 | 每帧工作线程 | 在飞帧数 | 总工作线程 | `drizzle_run` 每帧 (s) |
|---|---|---|---|---|
| `before_w16_2f` | 1 | 2 | 2 | 199.6 |
| `after_w16_2f` | 2 | 2 | 4 | 109.2 |
| `t2_16f_fixA` | 2 | 2 | 4 | 124.7 |
| `t2_16f_after` | 2 | 4 | 8 | 166.0 |

⇒ 2 → 4 条工作线程：每帧 199.6 → 109.2 s（**1.83×，91% 效率**）；
4 → 8 条工作线程：每帧 109.2 → 166.0 s，节点级净收益只有 **1.42×（1.46×/2，73% 效率）**，
且**每帧时间反而变长**。⇒ 该内核在 ~4 条并发 stripe 之后进入收益递减区。
（注：两次 16 帧运行的外部负载不同——`t2_16f_after` 期间 loadavg 9–13，`t2_16f_fixA` 期间 4–9——
故 4→8 的 73% 是**受污染的保守估计**，需在静默机上复测；但趋势与 PERF-MEM-FIX-01 §3 的
「K=2 使 drizzle 由 ≈40 s/帧升到 167.3 s/帧（×4.2）」一致。）

**分类结论**：

| 类别 | 条目 | 依据 |
|---|---|---|
| **当前实现的结构限制（可改）** | ① `K = 2` 是**策略值**不是物理值（见 §6）；② `B = 358` 是 `K = 16` 时代的标定，与 `K = 2` 耦合后偏保守 3.3×；③ 帧级并发受内存闸门限制；④ HiPS hierarchy 归并序（正确性缺陷，阻塞 ②）；⑤ `hips_write` 在帧内与 `drizzle_run` **串行**，占 drizzle 帧时 28–30%（`t2_16f_fixA`：49.6 s / 174.3 s）；⑥ `wcs-platesolve` 无帧级并行（占 2.9–3.7%） | §6、§4、日志 `[drizzle_profile]`、`[nodetrace]` |
| **物理/算法限制（调度参数不可改）** | ① drizzle 内核并行效率随宽度衰减（4→8 工作线程仅 1.46×）；② 每帧**不可压缩的输出本体**：PERF-MEM-FIX-01 §3 实测 canonical 累加器 ≈2.36 GB（T4 配置 275 tile × 262144 leaf × 32 B），该值不随 `K` 变；③ 机器只有 16 逻辑核 / 8 物理核 / 24.6 GB / 无 swap ⇒ `W_eff ≤ 16`，而 85% 均值门要求 `W_eff ≥ 13.6` 且内核效率 ≥85% | PERF-MEM-FIX-01 §3、本表 |

**结论**：在 `K = 2` 且内核效率 73% 的当前实现下，④ 的上限 ≈ **0.40**（实测 0.306）；
把 `K` 抬到 4 且效率维持 73%，投影上限 ≈ **0.55**（外推，**未实测**）。
⇒ **85% 均值门在当前实现下不可达**；可达路径 = 抬 `K`（一行，须实测内存）+ 修 HiPS 归并序
+ 让 `hips_write` 与 `drizzle_run` 重叠 + 改善内核在 >4 stripe 时的效率。
后两项属算法开发，不属编排参数标定。

### 6 `kScratchPoolCap = 2` 的来历（回答「是推出来的还是拍出来的」）

**结论：不是物理约束推出来的，是 PERF-MEM-FIX-01 明确标注的「本轮交付值」策略选择，且该报告自己就建议负责人裁决。**

依据 `run/RELEASE-05/perf/fix3/REPORT-PERF-MEM-FIX-01.md`：

- §3「代价（诚实边界）」原文：**「K=4 未实测；按『每份额外 scratch ≈ 0.2-0.3 GB、累加并行度翻倍』
  投影：峰值 ≈ 4.2-4.5 GB、drizzle ≈ 85 s/帧。切换只需把 `kScratchPoolCap` 改为 4（一行）。
  **建议由负责人裁决 K=2（内存优先，本轮交付值）还是 K=4（墙钟优先）。**」**
- 同节实测：`K = 16`（旧值）在生产规模「32 帧帧串行、每帧 16 线程」下峰值 **22.3 GB**、
  8 GB 地址空间处 `std::bad_alloc`（RC=134，RSS 6.84 GB）；`K = 2` 后峰值 **3.93 GB** 且完成。
- 同节实测的**唯一硬地板**：`canonical 累加器 275 tile × 262144 leaf × 32 B ≈ 2.36 GB`
  「是不可再压的输出本体」——**这个量不随 `K` 变**，`K` 只决定同时在飞的 stripe 数。
- 代码注释（`drizzle_engine.cpp`）自述取 2 的理由是「**保留『一份在累加、一份在归约』的重叠**」，
  即 `K = 2` 是「能维持流水线重叠的最小值」，不是「内存能承受的最大值」。

⇒ `K = 2` 属**当前实现的结构限制（可改）**，不属物理上限；抬它是一行改动，代价是每份额外
scratch 的实测 RSS 增量（**该增量本身尚未实测**，PERF-MEM-FIX-01 只给了 0.2–0.3 GB 的投影）。
