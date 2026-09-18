# RELEASE-02 · Phase2 剩余瓶颈排查（PERF-P2B）

- 任务：P2-IMPL 三阶段并行化后实测加速比仅 **2.03×**（1327.9s → 654.5s），远低于预期 4.70×。
  本轮**只分析 + 只读测量**，量化锁争用、定位真实瓶颈、判断并行是否生效、给出 S3 方案与下一步优先级。
- 范围：**未改任何生产代码/文档**；零 git 写权限；未跑 `ninja`/`cmake`/`ctest`；只跑 `build/astrocs mosaic`。
- 工作目录：`/workspace/Astro CS Database`；证据/脚本：`run/RELEASE-02/perf-p2b/`；
  产物：`run/RELEASE-02/perf-p2b/out/probe16w{,_lock,_lock2}/`（未触碰 `bitref_16w` / `mosaic_out_w1`）。

---

## 0. 结论摘要（TL;DR）

> **654.5s 不是被 CPU 计算吃掉的，而是被「每次 HiPS tile 读都要拿进程级 cfitsio 全局锁」串行化的
> 读盘地板（serial read floor）撑住的。** 三阶段线程确实跑在 16 个线程上（`thread_id` 16 值），
> 但有效核数只有 **upm_apply 7.8 / reject 2.5 / integrate 0.75**，因为其余线程全在
> `aio::cfitsio_io_mutex()` 上排队。每个阶段的墙钟 ≈（该阶段被锁保护的 tile 读次数）× 单次读成本 **7–15 ms**
> （sample 的读偏热、约 7ms；upm_apply/reject/integrate 偏冷、12–15ms）。

| 问题 | 结论 |
|---|---|
| 654.5s 花在哪 | **sample 274s(42%)** + upm_apply 145s(22%) + integrate 102s(16%) + reject 59s(9%) + write 27s(4%) + hash 尾部 43s(6.5%) + upm_fit 4s |
| 三阶段核数 | upm_apply **7.79**、reject **2.51**、integrate **0.75**（探针实测，16 线程在跑但被锁阻塞） |
| 锁争用占比 | 全部互斥等待 **2791.6 线程·秒**；其中 **cfitsio 全局锁 = 90.3%**（`read_tile_t` 73.0% + `aio_hips_open`/MOC 17.3%），sampler 自己的 `g_aio_mu` 9.7% |
| 并行化是否生效 | **生效（线程维度）但不生效（墙钟）**：`aio_hips_read_tile_f32` 在 16 线程下 **0.93×**（≈完全串行）；raw `pread` 同数据可 **3.0×**（冷）/ 2.7×（热单线程） |
| S3 | query/fill 去重（零科学风险）≈ 省 **120–137s（~20%）**；pass2/3 并行 ≈ 省 ≤20s |
| 最高性价比下一步 | **S3.1 sample query/fill 去重**（先做，零风险）；其次**锁无关 tile 读路径**（天花板最高，需位级回归） |

**两个必须先说明的混淆因子**（否则 2.03× 这个数字会被误读）：

1. **`sky_plane` gauge 修复改变了工作量**：旧基线 `mosaic_out_w1` 的 `p2_corrected.json.sky_plane_applied=false`
   （`[sky_plane] build FAILED rc=6` 回退 UPM C 场）；新构建 `sky_plane_applied=true`。同一 1 worker 下
   upm_apply **348s → 792s（+444s）**。同构建同口径的加速比是 **1568.7s → 654.5s = 2.40×**，不是 2.03×。
2. **page cache**：sample 冷输入 274s vs 热输入 133–158s（同一构建、同一配置）。654.5s 是冷输入口径。

---

## 1. 测量方法与口径

### 1.1 探针 `ts_utc` = END 时刻（沿用 PERF-DRZ 修正口径）

`tools/l4_rebuild/stage_cpu.py` 记录：阶段级事件的 `ts_utc` 是**结束**时刻，`wall_us` 是耗时；
正确窗口是 `[t_end − wall, t_end]`。本报告脚本 `run/RELEASE-02/perf-p2b/analyze_p2_stages.py`
按此口径把探针阶段窗口与 `resource_timeseries.csv`（`cpu_pct` 单位 = 1 核的百分比）对齐，
对齐点 `T0 = min(t − wall)`（与 series 的 `elapsed_seconds=0` 一致，实测误差 <0.2s）。

