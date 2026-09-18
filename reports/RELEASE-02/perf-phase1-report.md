# RELEASE-02 Phase1 性能剖析报告（**已更正**）

> **重要更正**：本报告初版称「drizzle 仅 1.21 核（单线程）」，该结论**错误**，
> 系测量脚本时间区间对齐缺陷所致（探针阶段事件的 `ts_utc` 是**结束**时刻，初版当成开始时刻，
> 使 drizzle 被采样到阶段结束后的空闲尾段并按 cpu=0 加权）。由 PERF-DRZ 分片独立发现并给出反证。
> 脚本已修正（`tools/l4_rebuild/stage_cpu.py`，`a = t - t0 - w`）。**本版数字为准。**

数据来源：L4 重建 49 帧 R 通道真实运行（探针 ON），12 个配置。
方法：探针 JSONL（逐阶段 `wall_us` + **END 时刻** `ts_utc`）× 各配置 `resource_timeseries.csv`（`cpu_pct`，单位=单核百分比）按时间区间对齐。
复现：`python3 tools/l4_rebuild/stage_cpu.py`

## 结论：并行度不是问题，**算法总工作量**才是

| 阶段 | 总耗时(s) | 占已计 | **平均核数** |
|---|---|---|---|
| **drizzle** | **2076.2** | **75.4%** | **12.82** |
| star_psf | 175.9 | 6.4% | 5.83 |
| noise | 172.0 | 6.2% | 3.62 |
| photometry | 159.1 | 5.8% | 2.02 |
| wcs | 115.0 | 4.2% | 5.35 |
| calibrate | 46.4 | 1.7% | 1.06 |
| cosmetic | 8.1 | 0.3% | 2.09 |

整跑墙钟 3438s。drizzle 以 12.82 核跑 2076s ⇒ **CPU 工作量约 26,614 核·秒**。

## drizzle 内部分解（PERF-DRZ 细粒度剖析，单帧实测）
- 重采样 33.77s 墙钟 = 2.013 µs/像素（**墙钟且已 16 核摊薄**）；**单核约 31.6 µs/像素**；
- 并行度 `accum_cpu/par_wall = 521.2/33.77 = 15.43×/16`（**96.5%**）；
- 每像素 CPU：**候选查询 15.14 µs（47.9%）+ 球面重叠 14.55 µs（46.1%）**+ drop 几何/累加 0.79 µs + WCS 0.49 µs；
- 每帧 231.57M 候选（57.9% quick-reject、42.1% 真重叠）；几何缓存**命中率仅 64.2%**；
- 串行尾：HiPS 直写 3.80s + 归约 0.98s = 4.78s/帧（12.7%）。

## 复杂度问题（优化头寸，按性价比）
| 编号 | 问题 | 证据 |
|---|---|---|
| C1 | TargetGeomCache 命中路径 **O(capacity=8192) LRU 线性扫描**（唯一准超线性） | `spherical_overlap.cpp:1389-1395`，1.487 亿次命中 |
| C2 | 缓存抖动：容量 8192 << 每 stripe 工作集约 9.3 万叶 ⇒ **35.8% miss** | 8285 万次 pix2radec+边界重建 |
| C3 | 候选过度枚举：49 格/像素仅保留 13.8（28%），被丢格仍付 pix2ang+三角 | 约 8.22 亿次/帧 |
| C4 | 每源像素一次 `std::sort` + 局部 filtered vector 堆分配 | 1678 万次/帧 |
| C5 | 每真重叠一次 `tileMap[parent]` unordered_map 查找 | 9749 万次，`drizzle_engine.cpp:1549` |
| C6 | drop 几何逐像素重建 | 1678 万次，`:1457` |
| C7 | profiler 计数 `g_tl_prof_cand/overlap` 从不重置 ⇒ 第 2 帧起为累计值 | `:317-318` |

主成本循环：`drizzle_engine.cpp:1512` `for (uint64_t ipix : candidates)`（2.316 亿次/帧）。
（`:118` 是诊断 trace 回退，仅 `ASTROCS_DRIZZLE_TRACE` 且无 selection 文件时执行，L4 未启用。）

## 并行化空间（已接近上限，不作为优化项）
- 现实现：固定 stripe（`drizzle_deterministic_stripe_count:1597` 仅依赖 height）+ 原子认领（`:1856`）
  + 每 stripe 私有 scratch（`:1759`）+ 按 stripe 索引升序归约（`:1611`，`merge_cursor:1961`）
  ⇒ **无累加器竞争、逐位可复现且与线程数无关**（P15a DRIZZLE-DET-001）；
- 线程数来自 ThreadLease（无硬编码）；
- **把 HiPS 直写也并行化，上限仅 1.075×（单帧）/ 1.044×（整跑）** ⇒ 不值得。

## 优化方向（算法，非调度）
1. **C1 + C2**（最高性价比）：缓存改 O(1) 触摸 + 调整容量/访问序；
2. **C3**：候选预过滤去 pix2ang / 收紧 delta；
3. C4/C5；4. C6；5. 顺带修 C7。

**潜在收益**：消除 C1–C6 单帧约 4.8s（约 7.8×），整跑约 1596s（**约 2.15×**）。

## 科学红线（必须遵守）
任何改动若改变 **stripe 划分 / stripe 内遍历序 / 归约顺序**，都会改变 FP 求和结合树 ⇒ 输出字节变化。
必须保持「固定 stripe + 升序归约」或给出等价确定性证明 + `tests/p1drz/p1drz_taskset_invariance.sh` 位级回归。
