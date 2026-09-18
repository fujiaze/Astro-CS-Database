# RELEASE-02 · drizzle 热点分析与并行化可行性评估 (PERF-DRZ)

- 任务：定位每帧 16.7M 输入像素重采样主循环的热点、审查算法复杂度、评估并行化可行性。
- 范围：**只分析，不改代码**（零 git 写权限；未跑 ninja/cmake/ctest）。
- 工作目录：`/workspace/Astro CS Database`
- 证据目录：`run/RELEASE-02/perf-drz/`

---

## 0. 结论摘要 (TL;DR)

> **任务书给出的前提「drizzle 平均仅 1.21 核、近乎单线程、是 drizzle 自身没并行」不成立。**
> 这是一个**测量脚本的时间区间对齐缺陷**造成的伪信号；drizzle 早已实现数据并行，
> 在 L4 实测里 **平均 12.82 核 / 峰值 14.25 核**，重采样区段（`par_wall`）达 **15.43×/16 核**（96.5% 效率）。

1. **勘误（最高优先级）**：`run/RELEASE-02/L4-rebuild/stage_cpu.py` 把探针 *阶段级* 事件的
   `ts_utc`（**结束时刻**）当作**开始时刻**，导致 drizzle 被采样到「阶段结束后的空闲尾段」。
   原脚本输出 1.21 核；按正确窗口 `[t_end-w, t_end]` 重算得 **12.82 核**。详见 §1。
   **该脚本的整张表（wcs 9.62 / noise 7.53 / …）都不可用**，且排名被反转。
2. **热点**（单帧实测，16 线程）：重采样 33.77s 墙钟里 CPU 31.55s/px 归一后为
   **候选查询 ~15.14 µs/px + 球面重叠 ~14.55 µs/px**（各占重采样 CPU 的 ~48%/46%），
   WCS 仅 0.49 µs/px（行级顶点缓存已生效）。真正的内层循环是
   `drizzle_engine.cpp:1512` 的候选循环（2.316 亿次），**不是** `:118`（诊断用 trace 回退，L4 未开启）。
3. **复杂度问题**：候选枚举过度枚举（49 格/像素 → 仅 13.8 保留，28% 命中率）、
   `TargetGeomCache` 的 LRU 命中路径是 **O(capacity=8192) 线性扫描**（1.487 亿次命中）、
   缓存抖动（容量 8192 << 每 stripe 工作集 ~9.3 万叶，命中率仅 64.2%）。
4. **并行化**：**已到位且接近最优**，不需要再做数据并行。剩余串行尾只有
   HiPS 直写 3.80s + 归约 0.98s/帧（12.7%）。把它全部并行化也只带来 **1.075×**（单帧）
   / **1.044×**（整跑 3438s）。真正的头寸在**算法**而非并行：见 §5。
5. **科学红线**：现有并行实现已用「固定 stripe + 升序归约」保证**逐位可复现且与线程数无关**
   （P15a DRIZZLE-DET-001）。任何后续优化必须保持该性质。

---

## 1. 数据勘误：1.21 核是怎么来的

### 1.1 复现原口径

```
$ python3 run/RELEASE-02/L4-rebuild/stage_cpu.py
stage           total_s  cpu_mean(核)      calls
drizzle          2076.2         1.21         12      <-- 任务书表格
star_psf          175.9         2.92         12
noise             172.0         7.53         12
photometry        159.1         3.34         12
wcs               115.0         9.62         12
calibrate          46.4         1.00         12
```

原脚本逐字复现了任务书表格 ⇒ 任务书数据确实来自该脚本。

### 1.2 缺陷：阶段级事件是「结束时刻」

探针里 `drizzle.frame` 与阶段级 `drizzle` 的 `ts_utc` 是**结束时刻**，`wall_us` 才是耗时：

| 事件 | ts_utc (end) | wall_us | 反推 start |
|---|---|---|---|
| drizzle.frame #1 | …086.761 | 35.663 | …051.098 |
| drizzle.frame #2 | …127.516 | 40.755 | …086.761 |
| drizzle (stage)  | …127.516 | 76.418 | …051.098 |

两帧首尾相接（frame2.start == frame1.end），且 `frame1.wall + frame2.wall == stage.wall`。
另：`events.jsonl` 的 `stage_start` = 15:20:32Z，而 `calibrate` 阶段 end=15:20:34.387、
wall=2.366 ⇒ start=15:20:32.021 = 进程起点。**END 语义确凿。**