### 1.2 与产物 mtime 独立交叉验证

`run/RELEASE-02/perf-p2b/stage_from_mtimes.py` 用产物 mtime 反推逐阶段墙钟（完全不依赖探针）：

| run | sample | upm_fit | upm_apply | reject | integrate | finalize | hash 尾 | total |
|---|---|---|---|---|---|---|---|---|
| OLD 1w `mosaic_out_w1`（旧构建，sky_plane 回退） | 159.9 | 3.9 | **348.3** | 213.9 | 138.6 | 23.6 | 439.5 | **1327.8** |
| NEW 1w `bitref_1w`（新构建） | 257.8 | 4.5 | **792.2** | 192.4 | 103.8 | 18.2 | 199.8 | **1568.7** |
| NEW 16w `bitref_16w`（官方位级回归） | 274.2 | 4.0 | 145.4 | 58.8 | 102.4 | 27.0 | 42.7 | **654.5** |
| NEW 16w `probe16w`（本轮探针+shim） | 266.4 | 4.4 | 155.3 | 54.4 | 81.5 | 59.4 | 50.4 | 671.7 |
| NEW 16w `probe16w_lock2`（热缓存） | 157.9 | 4.1 | 161.0 | 67.7 | 85.7 | 37.5 | 53.8 | 571.8 |

（`probe16w` 的 finalize 59.4s 是 mutex shim 观测开销放大；`bitref_16w` 的 27.0s 更干净。）

### 1.3 锁争用量化：LD_PRELOAD 互斥探针（不改行为）

`run/RELEASE-02/perf-p2b/mutex_probe.c` 编译为 `libmutexprobe.so`，interpose `pthread_mutex_lock`：
先 `pthread_mutex_trylock` 走快路径（不记录），失败才 `clock_gettime` 计时并记录
`(调用点返回地址, 墙钟秒, 次数, 等待 ns)`，析构时按 **PID 后缀**落盘。
它不改变任何语义，只给**被阻塞**的加锁事件加两次 `clock_gettime`。
调用点用 `addr2line -e build/astrocs` 解析（Release 无行号，函数名足以定位）。
实测开销：WALL 671.8s（shim on）vs 654.5s（bitref_16w，无 shim）= **+2.6%**。

> 注：`libmutexprobe.so` 只对 `build/astrocs` 进程生效（`env LD_PRELOAD=...`，不经 `/usr/bin/time`），
> 避免父进程析构覆盖日志——这是本轮踩过的坑，已在脚本里修正。

### 1.4 读路径微基准（绕过 cfitsio 锁）

- `aio_read_bench2.cpp`：直接用 `aio_hips_open`/`aio_hips_read_tile_f32`（走全局锁）做 1 vs N 线程读。
- `pread_concurrency.py` / `pread_cold49.py`：直接 `os.pread` 读同一批 `.fits`（**完全绕过 cfitsio 与全局锁**），
  冷数据用 `posix_fadvise(DONTNEED)` 丢弃。

---

## 2. 654.5s 花在哪：新构建逐阶段 wall 与核数

**新构建 16 worker，探针实测**（`run/RELEASE-02/perf-p2b/new_stages.txt`，
`probe16w`；括号内为 `bitref_16w` mtime 口径）：

| 阶段 | wall_s | 有效核数 (cpu_pct/100) | active_compute_threads 均 | 该阶段 tile 读次数 | 备注 |
|---|---|---|---|---|---|
| coverage | 0.04 | – | – | – | |
| **sample** | **267.3** (274.2) | **0.51** | 3.23 | ~22436（2 遍扫描） | query+fill 各扫一遍，读全被锁串行 |
| upm_fit | 8.4 (4.0) | 1.13 | 2.00 | – | sky_plane.build 4.6s |
| **upm_apply** | **150.3** (145.4) | **7.79** | 12.17 | 11218（signal+support） | 计算重（sky plane），唯一接近并行 |
| **reject** | **54.6** (58.8) | **2.51** | 10.39 | 5609（support） | 线程在跑、CPU 很低 ⇒ 等锁 |
| **integrate** | **81.8** (102.4) | **0.75** | 7.46 | 5609（support） | 与 1 worker（103.8s）几乎相同 ⇒ **零加速** |
| write | 58.9* (27.0) | 0.30* | 1.38 | 523×2 写 | *shim 放大；单线程写 |
| **TOTAL(监测窗)** | **625.9** (611.7) | 2.47~2.74 均值 | | | |
| hash 尾部（监测窗外） | 50.4 (42.7) | ~6 | | | S2 并行哈希，已从 439.5s 降下来 |

