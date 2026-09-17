# PERF-001 性能计时与热点分析报告（优化前基线）

> 任务：工程控制/RELEASE-01/tasks/PERF-001.md —— 性能计时与热点分析（本轮只做计时基座与热点定位，**不改实现代码**）。
> 权威：ASTROCS_DESIGN.md §8；ACCEPTANCE_SPEC.md §3(L2)；docs/plugins/infrastructure/19_runtime.md、21_observability.md §8、22_gaia_xpsd_client.md；docs/ci/03_GATES.md；contracts/resource_gate_v1.json。
> 证据：run/RELEASE-01/e2e/l3/**（前台真实数据 E2E）、artifacts/acceptance/L2/G-RES-01.json（历史合成 L2）。
> 复现：`python3 run/RELEASE-01/perf/extract_timing.py` → run/RELEASE-01/perf/out/*.csv|json。
> 结论口径：本轮为**基线**，未实施任何优化；所有"是否违约"为按契约语义的**复算**（程序内默认 record_only，最终裁决点为 CI `RESOURCE-GATE-REAL`）。

---

## 0. 结论摘要

- **分段计时**：normalize 由 **drizzle(drz) 主导（均值 56.2%）**，其次 psf 13.1%、snr 11.6%、phot 10.7%、wcs 4.0%、cal 3.7%；mosaic 与 export 无节点级耗时（见 §2.2/§2.3）。
- **最严重 3 个热点**：
  1. **Phase2 mosaic 近乎单线程 + I/O 阻塞**：16 核配额下均值利用率仅 **5.6–6.4%**，`active_compute_threads` p50=1–2，io_wait 最高 **94.47%**；
  2. **Phase1 内存无界增长**：峰值 RSS **4.1–8.6 GB**、稳健斜率 **45.1–77.5 MiB/s**（≥32）、结束回收比例 0.000–0.365（<0.5），peak_commit ≈50 GB；
  3. **Phase1 非 drizzle 节点并行不足**：cal≈1.0 核、phot≈1.9 核、snr≈2.3–4.1 核、psf≈3.0–5.7 核（drz 12.8–13.7 核）。
- **L2 enforce 零违约？** **否**。按 contracts/resource_gate_v1.json 复算：判据③ **4/4** 个 normalize 违约、判据① **3/4** 个 mosaic 违约、判据② 0 个。历史合成 L2（G-RES-01.json）曾 verdict=pass。
- **新增指标记录点**：6 项中 **4 项无记录点**（worker 空转率、上下文切换/块重载、跨 worker 数据搬运量、同组 Gaia 外部请求计数），**2 项部分记录**（缓存命中率、峰值 RSS–块大小关系）。
- **优化建议**：7 条（均**不改变科学结果**，复验方式=1/N worker 一致 + Oracle 对拍），见 §7。

---

## 1. 方法与数据来源

| 证据 | 路径 | 用途 |
|---|---|---|
| 命令日志（含时间戳） | `run/RELEASE-01/e2e/l3/logs/*.log` | normalize 里程碑、Gaia、SDET、drizzle ops、profile 行 |
| DAG 节点实测 | `run/RELEASE-01/e2e/l3/<run>/graph/observed_trace.json` | 节点级 duration_ms / module_id / class |
| 资源时间序列 | `<run>/resource_timeseries.csv` | 逐 0.5 s 采样：cpu_pct、active/runnable、RSS、读写字节、io_wait、active_compute_threads |
| 资源摘要 | `<run>/resource_summary.json` | 阶段 mean/p50/p95/peak/slope |
| 分配报告 | `<run>/alloc_report.json`、`alloc_samples.csv` | RSS 稳健斜率、growth/reclaim 判定、allocator cache |
| 调度均衡 | `<run>/worker_balance.csv` | active vs runnable |
| 批处理墙钟 | `logs/batch.log`、`logs/batch_stage23.log` | 进程级起止（顺序执行） |
| 历史 L2 | `artifacts/acceptance/L2/G-RES-01.json` | 合成 L2 复算对照 |

**机器/配额**：日志 `session run: budget workers=16 (cpus=16)`；全部 10 个 run 的 `granted_workers_peak=16`，故利用率分母=16 核（1600%）。

---

## 2. 分段计时

### 2.1 normalize（Phase1，节点级实测；来源 observed_trace.json）

单位：秒。`trace 合计` = 各节点 duration 之和；`进程墙钟` = batch 启动 → manifest finished_utc。

| run | 帧数 | cal | cos | psf | phot | wcs | **drz** | wr | snr | trace 合计 | 资源 wall | 进程墙钟 | 未监测尾部 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| p1_gc_panel1_red | 4 | 3.31 | 0.65 | 14.47 | 15.78 | 7.09 | **69.67** | 0.002 | 16.58 | 127.5 | 128.0 | 185 | 57.5 (31%) |
| p1_gc_panel2_red | 4 | 2.80 | 0.63 | 15.15 | 18.49 | 4.35 | **69.20** | 0.002 | 17.18 | 127.8 | 128.5 | 193 | 65.2 (34%) |
| p1_m42_t2m1_red | 2 | 2.81 | 0.35 | 5.44 | 3.48 | 2.32 | **37.49** | 0.002 | 4.25 | 56.1 | 56.5 | 72 | 15.9 (22%) |
| p1_m42_t3m1_red | 6 | 4.10 | 1.03 | 15.76 | 7.82 | 2.42 | **39.88** | 0.002 | 10.22 | 81.2 | 81.5 | 124 | 42.8 (35%) |

**占比（均值，4 run）**：

| 节点 | cal | cos | psf | phot | wcs | **drz** | wr | snr |
|---|---|---|---|---|---|---|---|---|
| 均值 | 3.7% | 0.7% | 13.1% | 10.7% | 4.0% | **56.2%** | 0.0% | 11.6% |

- 节点耗时之和与 `resource_summary.wall_seconds` 一致（差 <0.5 s），说明 trace 与资源窗口同源、可信。
- 执行是**严格串行链**（各节点 ended_utc 单调，无重叠），无节点间流水。
- 证据：`out/node_timing.csv`、`out/node_timing.json`。

### 2.2 mosaic（Phase2）

**无 `graph/observed_trace.json`**（p2 目录无 graph/），日志无时间戳，仅有 [hips][profile] 与 sampler 计数 → 只有阶段墙钟与资源读数：

| run | rc | wall(s) | io_wait 均值 | cpu 均值 | cpu p50 | act_thr p50 | 写字节 | 读字节 | RSS 峰值 |
|---|---|---|---|---|---|---|---|---|---|
| p2_gc_wm1 | 0 | 91.5 | **94.47%** | 5.62% | 6.38% | 2 | 7321 MB | 894 MB | 4818 MB |
| p2_gc_wm2 | 2(负例) | 52.0 | 11.12% | 6.30% | 6.38% | 1 | 1933 MB | 581 MB | 1115 MB |
| p2_m42_wm1 | 0 | 22.5 | 47.24% | 6.20% | 6.25% | 1 | 1964 MB | 0.1 MB | 1363 MB |
| p2_m42_wm2 | 2(负例) | 16.0 | 5.56% | 6.44% | 6.25% | 1 | 484 MB | 0.0 MB | 390 MB |

- 日志阶段标记（`p2_gc_wm1.log:2-9`）：`stage coverage ok: cells=476` → `[sampler] enter/cells resized/first tile` **各出现 2 次** → `stage sample ok: obs=4346` → `[hips] finalize` ×4。
- `[hips][profile]`（`p2_gc_wm1.log:14`）为 **per-tile 累加值**：`transform=3.583 fits_write=2.022 hierarchy_accum=3.095 products=0.685 hierarchy_write=2.000 total=2.714`——分项和(11.4 s)>total(2.7 s)，**不是墙钟分段**，仅覆盖 finalize 小段。
- 结论：mosaic 的 91.5 s 墙钟中，**仅 ~2.7 s 有 profile 覆盖**，其余（coverage/sample/integrate/写盘）无阶段计时 → 记录缺口（见 §6）。

### 2.3 export（Phase3，节点级实测）

| run | properties | wcs | **resample2** | writer | verify | trace 合计 | 资源 wall |
|---|---|---|---|---|---|---|---|
| p3_gc | 0.0007 s (0.0%) | 0.0002 s (0.0%) | **2.951 s (65.0%)** | 0.959 s (21.1%) | 0.628 s (13.8%) | 4.54 s | 5.0 s |
| p3_m42 | 0.0022 s (0.1%) | 0.0002 s (0.0%) | **1.733 s (51.6%)** | 1.004 s (29.9%) | 0.616 s (18.4%) | 3.36 s | 3.5 s |

- 两 run 墙钟 3.5/5.0 s **≤10 s** → G-RES-01 判定域外（NOT_APPLICABLE，非豁免）。
- p3 日志仅有 manifest 路径（`p3_gc.log` 单行），无阶段日志；分段来自 trace。

### 2.4 进程墙钟 vs 监测窗口（未记录尾部）

| run | 进程墙钟(s) | 资源/ trace 覆盖(s) | 未覆盖尾部(s) | 尾部占比 |
|---|---|---|---|---|
| p1_gc_panel1_red | 185 | 127.5 | 57.5 | 31% |
| p1_gc_panel2_red | 193 | 127.8 | 65.2 | 34% |
| p1_m42_t2m1_red | 72 | 56.1 | 15.9 | 22% |
| p1_m42_t3m1_red | 124 | 81.2 | 42.8 | 35% |

- 证据：`resource_summary.json` mtime `22:07:22` / `alloc_report.json` `22:07:22` vs `astrocs_run_*.json` `22:08:19`、`graph/*` `22:08:26`；该区间**无任何日志行、无资源样本**。
- 尾部内容未定位（疑为产品收尾/落盘/进程退出），**登记为缺口**（§8）。

---

## 3. 热点定位（含数据依据）

### H1（最严重）Phase2 mosaic 近乎单线程 + I/O 阻塞
- **数据**：`resource_summary.json` 4 个 mosaic run：cpu_mean 89.9–103.1%（=0.90–1.03 核 / 16 核配额 = **5.6–6.4%**），cpu p50 100–102%；`active_compute_threads` p50=1–2；io_wait 5.56–94.47%。
- **判据**：p2_gc_wm2 / p2_m42_wm1 / p2_m42_wm2 的 `active_compute_threads` p50=1 < 2 → **G-RES-01 ① enforce 违约**（复算见 §5）。
- **机理**：write 0.5–7.3 GB、io_wait 最高 94.47% → integrate+写盘串行、I/O 主导；sampler 对同一 tile 做 2 遍（`[sampler] enter` ×2）→ 块重载。
- 证据：`out/gate_recompute.csv`、`p2_gc_wm1.log:1-14`、`<run>/resource_timeseries.csv`。

### H2 Phase1 内存无界增长（判据③）
- **数据**（`alloc_report.json`）：

| run | RSS 峰值 | 稳健斜率 | growth | reclaim | reclaim_frac | peak_commit | peak_cache |
|---|---|---|---|---|---|---|---|
| p1_gc_panel1_red | 6.78 GB | 45.1 MiB/s | unbounded | reclaimed | 0.161 | 49.65 GB | 46.22 GB |
| p1_gc_panel2_red | 8.56 GB | 61.6 MiB/s | unbounded | reclaimed | 0.365 | 51.39 GB | 46.21 GB |
| p1_m42_t2m1_red | 4.13 GB | 69.5 MiB/s | unbounded | unexplained_residual | 0.000 | 49.58 GB | 47.55 GB |
| p1_m42_t3m1_red | 5.66 GB | 77.5 MiB/s | unbounded | unexplained_residual | 0.000 | 50.92 GB | 47.53 GB |

- **判据**：斜率 ≥32 MiB/s 且回收不足（`allocation_reclaim_min_fraction=0.5`）→ **③ enforce 违约 4/4**；程序内亦已告警：`p1_gc_panel1_red.log` 末行 `resource gate recorded (not enforced): alloc_growth_unbounded ... 45.102377 MB/s >= 失败线 32.000000 MB/s`。
- **流式性反证**：输入仅 2–6 帧（每帧 4096²/4500×3600 ×4B ≈ 67 MB），峰值 RSS 却达 4.1–8.6 GB；`alloc_samples.csv` 显示 outstanding 从 0.1 MB 升至 3.44 GB（t≈96 s）后回落到 23 MB，但 RSS 仅从 6.5 GB 降到 5.69 GB（allocator 保留 1.68 GB）→ 非流式。
- 证据：`<run>/alloc_report.json`、`<run>/alloc_samples.csv`、`p1_gc_panel1_red.log` 末行。

### H3 Phase1 非 drizzle 节点并行不足（单线程长计算）
按节点窗口对齐 `resource_timeseries.csv` 的实测 CPU（p1_gc_panel1_red）：

| 节点 | 窗口(s) | cpu 均值 | 等效核 | act_thr 均值/峰 |
|---|---|---|---|---|
| cal | 0.0–3.3 | 103.2% | **1.03** | 6.5 / 16 |
| cos | 3.7–4.3 | 190.7% | 1.91 | 16.0 / 16 |
| psf | 4.8–19.3 | 298.3% | **2.98** | 9.9 / 17 |
| phot | 18.5–34.3 | 196.7% | **1.97** | 5.2 / 17 |
| wcs | 34.2–41.3 | 338.9% | 3.39 | 6.8 / 17 |
| **drz** | 41.6–111.3 | 1281.2% | **12.81** | 15.0 / 18 |
| snr | 111.7–128.3 | 231.0% | **2.31** | 4.6 / 9 |

- p1_m42_t2m1/t3m1 同型：phot 1.82/1.95 核、snr 3.38/4.12 核、psf 5.24/5.69 核、drz 13.69/13.27 核。
- 结论：**仅 drz 接近满载（12.8–13.7/16 核，80–86%）**；cal/phot/snr/psf 明显单线程化，是 normalize 均值利用率仅 51–65% 的主因。
- DPSF 单线程证据：`lib/algorithms/psf/src/dpsf_psf.cpp:387/397/405` 告警在 psf 窗口内以 288–947 条/s 聚集（`p1_gc_panel1_red.log` 22:05:19–22:05:28）。

### H4 Phase1 重复帧读写 / 块重载
- **数据**（日志计数）：p1_gc_panel1 `FITS Reading=31`（header-only 5）、`Write OK=8`；t3m1 `Reading=45`（header-only 7）、`Write=12`。同一帧被读 2–3 次（calibration → cosmetic → 再读 cleaned），中间产品落盘后再读回。
- **数据**：mosaic sampler 对同一 tile 2 遍（`[sampler] enter` ×2，4/4 run）。
- 依据：19_runtime.md §4.2（同一块连续节点一次走完、避免反复重载）。
- 证据：`out` 脚本计数、`p1_gc_panel1_red.log:19/57/63/68`、`p2_*.log`。

### H5 drizzle 几何缓存命中率偏低 + 候选效率
- `[drizzle_engine][ops]`（4 run）：`gcache_hit/(hit+miss)` = **0.539–0.644**；`cand_eff` 0.42–0.475（候选→有效仅 42–48%）；`sh_frac` 0.633–0.656。
- 例（p1_gc_panel1）：`cand=352,287,604 true_ov=167,214,123 quick_rej=185,073,481 gcache_hit=189,958,025 gcache_miss=162,329,579`。
- 意义：drz 占 56.2% 墙钟，缓存命中率提升与候选裁剪可显著缩短该节点。
- 证据：`p1_gc_panel1_red.log:4371` 附近（out/normalize_segments.json::drizzle_ops）。

### H6 未监测尾部（31–35% 进程墙钟）
见 §2.4。无日志、无资源样本、无节点记录。

---

## 4. 资源分析先行（预估值 vs 实测）

| 待优化模块 | 峰值工作集估算 | 缓存复用点 | 调度顺序与切换次数（预估） | 实测对照 |
|---|---|---|---|---|
| Phase1 drz | 单帧 4500×3600×4B=64.8 MB；drizzle 候选/目标数组按 `ops` 计数 ×8B 估：cand≈2.8 GB、sh≈2.8 GB、tgt≈1.3 GB → 数 GB 级 | target-ipix 几何缓存（gcache） | 每 run 1 个 drz 节点、无切换 | RSS 峰值 6.78–8.56 GB；gcache 命中 53.9–64.4% |
| Phase1 psf/phot/snr | 单帧 67–65 MB + 星表/PSF 模型 | Gaia 结果、PSF 模型、母版 | 串行 3 节点/run | 实测 1.9–5.7 核、psf 窗口 DPSF 告警密集 |
| Phase2 mosaic | 单 tile 512²；n_union=476 cells、cells resized=30464 | 采样 tile 缓存（cap/resident 未落盘） | coverage→sample(2 遍)→integrate→write，2 遍重载 | util 5.6–6.4%、io_wait≤94.47%、write≤7.3 GB |
| Phase3 resample2 | 2048×2048×2 planes×4B≈33.6 MB | tile_cache（hits/misses 已算未落盘） | properties→wcs→resample2→writer→verify | resample2 占 51.6–65.0% |

**缓存复用点现状**：drizzle gcache 有计数（仅日志）；p3 sampler `tile_cache` 在 `lib/infrastructure/scheduler/src/module_adapters.cpp:6216` 写入内存 manifest，但 **L3 p3 输出无该字段落盘**（`p3_writer.json`/`p3_verify.json`/`p3_resampled.json` 均无 cache 键）；Gaia 客户端**无任何缓存命中计数**。

---

## 5. L2 指标复算（G-RES-01，阈值源 contracts/resource_gate_v1.json）

分母：`granted_workers_peak=16`（`allocated_source=granted_workers`），利用率= cpu_pct/(16×100)。

| run | wall(s) | util 均值 | util p50 | 逐样本≥85% 占比 | act_thr p50 | 最长<60% 窗(s) | 队列有工作 | io_wait | **enforce ①②③** | record ④⑤⑥ |
|---|---|---|---|---|---|---|---|---|---|---|
| p1_gc_panel1_red | 128.0 | 0.512 | 0.613 | 0.391 | 17 | 25.0 | 否 | 9.30% | **③** | ④⑤⑥ |
| p1_gc_panel2_red | 128.5 | 0.523 | 0.620 | 0.448 | 17 | 32.5 | 否 | 6.86% | **③** | ④⑤⑥ |
| p1_m42_t2m1_red | 56.5 | 0.651 | 0.926 | 0.584 | 17 | 0.0 | 否 | 2.18% | **③** | ④⑥ |
| p1_m42_t3m1_red | 81.5 | 0.541 | 0.611 | 0.436 | 17 | 13.5 | 否 | 6.93% | **③** | ④⑤⑥ |
| p2_gc_wm1 | 91.5 | 0.056 | 0.064 | 0.000 | 2 | 91.5 | 否 | 94.47% | — | ④⑤⑥ |
| p2_gc_wm2 | 52.0 | 0.063 | 0.064 | 0.000 | **1** | 52.0 | 否 | 11.12% | **①** | ④⑤⑥ |
| p2_m42_wm1 | 22.5 | 0.062 | 0.063 | 0.000 | **1** | 22.5 | 否 | 47.24% | **①** | ④⑤⑥ |
| p2_m42_wm2 | 16.0 | 0.064 | 0.063 | 0.000 | **1** | 16.0 | 否 | 5.56% | **①** | ④⑤⑥ |
| p3_gc | 5.0 | 0.209 | 0.135 | 0.000 | 7.5 | 0.0 | 否 | 3.20% | NOT_APPLICABLE(≤10s) | N/A |
| p3_m42 | 3.5 | 0.337 | 0.063 | 0.286 | 8 | 0.0 | 否 | 10.86% | NOT_APPLICABLE(≤10s) | N/A |

**判据对照（契约）**：① 单活跃计算线程 p50<2 → **违约 3 个**；② 连续 ≥10 s 窗 <60% 且就绪线程中位数>16 且 ≥2 → **0 个**（runnable 恒 16，无队列证据）；③ 稳健斜率 ≥32 MiB/s 且回收不足 → **违约 4 个**；④ 均值<85% → 8/8 域内记录项超标；⑤ p50<90% → 7/8 超标；⑥ 逐样本通过率<0.70 → 8/8 超标。

**当前是否零 enforce 违约：否**（①×3 + ③×4）。程序内默认 `record_only` 故 L3 命令 rc=0；最终裁决须由 CI `RESOURCE-GATE-REAL`（`run_monitored.py --gate-required --gate-workers 16`）复跑。

**历史对照**：`artifacts/acceptance/L2/G-RES-01.json` verdict=pass，violations=[]；recorded 仅 `frozen_avg_utilization_low 0.814` 与 `low_utilization_window_no_queue 15.1s`；active_threads_stat=64（p50）。即合成 L2 与真实 L3 结论不同，L3 暴露 ①②③ 违约。

---

## 6. 新增指标记录点现状

| # | 指标（19_runtime §3/§4.2、ACCEPTANCE_SPEC L2） | 是否有记录点 | 证据 |
|---|---|---|---|
| 1 | **worker 空转率** | **无** | `resource_timeseries.csv` 20 列无 idle；`worker_balance.csv` 的 `utilization_pct=active/(active+runnable)×100`（resource_recorder.h:321-326），active=runnable=16 恒为 **50.00**（p1_gc_panel1 256 行、t3m1 163 行、p2_gc_wm1 183 行、p3_gc 10 行全部 50.00）→ 非真实空转率 |
| 2 | **上下文切换 / 块重载次数** | **无** | `lib/infrastructure/cli/monitor.h:75` 采集 `ctx_switches`（voluntary+nonvoluntary），但 CSV 头（resource_recorder.h:262-266）20 列**不含** ctx_switches → 采样后被丢弃；块重载无任何计数器（仅可从日志计数：FITS_reads 17–45、sampler 2 遍） |
| 3 | **跨 worker 数据搬运量** | **无** | `grep -rniE 'steal_count|migrat|data_movement|搬运' lib/infrastructure/scheduler lib/infrastructure/pipeline` 无命中；仅 ACR（非生产）有 `enable_work_stealing` 开关，无搬运量统计 |
| 4 | **缓存命中率** | **部分** | drizzle gcache 命中率仅日志（`[drizzle_engine][ops]`，0.539–0.644）；p3 sampler `tile_cache`（module_adapters.cpp:6216）**未落盘**到任何 p3 产物；Gaia 客户端 `grep cache_hit/miss` 无命中；无 L2 要求的"缓存命中率统计归档" |
| 5 | **同组 Gaia 外部请求计数** | **无有效记录点** | 仅 `ipv_select.cpp:1021` 输出每次解算的 `gaia_calls=2–4`（p1 日志各 1 行），非 run 级/同组计数；无 in-flight 合并、无 cache hit/miss、无来源层；`contracts/schemas/catalog_query.schema.json`（22 文档引用）**不存在** |
| 6 | **峰值 RSS 随块大小与总数据量关系** | **部分** | `alloc_report.peak_rss_bytes`、`resource_summary.rss_peak_bytes` 有；但配置**无 `block`/`memory_limit` 字段**（l3/configs/*.json），无块大小记录、无"成倍增大帧数"对照实验；实测 RSS 4.1–8.6 GB 与帧数（2/4/6）非单调 → 无法验证流式性 |

**缺失记录点数量：4 项完全缺失（#1、#2、#3、#5），2 项部分缺失（#4、#6）。**

---

## 7. 优化建议清单（不实施；按收益/风险排序）

> 全部建议**不改变科学结果**（科学依赖/归约次序不变）。复验统一含：1/N worker 数值一致（tests/backend 既有覆盖）、Oracle 对拍、受影响产品 canonical sha256 不变。

| # | 建议 | 目标热点 | 预期收益 | 风险 | 改科学结果 | 复验方式 |
|---|---|---|---|---|---|---|
| O1 | Phase2 integrate/write 按 HEALPix tile/cell 多线程 + 块间流水，消除 sampler 2 遍重载 | H1 | **极高**（利用率 6%→接近满载，io_wait↓） | 中（确定性归约/写序） | 否 | 1/N worker 一致 + p2 产品哈希 + ① 归零 |
| O2 | Phase1 流式内存：按帧/块处理、用完即释、显式 LRU 字节预算；抑制 allocator 保留（peak_cache 46 GB） | H2 | **高**（③ 归零、峰值 RSS↓） | 中 | 否 | 峰值 RSS 不随帧数线性增长 + ③ 归零 + 1/N 一致 |
| O3 | Phase1 cal/phot/snr/psf 逐帧/逐像素循环多线程化（现 1.0–5.7 核） | H3 | **高**（normalize 均值利用率 51–65%→80%+） | 低-中 | 否 | 1/N worker 一致 + Oracle 对拍 |
| O4 | 消除重复帧读写：cal→cos→psf→phot→wcs 一块一次走完，减少中间落盘/重载 | H4 | 中 | 低 | 否 | 产品 canonical sha256 不变 |
| O5 | 观测补齐：ctx_switches 落盘、worker 空转率/块重载/跨 worker 搬运量计数、worker_balance 真实利用率、manifest started_utc 修正、监测覆盖全进程 | H6/§6 | 中（使后续优化可度量） | **极低** | 否 | 字段存在且非恒定；重跑对照 |
| O6 | Gaia 单例客户端 + 瓦片键两级缓存 + in-flight 合并，落盘同组外部请求计数与命中率 | §6#4/#5 | 中 | 低 | 否 | 同组请求计数=1、命中与网络结果一致、WCS 数值一致 |
| O7 | drizzle 几何缓存键/预取优化，提升 gcache 命中率（现 0.539–0.644）与 cand_eff（0.42–0.475） | H5 | 中低 | 低 | 否 | 产品 canonical sha256 不变 + 命中率↑ |

---

## 8. 未决项 / 上呈

| ID | 问题 | 证据 | 处置建议 |
|---|---|---|---|
| U1 | 进程墙钟 22–35% 未监测、无日志（尾部 15.9–65.2 s） | §2.4；resource/alloc mtime 22:07:22 vs manifest 22:08:19、graph 22:08:26 | 插桩定位尾部内容；扩展监测窗口至进程退出 |
| U2 | `astrocs_run_*.json` 的 `started_utc == finished_utc`（写入时取 now，非运行起点） | `commands.cpp:241-242`；10/10 manifest 两值相同 | 修正为真实起点（不影响科学） |
| U3 | Phase2 mosaic 无 `graph/observed_trace.json`，无节点级计时 | p2_* 目录无 graph/ | 补 DAG trace 落盘 |
| U4 | Phase3 无阶段日志，仅 trace；export 判定域外（≤10 s） | `p3_gc.log` 单行 | 补阶段日志；如需 L2 覆盖需放大合成负载 |
| U5 | 两条 RSS 斜率口径不一致：alloc_report 45.1 MiB/s vs resource_summary 42.5 MiB/s | `<run>/alloc_report.json` vs `resource_summary.json` | 统一斜率算法并说明 |
| U6 | 日志/告警标签 "MB/s" 与契约禁用口径冲突（值实为 MiB/s） | `p1_gc_panel1_red.log` 末行 "45.102377 MB/s" vs contract.memory.unit | 标签改为 MiB/s |
| U7 | `contracts/schemas/catalog_query.schema.json` 被 22 文档引用但不存在 | `find contracts -iname '*catalog*'` 无命中 | 补 schema 或订正文档引用 |
| U8 | enforce ①②③ 复算未走 CI 裁决点（本轮不跑重型运行） | §5 说明 | 由 CI `RESOURCE-GATE-REAL` 复跑裁决 |
| U9 | L4 全量（15 作业/81 帧）后台运行中，未纳入本轮 | `run/RELEASE-01/e2e/l4/logs/batch.log`（3/15 完成） | L4 完成后纳入对比 |
| U10 | p2 weight_mode=2 两个 rc=2（非性能问题） | `batch_stage23.log`、`p2_gc_wm2.log` | 归 PRE-F-01 链路问题，不在本任务范围 |

---

*本报告只读分析，未修改任何实现代码/配置；脚本与中间数据在 run/RELEASE-01/perf/。*
