# Performance Model

> 上游：ASTROCS_DESIGN.md §8（软件架构）

- 热路径内每像素零 alloc/log/fs/clock；per-pixel 数学用连续 buffer。
- 科学精度优先：FP64 reference；FP32 仅显式精度等价路径。
- 关键 fast path 及其 reference：
  - Drizzle candidate conservative test（false negative=0 oracle）；
  - UPM dense cache（sparse evaluate 等价，1e-12）；
  - NoiseWeightModelV1（Monte Carlo/oracle 矩阵）；
  - Gaia 极区 prune（provably-conservative，cache 键精确）。
- 已知性能基线见 docs/performance/BASELINE.md；benchmark 指标挂
  METRIC-* ID（S2 注册）；G-QA 阈值 <5% 回归。

## 1 编排参数标定（探针驱动）

> 权威：`docs/contracts/SCHEDULER_CONTRACT.md` §3「编排参数（窗口大小、预取深度、帧并发度、
> 工作窃取策略）的最终取值由探针基于实测数据确定」；`ASTROCS_DESIGN.md` §9（探针驱动优化）。
> 并行轴语义见 `docs/architecture/THREADING_MODEL.md` §并行轴分配。

### 1.1 实测根因（一手证据）

M42 T2（16 帧 4096²，资源时序 `resource_timeseries.csv`）的 CPU 曲线
**全程钉在 202%**：4053 个样本中 77.8% 落在 200–299% 桶，`per_thread_cpu_max_pct = 102`
（只有 ~2 条线程在算），而 `workers_peak = granted_workers = 16`。即 **16 核预算只用了 2 核**。

归因（探针日志 `[lease]` / `[p1cap]` 行，均为实测）：

| 量 | 值 | 来源 |
|---|---|---|
| Runtime lease（预算权威） | `cap=16` | `[lease] astrocs.phase1.drizzle host_workers=16 acquired=1 cap=16` |
| 内存闸门 `p1_memory_cap` | `2` | `[p1cap] frame_workers node=drz lease=16 memory_cap=2` |
| 帧级宽度 `p1_frame_workers` | `2` | 同上 `frame_workers=2` |
| 帧内 OpenMP 度（无轴分配时） | `1` | `p1_parallel_for` 朴素实现 `(n>=workers)?1:...`，n=16≥2 |

⇒ 实际并行宽度 = 2×1 = **2**。2 帧冒烟节点瀑布实测：calibration 3.23 s / cosmetic 0.0001 s /
wcs 6.12 s / star-psf 14.06 s / photometry 1.37 s / noise-snr 3.66 s /
**drizzle 240.30 s（89.4%）** / writer 0.07 s = 268.8 s。
**drizzle 是绝对关键路径，且它以 2 条线程运行。**

### 1.2 冻结参数

| 参数 | 值 | 落点 | 依据 |
|---|---|---|---|
| 帧内 OpenMP 度 | `max(1, thread_budget / min(n, frame_workers))` | `p1_parallel_for`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 帧级被内存压低时把剩余预算转给帧内轴；帧级未压低时退化为 1 |
| `kP1FrameBytesPerPixel` | **116.0** | 同上 | 同二进制实测**边际 99.68 B/px、base 0.143 GB**（§2）；116.0 使 4096² 帧在 `A ≥ 20.76 GB` 时得 `F=8` ⇒ `W_eff = 8×2 = 16` |
| `kP1FrameMemSafetyFrac` | 0.75 | 同上 | 留基础占用与运行波动 |
| 内存预算百分比 | 95 | `eng/packaging/config/runtime_resources.json` | 单一来源；24.6 GB 机器 MemAvailable 20.77 GB ⇒ 准入预算 19.73 GB |

### 1.3 逐位一致性（帧内轴不进数值路径）

帧内 OpenMP 轴的同帧并发 A/B 对照（帧并发=2、lease=16 恒定，仅帧内轴宽度不同）：
全部 `.fits`（1464/1464）与目录/星表类 JSON（`p1_sources` / `p1_flux` / `p1_phot` /
`p1_snr` / `p1_psf` / `p1_products`，6/6）**逐字节相同**；遥测与路径类工件
（`alloc_*` / `resource_*` / `worker_balance` / `graph/*` / `p1_final` / HiPS `properties`）
仅差 output_dir 路径串 / 时间戳 / run_id。
⇒ 帧内 OpenMP 轴对全部科学产物**逐位中性**（与 `p1drz` 的 P15a DRIZZLE-DET-001
「1..16 线程预算逐位恒等」一致）。