逐阶段「计算地板 vs 读地板」：

| 阶段 | 计算量 core·s | 理想@16 wall | 锁读次数 | 单次读成本 (wall/N_reads) | 实际 wall | 受限方 |
|---|---|---|---|---|---|---|
| sample | ~68 | 4.3s | ~22436（2 遍） | 7.0 ms（读偏热） | 158–274s | **磁盘带宽 + 锁** |
| upm_apply | ~1180 | 73s | 11218 | 14.3 ms | 150s | **读（锁）+ 计算** |
| reject | ~137 | 8.6s | 5609 | 12.1 ms | 55–68s | **读（锁）** |
| integrate | ~62 | 3.9s | 5609 | 15.3 ms | 82–102s | **读（锁）** |

⇒ **四个阶段全部被「串行化的读路径」压住**：sample 是**冷盘带宽 + 锁**（单线程 raw 冷读 167MB/s，
并发可达 508MB/s），其余三个是**锁护航**（互斥等待 763–924 线程·秒/阶段）。
并行只把计算部分藏进了读路径的空隙里；计算本身若不受限，四阶段 16 核下只需 ~90s。

---

## 3. 锁争用量化（核心证据）

### 3.1 互斥等待归因（`probe16w_lock2`，WALL 571.8s）

`run/RELEASE-02/perf-p2b/mutex_attribution.txt`：

| 调用点 | 等待总时长 | 争用次数 | 平均等待 | 占比 |
|---|---|---|---|---|
| `read_tile_t<float>` @ **aio_hips_reader.cpp:126**（cfitsio 全局锁） | **2036.4 s** | 7830 | 260 ms | **73.0%** |
| `aio_hips_open`（MOC 读，**aio_hips_reader.cpp:216** 同一把锁） | **483.8 s** | 1521 | 318 ms | **17.3%** |
| sampler `g_aio_mu`（**sampler.cpp:169/174**，open 时串行化） | 271.3 s | 1103 | 238–386 ms | 9.7% |
| probe sink / scheduler | ~0.03 s | 7 | – | ~0% |
| **合计** | **2791.6 线程·秒** | 10461 / 336378 次加锁 | | 100% |

⇒ **cfitsio 全局锁 = 2520.2 线程·秒 = 全部互斥等待的 90.3%**。

按阶段切分（同一次运行的探针窗口）：

| 阶段 | wall | 锁等待（线程·秒） |
|---|---|---|
| sample | 157.9 s | 271.3 |
| upm_apply | 161.0 s | 833.1 |
| reject | 67.8 s | 763.2 |
| integrate | 85.7 s | 924.0 |
| write | 37.5 s | 0（单线程，无争用） |

`read_tile_t` 的锁临界区**覆盖整个 `fits_open_file` → `fits_get_img_param` → `fits_read_pix` → `fits_close_file`**
（`aio_hips_reader.cpp:126-168`），即连 1MB 的数据拷贝和读盘都在锁内。
`aio_fits.cpp:525-529` 的 RT-008 注释明确：cfitsio 全局表在并行访问下非线程安全，故**必须**进程级串行化。
平均等待 260ms ≫ 单次临界区长度，说明存在**锁护航（convoy）**：16 个线程反复醒来/被抢占，
等待时间被放大到临界区的十几倍——这部分等待同时也在烧 CPU（`USER` 从 1w 的 940s 涨到 16w 的 1815s）。

### 3.2 读路径微基准：锁 = 零并行

`run/RELEASE-02/perf-p2b/aio_bench_hot.txt`（`aio_hips_read_tile_f32`，热缓存、先 warmup）：