原脚本用 `a=t-t0; b=a+w`（把 end 当 start），drizzle 窗口被算成
`[93.1s, 169.5s]`，而真实窗口是 `[19.1s, 95.5s]`；该配置 `resource_timeseries.csv`
只到 95.5s ⇒ 窗口几乎全空，空窗按 `cpu=0` 计入加权平均 ⇒ 1.21 核。

### 1.3 正确重算

```
$ python3 run/RELEASE-02/perf-drz/stage_cpu_fixed.py
stage              wall_s  FIXED cores |  (BUG 口径复现)
drizzle            2076.2        12.82   |        0.86
star_psf            175.9         5.83   |        2.66
noise               172.0         3.62   |        9.04
photometry          159.1         2.02   |        4.52
wcs                 115.0         5.35   |       13.09
calibrate            46.4         1.06   |        6.06
cosmetic              8.1         2.09   |        1.95
TOTAL              2752.8
```

**drizzle 逐帧（49 帧，修正后）：mean 12.88 核，min 10.83，max 14.25。**

独立交叉验证（不依赖探针对齐）：`run/RELEASE-02/L4-rebuild/logs/t2_m1_red.resource_summary.json`
报告整配置 `cpu_pct_mean=1175.41`（11.75 核）、`cpu_pct_p50=1532`（15.32 核）；
`timings.csv` 里 `t2_m1_red` 墙钟 113.8s、user 1134.5s（≈10 核）。drizzle 窗口内
`resource_timeseries.csv` 的 `cpu_pct` 为 1450–1600（14.5–16 核）。

### 1.4 旁证：drizzle 的并行代码确实存在且被 L4 二进制执行

- `nm -C build/astrocs | grep drizzleTiledImpl` 有 `[clone ._omp_fn.*]` 并行 outline 符号；
- 任务书声称的 `grep "parallel|thread|Executor|worker" drizzle_engine.cpp` **零命中不成立**：
  该文件含 `#pragma omp parallel`（:1796）、`num_threads`（:1729）、大量 `thread_local`。

---

## 2. ① 热点定位（到函数/循环/行）

### 2.1 调用链（Phase1 HiPS 直写）

```
build/astrocs normalize
 └─ scheduler p1_op_drizzle            lib/infrastructure/scheduler/src/module_adapters.cpp:3693
     └─ hp_drizzle_run_phase1_hips      hp_drizzle_hips_api.cpp:34
         └─ run_drizzle_internal        hp_drizzle_api.cpp:521
             └─ DrizzleEngine::drizzleTiled(_f64)  drizzle_engine.cpp:2168/2184
                 └─ drizzleTiledImpl    drizzle_engine.cpp:1645
                     └─ #pragma omp parallel (:1796)
                         └─ for y (:1871) / for x (:1889)
                             └─ processPixelSharedTiled  (:1360)
                                 ├─ build_drop_geometry_into          (:1457)
                                 ├─ query_candidate_pixels_fast       (:1475)  <-- 热点 A
                                 └─ for (uint64_t ipix : candidates)  (:1512)  <-- 热点 B 内层
                                     └─ compute_overlap_area_g_ctx_cached (:1517)
                                         ├─ TargetGeomCache::get_or_build (:1439)
                                         └─ overlap_area_impl             (:1441)
```

### 2.2 单帧实测（`ASTROCS_DRIZZLE_FINE_PROFILE=1`，M42_M1_T2 …012404-300S-Red）

```
[lease] astrocs.phase1.drizzle host_workers=16 acquired=1 cap=16
[p22]   par_wall=33.767 accum_cpu=521.208 merge_wait_cpu=18.052
        merge_work_cpu=0.981 worst_thread=33.767 n_stripes=256 nthreads=16 canontiles=115
[profile] wcs=8.174s geom=511.271s cand=254.004s overlap=244.103s (threads=16, 16.8M px)
[ops]   src=16777216 cand=231571683 true_ov=97491005 quick_rej=134080678
        gcache_hit=148723744 gcache_miss=82847939 tile_lk=97491005 heap_alloc=16
[drizzle_profile] drizzle_run=33.768s hips_write=3.800s total=37.606s
```

**每输入像素成本（16.78M px）**

| 分量 | 墙钟 µs/px | CPU µs/px | 占重采样 CPU |
|---|---|---|---|
| 重采样并行区 | **2.013** | — | — |
| ├ WCS（行级顶点缓存） | — | 0.487 | 1.5% |
| ├ 几何/重叠主循环 `processPixelSharedTiled` | — | **30.474** | 96.5% |
| │ ├ **候选查询** `query_candidate_pixels_fast` | — | **15.140** | 47.9% |
| │ ├ **球面重叠** `compute_overlap_area_g_ctx_cached` | — | **14.550** | 46.1% |
| │ └ drop 几何构建 + 累加 + TLS | — | 0.785 | 2.5% |
| HiPS 直写（串行） | 0.226 | — | — |
| **整帧合计** | **2.241** | 31.55 | — |