## 2 有效并行宽度的可复核推导（G-RES-01 ④⑤⑥ 边界）

**符号**：`A` = 运行时 `MemAvailable`；`P` = 单帧像素数；`B` = `kP1FrameBytesPerPixel`；
`L` = lease（观测到的 `granted_workers`）；`K` = scratch 池上限（`drizzle_engine.cpp` 的 `kScratchPoolCap`，派生规则见 §3）；
`F` = 帧级并发；`I` = 帧内 OpenMP 度；`W_eff` = 同时真正在算的线程数。

```
F     = min(L, floor(0.75·A / (P·B)))      // p1_frame_workers（内存闸门）
I     = max(1, L / min(n, F))              // 冻结公式
W_eff = F × min(I, K)                      // drizzle 节点；K 为每帧 scratch 槽上限
```

本机实测输入：`A = 20,773,257,216 B`、`P = 4096² = 16,777,216`、`L = 16`。

**实测内存模型**（同一二进制、同场 8 帧 4096²/FP64/auto nside、`inner_omp` 恒 1、
只变 `in_flight`、峰值 RSS 由**外部**采样 `/proc/<pid>/status` VmHWM）：
`RSS(F) = 0.143 GB + F × 1.6724 GB` ⇒ **边际 99.68 B/px、base 0.143 GB**，
拟合残差 ≤ ±2.5%（F=1/2/4/8 四个实测点，F=2 与 F=8 各有两次重复：3.576/3.580 GB、
13.555/13.325 GB）。

| B (B/px) | A=20.77 GB 下 F | I | `min(I,K)` | **W_eff** | 需求 RSS (GB) | 0.75A (GB) | 安全? | 实测墙钟 (s) | 性质 |
|---|---|---|---|---|---|---|---|---|---|
| 358 | 2 | 8 | 2 | **4** | 3.49 | 15.58 | OK | 367.2（F=2 档实测） | 实测 |
| 200 | 4 | 4 | 2 | **8** | 6.83 | 15.58 | OK | 423.3（F=4 档实测） | 实测 |
| **116.0（现行标定值）** | **8** | **2** | **2** | **16** | **13.94** | 15.58 | OK（余量 10.5%） | **421.0**（目标配置实测） | 实测 |
| 103.0 | 9 | 1 | 1 | **9（死区）** | 15.20 | 15.58 | OK 但 W_eff 掉回 9 | 推导 | 推导 |
| 80.0 | 11–12 | 1 | 1 | 11–12（死区） | 18.5–20.2 | 15.58 | **超预算** | 未测（不安全，不采纳） | 推导 |

**关键：`W_eff(F)` 在 `F > L/2` 处非单调 ⇒ "B 越小越好"不成立。**
`I = max(1, L / in_flight)` 是整数除法，故 `F` 一越过 `L/2`，`I` 就掉到 1：
`W_eff = 16` 只在 **`F = 8`**（8×2）或 **`F ≥ 16`**（16×1）取到，`F ∈ [9,15]` 是死区
（`W_eff = 9..15`）。实测印证：`B=80` 在 A=21.5 GB 下放行 `F=12`，按实测模型需求
20.2 GB > 0.75A = 16.13 GB（**超预算 25%**）——这正是"必须实测、不能外推"的理由。

**同帧集 8 帧的实测墙钟（唯一变量 = `W_eff`）**：`W_eff`=2 → **1510.6 s**、
4 → **783.5 s**、8 → **509.2 s**、**16 → 421.0 s**。
⇒ `W_eff`=16 相对 `W_eff`=4 为 **1.86×**。

**第二条硬边界：内核并行效率随宽度衰减。** 实测每帧 `drizzle_run`（同一日志口径）：

| 档位 | 每帧工作线程 | 在飞帧数 | 总工作线程 | `drizzle_run` 每帧 (s) |
|---|---|---|---|---|
| 帧内轴=1 | 1 | 2 | 2 | 199.6 |
| 帧内轴=2 | 2 | 2 | 4 | 109.2 |
| 帧内轴=2（T2 全帧集） | 2 | 2 | 4 | 124.7 |

⇒ 2 → 4 条工作线程：每帧 199.6 → 109.2 s（**1.83×，91% 效率**）；
>4 条并发 stripe 后进入收益递减区，且机器级负载噪声会污染更高档位的测量
（须在静默机上复测）。每帧都存在**不可并行的串行段**（FITS 读、`hips_write`、
星表查询、`wcs-platesolve` 无帧级并行），见 `THREADING_MODEL.md`。