| 产品 | 1 线程 | 16 线程 | 并行比 |
|---|---|---|---|
| signal | 0.320 ms/read（3.1 GB/s） | 0.343 ms/read | **0.93×** |
| support | 0.306 ms/read | 0.333 ms/read | **0.92×** |
| （冷 fadvise）signal | 1.307 ms/read | 1.369 ms/read | **0.95×** |

⇒ 只要走 `aio_hips_read_tile_f32`，**16 线程不比 1 线程快，反而略慢**。锁把 tile 读彻底串行化。

### 3.3 绕过 cfitsio 直接 `pread`：磁盘能并行

`run/RELEASE-02/perf-p2b/pread_cold49.out`（全 49 帧 signal 共 8946 tiles / 9.3 GB，`fadvise DONTNEED` 后冷读）：

| 线程 | wall | ms/read | 吞吐 |
|---|---|---|---|
| 1 | 55.86 s | 6.244 | **167 MB/s** |
| 16 | 18.37 s | 2.053 | **508 MB/s（3.0×）** |

热缓存下 raw `pread`：1 线程 0.117 ms/read（8.5 GB/s），16 线程 0.098 ms/read（10.2 GB/s）。

⇒ **磁盘本身可以并发（冷读 3.0×）**；当前 90% 的互斥等待是**锁强加**的，不是磁盘能力上限。
同时 cfitsio 单次读比 raw `pread` 慢约 2.7×（0.320 vs 0.117 ms，热），即 open/header 解析/close 的固定开销也不小。

---

## 4. 并行化是否真的生效？

**生效（线程维度）**：探针逐 tile 事件的 `thread_id` 已从 P2-IMPL 前的单值变为 16 值：

- `sky_plane.apply.tile` 5609 条：16 个 tid（2–18）
- `reject.tile` 523 条：16 个 tid（18–34）
- `integrate.tile` 523 条：16 个 tid（34–50）

**不生效（墙钟维度）**：有效核数 upm_apply 7.79 / reject 2.51 / integrate 0.75。
原因不是「没活干」，也不是「并行没生效」，而是**活被一把全局锁串起来**：
三阶段每个 tile 都要读 FITS，而 `read_tile_t` 对每次读都取进程级 `cfitsio_io_mutex`。
`active_compute_threads`（区间内有 CPU 增量的线程数）在 reject 是 10.4、integrate 7.5，
但 `cpu_pct` 只有 2.5/0.75 核 ⇒ 线程在「醒来→拿不到锁→睡下」之间空转。

`plan().work_units` 仍为 1（`module_adapters.cpp:6407`），但三个 op 已按 lease `cap` 自并行，
所以本轮的墙钟瓶颈与 work_units 无关，**是锁**。

---

## 5. 混淆因子（必须与 2.03× 一起读）

### 5.1 `sky_plane` gauge 修复新增了真实工作量

| 产物 | `p2_corrected.json.sky_plane_applied` | 1 worker upm_apply |
|---|---|---|
| `mosaic_out_w1`（旧构建） | **false**（`[sky_plane] build FAILED rc=6` → 回退 UPM C 场） | 348.3 s |
| `bitref_1w`（新构建） | **true** | 792.2 s |

新构建在 upm_apply 里对每个 tile 的**全部 2^18 像素**做 `pix2ang_nest`（`module_adapters.cpp:4904-4912`）
并对有效像素做 `p2_sky_plane_eval_block`（`:4944`），而旧构建走回退分支完全跳过。
`sky_plane.apply.tile` 逐 tile wall 之和：旧构建 217.2s → 新构建 2144.2s（含锁等待）。
⇒ upm_apply 的 348→792s **主要是并发 agent 的 sky_plane 修复，不是 P2-IMPL**。

**因此正确的同口径加速比 = 1568.7s（新构建 1w）→ 654.5s（新构建 16w）= 2.40×。**
（若再扣掉 hash 尾：监测窗 1369.0 → 611.7 = 2.24×。）

### 5.2 page cache 让 sample 波动近 2×