> 任务书的「2.5 µs/像素」（=4096²/~42s）是**墙钟**口径，且**已经是 16 核摊薄后的值**。
> 单核 CPU 口径是 **~31.6 µs/像素**。回答「2.5µs 花在哪」：花在
> **候选查询（~48%）+ 球面重叠（~46%）**，两者都在 `processPixelSharedTiled` 内。

### 2.3 热点 A：候选查询（`spherical_overlap.cpp:1525-1685`）

每源像素执行：
1. 求 drop 包围圆：4 顶点求和 + 归一化 + 4 次 `acos`（:1538-1551）；
2. `hp.radec2pix` 求中心叶（:1565），morton 解交织取 face 内 (ix,iy)（:1568-1581）；
3. 半径转像素：`delta = ceil(radius_px * 1.15)`（:1619）。本配置 hp_res=0.805192″、
   源像素 0.9405″ ⇒ `query_radius≈1.63″` ⇒ `delta=3` ⇒ **7×7=49 格/像素**（:1659-1664）；
4. 对 49 格逐个 `hp.pix2ang` + 2×`sin`/2×`cos` 做圆心距离预过滤（:1672-1682）；
5. `std::sort(candidates)`（:1684）。

实测 **231.57M 保留候选 / 16.78M 源像素 = 13.8/px**（枚举 49 → 保留率 **28%**），
即 **~822M 次 pix2ang/三角预过滤**。候选查询 CPU = 254.0s/帧 = **1.10 µs/候选**。

### 2.4 热点 B：球面重叠（`spherical_overlap.cpp:1427-1443 → 1166-1332`）

每候选：`TargetGeomCache::get_or_build`（:1383）取叶中心/4 角 → `overlap_area_impl`（:1166）：
- 包围圆 quick-reject（:1193-1204）；
- `leaf_fully_inside_drop`（4×4=16 点积，:1221-1235）；
- `drop_inside_pixel`（4 边 cross + 4×4 点积，:1242-1267）；
- 部分相交 → 一次四边形球面 S-H 裁剪 + Girard/Eriksson 面积（:1275-1287）。

实测 231.57M 候选：**57.9% quick-reject、42.1% 真重叠**；几何缓存 **64.2% 命中 / 35.8% 未命中**
（未命中 82.85M 次需重建 `pix2radec`+4 角边界）。重叠 CPU = 244.1s/帧 = **1.05 µs/候选**。

### 2.5 关于 `:118` vs `:1512`（任务书明确要求判断）

| 位置 | 代码 | 是否主成本 | 依据 |
|---|---|---|---|
| `drizzle_engine.cpp:118` `for (int64_t i=0;i<total;i+=stride)` | `drizzle_trace::ensure_selection` 的**诊断**回退采样 | **否** | 仅当 `ASTROCS_DRIZZLE_TRACE` 置位且无 `trace_selection.tsv` 时执行；循环上限 `want=min(total,1024)`，即最多 ~1024 次/帧。L4 未设该 env，`init_from_env` 直接返回，整段不执行。 |
| `drizzle_engine.cpp:1512` `for (uint64_t ipix : candidates)` | 逐候选重叠计算 + 累加 | **是** | 实测 **231,571,683 次**/帧，占重采样 CPU 的 46%。 |

**结论：`:1512` 是主成本，`:118` 与热点无关。**

---

## 3. ② 算法复杂度审查

### 3.1 可避免的重复计算