**分类结论**：

| 类别 | 条目 |
|---|---|
| **当前实现的结构限制（可改）** | ① `K` 是**策略值**不是物理值（派生规则见 §3）；② `B` 标定值随实测内存模型更新（现行 116.0，实测边际 99.68 B/px、base 0.143 GB）；③ 帧级并发受内存闸门限制（`B=116` 后本机 `F` = 8）；④ `hips_write` 在帧内与 `drizzle_run` **串行**，占 drizzle 帧时 24–30%；⑤ `wcs-platesolve` 无帧级并行（占 2.9–3.7%）；⑥ `inner_omp = max(1, L/F)` 的整数除法使 `W_eff(F)` 在 `F > L/2` 非单调（`F∈[9,15]` 死区） |
| **物理/算法限制（调度参数不可改）** | ① drizzle 内核并行效率随宽度衰减（>4 stripe 收益递减）；② 每帧**不可压缩的输出本体**：canonical 累加器 ≈2.36 GB（275 tile × 262144 leaf × 32 B），该值不随 `K` 变；③ 机器只有 16 逻辑核 / 8 物理核 / 24.6 GB / 无 swap ⇒ `W_eff ≤ 16`，而 85% 均值门要求 `W_eff ≥ 13.6` 且内核效率 ≥85% |

**结论**：现行标定（`B = 116.0`）下本机（A ≈ 20.8–21.5 GB）`F = 8`、`W_eff = **16**`，
同帧集 8 帧墙钟 **421.0 s**（较 `W_eff = 4` 档 783.5 s 为 1.86×），且全部科学产品**逐字节不变**。
内核效率在 >4 stripe 处的衰减是硬边界；85% 均值门是否可达，须在 `W_eff = 16` 下
实测利用率判定。剩余可达路径 = 让 `hips_write` 与 `drizzle_run` 重叠
+ 改善内核在 >4 stripe 时的效率；后两项属算法开发，不属编排参数标定。

## 3 scratch 池上限的派生（`K = num_threads`）

`kScratchPoolCap` 取**派生值 `num_threads`**，不硬钉。定理（§2 记号）：
`W_eff = in_flight × min(inner_omp, K)` ⇒ 要 `W_eff` 达到帧内轴宽度必须
`K ≥ inner_omp`；而 `K = inner_omp = num_threads` 时同时在飞的 scratch 份数
`= in_flight × inner_omp ≤ lease`（`THREADING_MODEL.md`「并行轴分配」的轴不变式）
⇒ **`K = num_threads` 是达成满宽的唯一最小取值，且总份数与轴形态无关**；
`K < inner_omp` 会让多余线程在池上空等（实测 `W_eff=4` 档 CPU p50 仅 385.6%）。
在当前标定形态（`I = 2`）下 `K = num_threads` 与 `K = 2` 等价（no-op）；
它保证 `I > 2` 的形态可达满宽（例：`n = 2` 帧 ⇒ `F=2, I=8`，`W_eff` = 2×min(8,2)=4
抬到 2×8=16，**4×**）。

**「K 不进数值路径」由实测证实（非只引注释）**：既有回归锁
`p1drz_merge_pipeline_lock` **PASS**（256×1024 高瘦帧 / 64 stripe / FP32+FP64 /
`taskset` 预算 1,2,4,8,16 / 每预算两轮重复，要求逐 leaf 转储 `.canon` 与 `.norm.hiss`
sha256 完全一致 —— 该锁在 16 核档下直接比较 `K=2` 与 `K=num_threads` 两条路径）；
另在同二进制四档轴形态 A/B 下全部产品**逐位相同**。
内存代价投影：每份额外 scratch ≈ 0.2–0.3 GB（实测帧内轴 `inner_omp` 1→8，
2 帧在飞，峰值 3.576 → 3.825 GB，**+0.249 GB**，落在投影区间内）。

## 4 关联锚

- 编排参数语义与轴分配不变式：`docs/architecture/THREADING_MODEL.md`。
- 跨帧零浮点归约（帧结果按下标写各自槽位、join 后帧序归约）：`lib/infrastructure/scheduler/src/module_adapters.cpp:2218`（代码侧记载）。
- 生产实现的 z 映射按 `s` 互斥、祖先归约是互不相交的散射（不存在跨瓦片求和形态）：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:771-777`（确定性注入面注释）。
- 性能基线与测量工件：`docs/performance/BASELINE.md`。