同一配置、同一构建，`probe16w`（冷）sample 266.4s，`probe16w_lock2`（热）157.9s。
输入产品 49×370MB ≈ 18 GB > 12 GB page cache，且每次运行还要写 18 GB 产物 ⇒ 基本每跑必冷。
⇒ 654.5s 是冷输入口径；比较任何方案时必须在同一缓存状态下测。

### 5.3 并行把总 CPU 抬高了 ~66%（锁护航的代价）

| run | WALL | USER+SYS | 有效核 |
|---|---|---|---|
| OLD 1w | 1327.9 | 1064.7 | 0.80 |
| NEW 16w | 654.5 | 1925.4 | 2.94 |

多出的 ~860 core·s ≈ sky_plane 新增（1w 口径 ~336 core·s）+ 锁护航空转/内存带宽。
这也是资源门 `avg_equivalent_cores 2.74 < 13.6` 的直接原因：
**分母是整段监测墙钟，而 60%+ 的墙钟跑在 <1–2 核上**。

---

## 6. S3 方案（只给方案，不实现）

### 6.1 S3.1 —— sample query/fill 双扫描去重（**最高性价比，零科学风险**）

**现状**：`p2_op_sample`（`module_adapters.cpp`）对同一个 `p2_sample_controls_cached` 调两次：

- `module_adapters.cpp:4375`：`out_obs=nullptr, out_capacity=0, out_controls=nullptr, ctrl_capacity=0`，
  只为拿 `n_obs/n_controls`（用于给第二次分配缓冲）；
- `module_adapters.cpp:4385`：带真实缓冲再跑一遍。

两次调用都**完整跑 pass1（全部 tile 读）/ pass2 / pass3**（`sampler.cpp:1227 → p2_sample_controls_impl`；
`out_obs==nullptr` 没有早退）。旧串行 stderr 证据：`[sampler] enter` 出现 2 次，
pass1 分别 60.1s / 79.4s，合计约 87% 的 sample 时间花在 pass1，其中一半是纯冗余 I/O。

**方案（推荐）**：给 sampler 增加一个**输出 vector 的重载**，让 impl 把已经构造好的局部
`obs` / `sky` / `mask` / `cells`（`sampler.cpp:1175-1204` 处已有 move/copy 落点）**移出**，一次调用搞定：

    // sampler.h / sampler.cpp 新增（示意）
    int p2_sample_controls_vec(const P2CoverageResult*, const char* const* paths,
                               const std::uint64_t* frame_ids, const P2SamplerConfig*,
                               std::vector<P2ControlObservation>* obs_out,
                               std::vector<P2ControlNode>* nodes_out,
                               P2SampleStats* stats, char* err, std::size_t err_size);

`p2_op_sample` 改为单次调用。**同一函数、同一输入、同一遍历序 ⇒ `p2_samples.json` 逐字节不变**，
科学数值零风险（不需要新回归口径，只需沿用现有 1w/16w 位级回归）。
**预期**：省掉一整遍 pass1+pass2+pass3，sample **274 → ~137s（冷）/ 158 → ~79s（热）**，
即 **端到端省 ~120–137s（~20%）**，同时把 sample 的锁读次数减半。

**次选（改动更小但收益略低）**：把第一次调用的结果（`obs`/`nodes`/`stats`）缓存到函数内静态，
第二次直接复用——但引入可变静态状态，不如 vector 重载干净。

### 6.2 S3.2 —— pass2 / pass3 按 cell 并行（低收益，中等风险）

- **pass2**（`sampler.cpp:987-1044`）：按 cell 并行。每个 cell 只写自己的
  `cells[ci].accepted/reason`；邻域只**读**同 tile 的其它 cell（`:1001-1020`）⇒ 无数据竞争。
  三个整数计数器（`rejected_bright_tolerance/high_contamination/insufficient_retained`）
  按 cell 序合并（整数结合律 ⇒ 精确）。
- **pass3**（`sampler.cpp:1046-1113`）：按 cell 并行，但**必须按 cell 升序、cell 内 frame 升序拼接**
  `obs`/`sky`，否则 `p2_samples.json` 的发射顺序变化。实现：每 cell 私有 `obs_c[ci]`/`sky_c[ci]`，
  join 后按 `ci` 升序串行拼接（或先用前缀和算出每 cell 的 offset 再定长写入，同 reject/integrate 的做法）。
  pass3 里的 `aio_hips_read_leaf_f32`（`:1101`，ivar）在本数据集 ivar 缺失、成本为 0，但若存在仍是锁内 I/O。