| # | 问题 | 位置 | 量级 | 说明 |
|---|---|---|---|---|
| C1 | **LRU 命中路径 O(capacity) 线性扫描** | `spherical_overlap.cpp:1389-1395` | 1.487 亿次命中，容量 8192 | 每次命中遍历 `std::deque lru_` 找 ipix 再 erase+push_front；结构上 O(N·cap)，最坏 8192 步/命中 |
| C2 | **几何缓存抖动** | `TargetGeomCache`（:303-330），容量 8192 | 命中率仅 64.2%，82.85M 次重建 | 全图 23.9M 个目标叶，256 stripe ⇒ 每 stripe 工作集 ~9.3 万叶 >> 8192 ⇒ 反复淘汰重建 `pix2radec`+边界 |
| C3 | **候选过度枚举** | `query_candidate_pixels_fast` :1619,1659-1664 | 49 枚举/像素，仅 13.8 保留（28%） | 包围盒 (2δ+1)² 全枚举，且每个被丢弃格都付了 `pix2ang`+三角 |
| C4 | **每源像素一次 `std::sort` + 一次 `filtered` 局部 vector 分配** | :1670-1684 | 16.78M 次 sort / 16.78M 次 malloc | `std::vector<uint64_t> filtered` 是局部对象，逐像素堆分配/释放（未计入 `heap_alloc` 计数器） |
| C5 | **每真重叠一次 `tileMap[parent]` unordered_map 查找** | `drizzle_engine.cpp:1549` | 9749 万次 | `std::unordered_map` 哈希查找；`tile.leaf(local)` 再做越界扩容检查 |
| C6 | **drop 几何逐像素重建** | :1457 → `build_drop_geometry_into` :1038 | 16.78M 次 | 4 角多趟扫描 + 2 次 `acos` + 归一化；相邻行的 drop 顶点高度共享（pixfrac=1 已复用顶点，但几何未按行复用） |
| C7 | **profiler 自身的 thread_local 累计 bug** | `drizzle_engine.cpp:317-318` | — | `g_tl_prof_cand/g_tl_prof_overlap` 从不重置，同进程第 2 帧起 `[profile]` 为累计值（本次分析只取第 1 帧）。仅影响观测，不影响数值 |

### 3.2 超线性结构

- **没有** O(N²) 的像素级结构：候选搜索是局部有界的（δ≤nside-1，本配置 δ=3）。
- **唯一准超线性**是 C1 的 LRU（O(命中数 × 容量)）与 C2 的抖动（容量不足导致的重复构建）。
- 归约：`merge_tile_map_into`（:1611）按 touched 叶迭代，全帧 leaf_ops=25.8M、串行 merge 仅 0.98s，不构成瓶颈。

### 3.3 预计算/向量化空间

- **已有**：行级 WCS 顶点缓存（`drizzle_engine.cpp:1808-1887`，每像素 2 次 `pixelToSky` 而非 4 次）；
  run 常量（`hp_res_rad`/`cos_thresh_60`/shift/mask）已 hoist 到 `DrizzleRunContext`（:241-248, :1770-1776）；
  `thread_local` scratch 复用（`t_drop_corners`/`t_candidates`/`t_drop_geom`），`heap_alloc=16`。
- **可做**：把候选枚举的 49 格 `pix2ang` 预过滤改为面内解析中心（或直接对盒内格计算面坐标到球面距离的上界），
  避免 822M 次三角；把 `TargetGeomCache` 改为 O(1) 触摸的侵入式链表或 clock 缓存，并按 stripe/输出 tile 重排访问以消除抖动。

---

## 4. ③ 并行化可行性

### 4.1 主循环是否可安全数据并行？—— **已经是了**

主循环是「源像素 → 输出叶累加器」的 **scatter**。共享可变状态（`canonicalTiles`）**不被并发写**：

- `#pragma omp parallel num_threads(num_threads)`（`drizzle_engine.cpp:1796`），
  `num_threads = config.threads>0 ? config.threads : omp_get_max_threads()`（:1729，无硬编码）；
- 并行单元是 **固定 stripe**（`drizzle_deterministic_stripe_count(height)` :1597，仅依赖 `img.height`），
  由 `std::atomic next_stripe` 认领（:1856）；
- 每 stripe 用**私有 scratch `TileAccumulator` map**（`scratchPool` :1759），累加期无锁、无竞争；
- 归约 `merge_tile_map_into`（:1611）由 `merge_cursor` **按 stripe 索引升序**执行（:1961-1982），
  同一时刻只一个归约写 `canonicalTiles`；
- 线程数注入来自 ThreadLease：`lib/algorithms/drizzle/src/module_entry.cpp:906-921`（`DRZ_OMP_SET`），
  或 scheduler 的 `ScopedOmpWorkerInjection`（`lib/infrastructure/scheduler/src/module_adapters.cpp:258-272`）。

**实测并行度**：`accum_cpu/par_wall = 521.208/33.767 = 15.43×/16 核（96.5% 效率）`；
整帧（含串行写）有效核数 13.86。

### 4.2 逐位可复现保证（科学红线）