- **预期**：pass2+3 合计 ~20s（1w 口径）⇒ 16w 下省 ≤20s。**收益远小于 S3.1，建议最后做。**

### 6.3 锁无关 tile 读路径（S3 之外，但天花板最高）

证据链已齐：cfitsio 全局锁占 90.3% 互斥等待、16 线程 0.93×、raw `pread` 冷读 3.0×。
可选路径（按风险从低到高）：

1. **对「普通未压缩图像 tile」走直读**：signal/support 的 tile 就是 2880B 头 + 512×512 float/double
   的平面 FITS。自解析 `NAXIS/NAXIS1/NAXIS2/BITPIX` 后用 `pread` 读数据面，绕开 cfitsio 与全局锁；
   遇到压缩（`ZIMAGE`/`COMPRESSED_IMAGE`）或 BINTABLE 时**回退**现有加锁路径。
   要求：输出缓冲与 `read_tile_t` 逐位一致（现有位级回归可直接覆盖）。
2. 每 (worker, frame) **常开** fitsfile，只把 `fits_read_pix` 留在锁内——但 RT-008 明确全局表不安全，
   需要先验证 cfitsio `_REENTRANT` 语义，风险高于路径 1。
3. 跨 reject/integrate 缓存 support tile（同一 5609 对读两遍）：内存 ~5.6GB，收益一般，不建议。

**预期**：若读能并发（哪怕只到 raw `pread` 的冷读 508MB/s 水平），
sample/reject/integrate/upm_apply 的读地板可从 ~450s 压到 ~150s，端到端有望 **654 → ~400s**。

### 6.4 附带候选：upm_apply 的 sky-plane 求值去冗余（非 S3）

`module_adapters.cpp:4904-4912` 对每个 tile 的**全部 2^18 像素**算 `pix2ang_nest`，
但 49 帧只覆盖 523 个不同的 tile（5609 个 tile 实例 ⇒ 平均每 tile 重复 10.7 次）；
且只有 `support>0` 的像素才需要 `ra/dec`。可（a）把 `pix2ang` 移进 valid 像素循环，
（b）按 tile 缓存 `ra/dec`（523×4MB ≈ 2.2GB，内存需评估）。
`sky_plane.apply.tile` 旧构建逐 tile 和 217.2s、新构建 2144.2s（含锁等待），量级不小，但需科学侧确认无副作用。

---

## 7. 下一步优先级建议

| 优先级 | 改动 | 预期收益 | 风险 | 依据 |
|---|---|---|---|---|
| **P0** | **S3.1 sample query/fill 去重**（§6.1） | **−120…137s（~20%）**，sample 锁读减半 | **零科学风险**（同函数同序，字节不变） | sample 占 42% 墙钟且 100% 被锁串行 |
| **P1** | **锁无关 tile 读路径**（§6.3 路径 1） | 天花板 **−250s+**（654→~400s） | 中（需位级回归 + 压缩回退） | cfitsio 锁占 90.3% 互斥等待；raw pread 冷读 3.0× |
| **P2** | S3.2 pass2/3 并行（§6.2） | ≤ −20s | 中（须保发射顺序） | pass2+3 仅占 sample 的 ~13% |
| **P3** | sky-plane `pix2ang` 去冗余（§6.4） | upm_apply 内 ~10–30s | 低-中（科学确认） | 10.7× 重复求值 |

**一句话**：先做 **S3.1**（白捡 20%、零风险），再评估 **锁无关读路径**（唯一能把 654.5s 拉到 400s 量级的改动）；
S3.2 与 sky-plane 去冗余排在后面。**不建议**在没有位级回归前动 cfitsio 句柄策略。

---

## 8. 证据与复现

### 8.1 证据文件（`run/RELEASE-02/perf-p2b/`）