现有实现已满足「结果与线程数无关」：
- stripe 划分只由 `img.height` 决定（`drizzle_deterministic_stripe_count` :1597-1607）；
- 每个 stripe 内部按 (y,x) 行主序累加；
- 归约结合树 = stripe 0..n-1 升序左折叠，**与线程调度无关**；
- 线程数只决定 worker 数，不改变 FP 加法顺序（P15a DRIZZLE-DET-001，见 :1574-1592 注释；
  回归测试 `lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh`）。

**风险**：任何后续改动若改变 stripe 划分、stripe 内遍历序或归约顺序，都会改变浮点求和结合树
⇒ 输出字节变化。必须保持「固定 stripe + 升序归约」，或对新的分片方式给出等价的确定性证明与回归。

### 4.3 剩余串行尾与 Amdahl

单帧 37.61s = 重采样 33.77s（已 15.4×）+ HiPS 直写 3.80s（串行）+ 归约 0.98s（串行）。

| 场景 | 单帧 | 单帧加速 | 整跑 3438s |
|---|---|---|---|
| 现状 | 37.61s | 1.000× | 3438s |
| HiPS 写也 16× 并行 | 34.99s | **1.075×** | ~3293s（**1.044×**） |
| 无限核（重采样+写全并行） | 0.98s | 38.3× | — |

> **把 drizzle 进一步并行化的上限是 1.075×（单帧）/ 1.044×（整跑）。**
> 任务书预期的「2076s → 2076/N」没有前提——它已经被 N 除了。

### 4.4 真正的头寸：算法而非并行

若消除/大幅削减候选查询与重叠计算（§3.1 的 C1–C6），单帧可降至 ~4.8s（≈7.8×），
整跑约 3438 − 2076 + 49×4.8 ≈ **1596s（2.15×）**。这些是**算法重构**，必须在
保持 §4.2 逐位可复现的前提下做，并由现有 p1drz 位级回归锁定。

---

## 5. 建议（下一轮，供前台决策）

1. **先修测量**：`run/RELEASE-02/L4-rebuild/stage_cpu.py` 的时间对齐（把 `a=t-t0` 改为
   `a=t-t0-w`），否则后续所有性能路线图都会被 1.21 核误导。参考实现：
   `run/RELEASE-02/perf-drz/stage_cpu_fixed.py`。
2. **不要**把 drizzle 并行化列为优化项——已完成，边际 1.04×。
3. 若要继续提速，按性价比排序做**算法**优化（每项独立提交、位级回归）：
   - C1+C2：`TargetGeomCache` 改 O(1) 触摸 + 容量/访问序调整（预期省重叠 CPU 的显著份额）；
   - C3：候选预过滤去 `pix2ang` 或收紧 δ（预期省候选 CPU 的显著份额）；
   - C4/C5：复用 `filtered` buffer、`tileMap` 换 open-addressing/按 tile 分组；
   - C6：按行复用 drop 几何。
4. 顺带修 C7（profiler 重置），否则多帧 run 的 `[profile]` 不可用。

---

## 6. 复现

```bash
export TMPDIR=/dev/shm/astrocs_drz; mkdir -p "$TMPDIR"

# 1) 复现任务书口径（1.21 核）
python3 run/RELEASE-02/L4-rebuild/stage_cpu.py

# 2) 修正口径（12.82 核）
python3 run/RELEASE-02/perf-drz/stage_cpu_fixed.py

# 3) 单配置前台复测（fine profile + lease trace；输出到 perf-drz/）
ASTROCS_DRIZZLE_FINE_PROFILE=1 ASTROCS_LEASE_TRACE=1 \
  ./build/astrocs normalize --json run/RELEASE-02/perf-drz/cfg_t2_m1_red.json -y --events-jsonl

# 4) 逐像素/逐候选成本与 Amdahl
python3 run/RELEASE-02/perf-drz/profile_breakdown.py
```

**证据文件**

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/perf-drz/stage_cpu_original.out` | 原脚本输出（1.21 核，复现任务书） |
| `run/RELEASE-02/perf-drz/stage_cpu_fixed.out` | 修正后输出（12.82 核） |
| `run/RELEASE-02/perf-drz/evidence_fine_profile.txt` | fine profile / p22 / ops / lease 原始行 |
| `run/RELEASE-02/perf-drz/logs/norm_test.stderr` | 前台复测完整 stderr（含 `[lease]` cap=16） |
| `run/RELEASE-02/perf-drz/profile_breakdown.out` | 每像素/每候选成本 + Amdahl |
| `run/RELEASE-02/perf-drz/stage_cpu_fixed.py` | 修正版对齐脚本 |
| `run/RELEASE-02/perf-drz/profile_breakdown.py` | 成本分解脚本 |