| 文件 | 内容 |
|---|---|
| `stage_mtimes.txt` | 五个 run 的逐阶段墙钟（产物 mtime 独立口径） |
| `old_stages.txt` / `new_stages.txt` / `lock2_stages.txt` | 旧构建 1w / 新构建 16w / 新构建 16w(热) 的逐阶段 wall+核数 |
| `mutex_attribution.txt` | LD_PRELOAD 互斥等待按调用点 + 按阶段归因（核心证据） |
| `aio_bench_hot.txt` | `aio_hips_read_tile_f32` 1 vs 16 线程（热） |
| `pread_cold49.out` | raw `pread` 全量 9.3GB 冷读 1 vs 16 线程（3.0×） |
| `pread_concurrency.out` | raw `pread` 单帧冷/热对比 |
| `bitref16w_profile.txt` | bitref_16w 资源曲线按阶段窗口的 CPU/线程/iowait |
| `out/probe16w*/` | 三次带探针运行（含 `probe.jsonl`、`mutex.log.<pid>`、`resource_timeseries.csv`） |

### 8.2 脚本

- `analyze_p2_stages.py`：探针 END 语义 → 逐阶段 wall + 有效核数 + thread_id 分布
- `stage_from_mtimes.py`：产物 mtime → 逐阶段墙钟（独立交叉验证）
- `analyze_mutex.py`：互斥日志 → `addr2line` 归因 + 阶段窗口映射
- `mutex_probe.c` + `libmutexprobe.so`：互斥等待 LD_PRELOAD 探针
- `aio_read_bench2.cpp`：cfitsio 读 1 vs N 线程
- `pread_concurrency.py` / `pread_cold49.py`：绕过 cfitsio 的 raw pread 并发
- `run_probe16w*.sh`：带探针/带 shim 的 mosaic 复现脚本

### 8.3 复现命令

    export TMPDIR=/dev/shm/astrocs_p2b; mkdir -p "$TMPDIR"
    cd '/workspace/Astro CS Database'

    # 互斥探针
    gcc -O2 -fPIC -shared -o run/RELEASE-02/perf-p2b/libmutexprobe.so \
        run/RELEASE-02/perf-p2b/mutex_probe.c -ldl -lpthread

    # 带探针 + shim 跑一次（16 worker = 全 affinity；产物落 perf-p2b/out/）
    bash run/RELEASE-02/perf-p2b/run_probe16w_lock2.sh

    # 归因
    python3 run/RELEASE-02/perf-p2b/analyze_p2_stages.py \
      run/RELEASE-02/perf-p2b/out/probe16w/probe.jsonl \
      run/RELEASE-02/perf-p2b/out/probe16w/resource_timeseries.csv "NEW 16w"
    python3 run/RELEASE-02/perf-p2b/analyze_mutex.py \
      run/RELEASE-02/perf-p2b/out/probe16w_lock2/mutex.log.<pid> \
      "$PWD/build/astrocs" run/RELEASE-02/perf-p2b/out/probe16w_lock2/probe.jsonl

    # 读路径微基准
    g++ -O2 -std=gnu++17 -Ilib/infrastructure/aio/include -o run/RELEASE-02/perf-p2b/aio_read_bench2 \
        run/RELEASE-02/perf-p2b/aio_read_bench2.cpp build/libastrocs_hips.a build/libastrocs_aio.a \
        build/libastrocs_cfitsio.a build/libastrocs_hips_properties.a build/libastrocs_common.a -lz -lm -lpthread -ldl
    python3 run/RELEASE-02/perf-p2b/pread_cold49.py

### 8.4 行号快照

`module_adapters.cpp` 当前：sample 两次调用 `:4375` / `:4385`（任务书写的 `4339/4349` 已因并行 agent 改动漂移）；
reject 并行 `:5261`（`P2SupportReader` `:5243`）；integrate 并行 `:5914`（`P2FrameReader` `:5888`）；
upm_apply 并行 `:4832`；`pix2ang` `:4904-4912`；`work_units=1` `:6407`。
`aio_hips_reader.cpp`：`read_tile_t` 锁 `:126`，MOC 锁 `:216`。
`sampler.cpp`：`g_aio_mu` `:169`，open 加锁 `:174/:728/:729`，pass2 `:987`，pass3 `:1046`，
输出落点 `:1175-1204`，入口 `:1227`。
