# RELEASE-02 · Phase2（mosaic）并行度分析（PERF-P2）

- 任务：定位「16 个 worker 只跑出 0.5–0.9 核」的根因（到 file:line）、给出逐阶段并行化方案与
  逐位可复现保证、Amdahl 预期收益、以及 `[sky_plane] build FAILED rc=6` 的数值归因。
- 范围：**只分析，未改任何代码**；零 git 写权限；未跑 `ninja`/`cmake`/`ctest`。
- 工作目录：`/workspace/Astro CS Database`；证据目录：`run/RELEASE-02/perf-p2/`。
- 数据源：L4 真实运行 `mosaic_49_w1`（49 帧 Phase2，rc=0）：
  `run/RELEASE-02/L4-rebuild/probe_mosaic_w1.jsonl`（6831 事件）、
  `run/RELEASE-02/L4-rebuild/mosaic_out_w1/resource_timeseries.csv`（1777 采样）、
  `run/RELEASE-02/L4-rebuild/logs/mosaic_w1.stderr`（`WALL=1327.88 USER=940.47 SYS=124.24`）。

> **行号快照与复核**：本报告的 `module_adapters.cpp` 行号已按修订
> `sha256=90a4cc95a6223d77fec15d6509657d8cc9b7bbb9451b88b549ecff0ef01112f3`（mtime 2026-09-19 01:21:32）
> 重新锚定。该文件在本次分析期间被**并行修改**（`git diff --stat` +202 行），若再次变动请以符号名复核。
> 其余被引用文件（`sampler.cpp` / `sky_plane.cpp` / `upm.cpp` / `aio_hips_reader.cpp` / `commands.cpp` /
> `parser.cpp` / `canonical_hash.cpp` / `sha256.cpp` / `rejection.cpp`）在分析期间未变。
> 40 条引用全部通过：`python3 run/RELEASE-02/perf-p2/verify_refs.py`（输出 `verify_refs.out`）。

---

## 0. 结论摘要（TL;DR）

> **16 个 worker 只跑 0.5–0.9 核，根因不是「没活干」，也不是任务图被压成串行 ——
> 而是 Phase2 的三个最大阶段根本就是一段裸串行 `for` 循环，跑在调度线程上；
> 16 个 worker 只是 ThreadLease 的账面数字，从未被派发任何 work unit。**

1. **调度层**：Phase2 节点 `P2NodeModule::plan()` 声明 `work_units = 1`
   （`lib/infrastructure/scheduler/src/module_adapters.cpp:6407`），且
   `parallel_axes`（:6408）**全仓只有赋值、没有任何消费者**。调度器无 work unit 可扇出。
2. **执行层**：`execute()`（:6413）领到 `cap=16` 的租约后，只把 `__workers=cap` 塞进 config
   （:6471），然后**在调用线程上直接调 `p2_op_*`**；`execute` 体内无 `std::thread`、无
   `#pragma omp`、无任务提交。
3. **实证（单线程）**：探针逐 tile 事件的 `thread_id` 完全单一 ——
   `sky_plane.apply.tile` 5609 条**全部** `thread_id=2`；`reject.tile` 523 条**全部** `thread_id=3`；
   `integrate.tile` 523 条**全部** `thread_id=3`。
   资源曲线上这三阶段的 `active_compute_threads` 均值 = 2.00 / 2.02 / 2.00（峰值 3）。
4. **全局锁**：所有 HiPS tile 读都取**进程级** `aio::cfitsio_io_mutex()`
   （`lib/infrastructure/aio/src/hips/aio_hips_reader.cpp:126`）。即使补上数据并行，
   FITS tile 读仍会被这一把全局锁串行化 —— 这是第二条（I/O 侧的）并行度上限。
5. **`sample` 是唯一真有并行区的阶段**：`sampler.cpp:921-948` 起了 16 个 `std::thread`、
   用 `next_c.fetch_add`（:932）原子认领 cell；实测 `active_compute_threads` 峰值 17。
   但它 I/O 受限（cpu 均值 0.69 核），**且整轮扫描被 query+fill 各做一遍**
   （`module_adapters.cpp:4339` 与 `4348`），stderr 里 `[sampler] enter` 恰好出现 2 次 ⇒ 2× 冗余 I/O。
6. **33% 的墙钟在资源门监测窗之外**：`WALL=1327.88s` 但资源门只监测到 `888.51s`；
   尾部 **439.37s** 是 `cmd_session2_run` 对 67 个 artifact（18.45 GB）**串行**做
   `file_sha256` + `canonical_product_hash_file`（`commands.cpp:1022-1030`，实现为
   `parser.cpp:207` 64KB 流式 + `canonical_hash.cpp:319` 整文件读入），
   且用的是**无 SHA-NI 的纯软件 SHA-256**（`lib/algorithms/shared/crypto/sha256.cpp:38-66`），
   同一文件还被重复哈希 2–3 遍。文件时间线佐证：最后产物 00:55:43.76 → run manifest 01:03:03.01。
7. **`sky_plane` rc=6 是结构性数值缺陷，不是数据质量问题**：`B_ref` 与逐帧 δ_k 的多项式基
   共享一个未约束的 gauge（常数 + 线性），而二阶差分粗糙度惩罚恰好在同一方向上有零空间 ⇒
   `H_solve` 精确奇异 ⇒ `chol_spd` 失败（`sky_plane.cpp:770`）⇒ `P2_SKY_PLANE_RANK_DEFICIENT(6)`。
   默认 `gauge_mode=0`；`gauge_mode=1` 的居中在**求解之后**（:868）才做，救不了 :770 的前提。
   现状是「显式记录后回退到 UPM C 场」（`module_adapters.cpp:4632-4634`）——不是静默，但它是
   **每次都必然触发的固定降级**，应修 gauge 而不是长期容忍。
8. **收益上限提醒**：即使把三个串行计算阶段全部并行到 16 核，整跑也只从 1327.9s → 694s（**1.91×**），
   因为 439s 串行哈希尾部 + 161s I/O 受限的 sampler 会立刻成为新瓶颈。**必须同时处理尾部哈希**，
   整跑才有 4.7×（N=16）/5.4×（N=32）量级。

---

## 1. 证据基线与墙钟对账

### 1.1 阶段表（探针 `ts_utc` = **结束**时刻；窗口 `[t_end−wall, t_end]` × `resource_timeseries.csv`）

| 阶段 | wall_s | cpu 均值(核) | per_thread_cpu_sum 均值 | active_compute_threads 均/峰 | threads 均 |
|---|---|---|---|---|---|
| coverage | 0.01 | 0.00 | 0.00 | 0/0 | 2.0 |
| **sample** | **160.79** | **0.69** | 0.67 | **3.42 / 17** | 23.6 |
| upm_fit | 3.63 | 1.24 | 1.04 | 2.00 / 2 | 19.0 |
| **upm_apply** | **347.70** | **0.86** | 0.86 | **2.00 / 3** | 19.0 |
| **reject** | **214.05** | **0.68** | 0.68 | **2.02 / 3** | 19.0 |
| **integrate** | **138.92** | **0.53** | 0.53 | **2.00 / 3** | 19.0 |
| write | 23.10 | 0.72 | 0.72 | 1.78 / 3 | 19.0 |
| **监测窗合计** | **888.20** | | | | |

复现：`python3 run/RELEASE-02/perf-p2/analyze_p2.py`（输出 `analyze_p2.out`）。
`active_workers` 在全部 1777 个采样点恒为 16（`resource_summary.json` 的 `workers_mean=16.00`），
但 `active_compute_threads` 在这三个阶段只有 2 —— **「在册」与「在跑」是两回事**。

### 1.2 整跑墙钟对账（本轮新增的关键事实）

| 项 | 值 | 出处 |
|---|---|---|
| `WALL` | **1327.88 s** | `logs/mosaic_w1.stderr`（/usr/bin/time） |
| `USER` / `SYS` | 940.47 / 124.24 s | 同上 ⇒ (U+S)/WALL = **0.802 核** |
| 资源门 `wall_seconds` | **888.51 s** | `mosaic_w1.events.jsonl` resource 事件 |
| 探针阶段合计 | 888.20 s | `probe_mosaic_w1.jsonl` |
| **未监测尾部** | **439.37 s（33.1%）** | 1327.88 − 888.51 |
| 资源门判据 | `avg_equivalent_cores 0.732729 < 0.85×16=13.6` | `low_avg_cores`（record_only） |

**尾部是什么**：`cmd_session2_run`（`lib/infrastructure/cli/commands.cpp:959`）在
`run_with_resource_gate` 返回、`write_run_manifest` 之前，对每个 node artifact 串行哈希：

```cpp
// commands.cpp:1022-1030
for (const std::string& ap : astrocs::cli::collect_node_artifact_paths(mans)) {
    bool ok2 = false;
    const std::string sha = file_sha256(ap, &ok2);          // pass 1
    ...
    artifacts.push_back(with_canonical_hash(
        {{"path", ap}, {"sha256", ok2 ? sha : ""}, {"size_bytes", ...}}, ap));  // pass 2 + 3
}
```

- 体量：run manifest 实测 **67 个 artifact / 18.45 GB**（其中 49 个 `p2_corrected_f*.bin` ≈ 11.8 GB，
  `p2_integrated_*.bin` ≈ 4.4 GB，`p2_rejection_sample_mask.bin` 1.47 GB）。
- 冗余：`with_canonical_hash`（:124）内部又调一次 `file_sha256`（:127）——
  与 :1024 完全重复；随后 `canonical_product_hash_file`（`canonical_hash.cpp:311`）用
  `read_file_bytes`（:319）**把整个文件读进 `std::string`** 再哈希一次（:324）。
  ⇒ 每个 artifact 2–3 遍整读 + 2–3 遍 SHA-256；1.47 GB 的文件会被整块读入内存。
- 实现：`astrocs::crypto::Sha256` 是**纯软件 FIPS 180-4，无 SHA-NI / 无 intrinsics**
  （`lib/algorithms/shared/crypto/sha256.cpp:38-66`）。
- 时间线佐证：`ls --time-style=full-iso` 显示最后产物 `alloc_report.json` = 00:55:43.76，
  run manifest = 01:03:03.01 ⇒ **439.25 s 内没有任何文件写活动**。

> **资源门本身漏掉了这 439s**：`active` 监测窗在 `ev.stage("phase2_session", false)`（:1017）
> 时结束，而哈希循环在其后。所以「0.73 核」这个判据**低估了真实墙钟占比**，
> 也**高估了**「计算阶段已利用的核数」（分子里还含尾部 CPU，分母却不含尾部墙钟）。

---

## 2. 根因定位（file:line）

### 2.1 调度层：`work_units = 1`，没有任何 work unit 可以领

```cpp
// lib/infrastructure/scheduler/src/module_adapters.cpp:6402-6411  (P2NodeModule::plan)
Result<ModulePlan> plan(const std::string& node_id, const std::string& config_json) override {
  config_ = config_json;
  ModulePlan p;
  p.node_id = node_id;
  p.work_units = 1;                          // :6407  <-- 一个节点 = 一个 work unit
  p.parallel_axes = {"tile-leaf-band"};      // :6408  <-- 声明了轴, 但没有消费者
  p.cpu_heavy = desc_.execution_class == "cpu_heavy";
  return Result<ModulePlan>::ok(std::move(p));
}
```

- 七个 Phase2 节点描述符全部 `execution_class="cpu_heavy"`（`:906/:925/:944/:963/:983/:1002`；
  仅 `write` 是 `"io"` :1022）。
- `grep -rn parallel_axes lib` 的结果**只有 10 处赋值**（:473/:6173/:6176/:6179/:6182/:6185/
  :6188/:6191/:6408/:7456），**零消费点** ⇒ 这个字段目前是文档性元数据。
- RT-005 的守卫本应拦住这种组合：`plan_estimator.cpp:496`
  `if (in.is_heavy && e.max_useful_workers == 1 && !e.tiny_work) -> HEAVY_TINY`。
  但 `estimate_plan` 在生产路径上**没有任何调用者**（只有 `plan_estimator.cpp` 自身与测试），
  Phase2 的 IR 路径绕过了它。

**结论**：worker 拿不到 work unit，不是「任务图把并行压成串行」，而是**计划里只有 1 个 unit**。

### 2.2 执行层：三个最大阶段是裸串行 `for` 循环（单线程实证）

```cpp
// module_adapters.cpp:6413-6470  (P2NodeModule::execute)
ThreadLease lease = ctx.acquire_lease(host_workers);          // :6416 领 16
const uint32_t cap = lease.acquired() ? lease.size() : 1u;    // :6417 cap=16
...
cfg2["__workers"] = cap;                                      // :6471 只塞进 config
switch (spec_.op) {                                           // 然后在**调用线程**上直接调
  case P2NodeOp::UpmApply: r = p2_op_upm_apply(cfg2, &man); break;   // :6488
  ...
}
```

`execute` 体内没有 `std::thread`、没有 `#pragma omp parallel`、没有 work queue。
`module_adapters.cpp` 里确实存在并行实现（`:3035 #pragma omp parallel for schedule(dynamic,8)`、
`:6933-7026` 的原子行带计数），但**都不在 Phase2 的 op 上**。

三个最大阶段的串行循环与单线程实证：

| 阶段 | 串行循环 | 迭代数 | 探针 | `thread_id` 分布 | tile 探针合计 |
|---|---|---|---|---|---|
| **upm_apply** | `for (size_t f=0; f<paths.size(); ++f)` :4748 → `for (int t=0; t<n_tiles; ++t)` :4780 | 49 帧 × ~114 tile = **5609** | `sky_plane.apply.tile` | **{2: 5609}** | 217.2 s |
| **reject** | `for (const auto& [tip, refs] : union_tiles)` :5086 | **523** tile | `reject.tile` | **{3: 523}** | 212.8 s |
| **integrate** | `for (const auto& rt : rej_tiles)` :5604 | **523** tile | `integrate.tile` | **{3: 523}** | 134.3 s |

（`upm_apply` 的 tile 探针只覆盖 217.2 s，另外 **130.5 s** 在循环外：
每帧 `p2_node_frame_id`（:4751 → `sampler.cpp:324`，**把整帧 HiPS payload 再流式哈希一遍**）
+ 每帧 `p2_write_bin` 245 MB（:4872）+ JSON 组装。）

**负载均衡**（tile 粒度足够细，适合并行）：

| 阶段 | tile 数 | 均值 | 最大 | ideal@16 | max/ideal | top-16 tiles 占比 |
|---|---|---|---|---|---|---|
| upm_apply | 5609 | 0.039 s | 0.171 s | 13.6 s | 0.01 | 1.1% |
| reject | 523 | 0.407 s | 2.601 s | 13.3 s | 0.20 | 12.9% |
| integrate | 523 | 0.257 s | 2.869 s | 8.4 s | 0.34 | 22.6% |

⇒ 动态调度下 makespan ≈ max(ideal, 最大 tile)，三者效率都 ≥ 95%。

### 2.3 全局锁：所有 HiPS tile 读被进程级 cfitsio 互斥串行化

```cpp
// lib/infrastructure/aio/src/hips/aio_hips_reader.cpp:123-127
template <typename T>
static int read_tile_t(AioHipsDataset* d, uint64_t ipix, T* out) {
    if (!d || !out) return -1;
    std::lock_guard<std::mutex> cfitsio_guard(aio::cfitsio_io_mutex());   // :126 进程级全局锁
    ...fits_open_file / fits_read_pix / fits_close_file...
}
// :430  int aio_hips_read_tile_f32(...) { return read_tile_t(d, ipix, out); }
```

- `aio_cfitsio_mutex.h:11` 是**单一 Meyers 单例**；`aio_fits.cpp:525-529` 注释明确
  「cfitsio 全局表在并行访问下非线程安全 → 进程级串行化（RT-008）」。
- 影响：即便给每个 worker 各自的 `AioHipsDataset*`（sampler 的 `SamplerReader::init_own`
  已经这么做，见 `sampler.cpp:727-729`），**tile 读本身仍会互相排队**。
- Phase2 各阶段的 FITS 读量（`aio_hips`）：reject/integrate 每 (tile,frame) 读 support 512²×4B=1MB，
  共 ~5609 个 pair ≈ 5.6 GB；corrected 数据面走 `p2_read_bin_range`（:4122，ifstream，不受该锁影响）
  ≈ 11.2 GB。**所以 FITS 那 5.6 GB 是被全局锁串行化的部分，.bin 那 11.2 GB 不是。**
- 这一条解释了为什么 `sample` 起了 16 个线程却只有 0.69 核：pass1 的 tile 读全在
  `cfitsio_io_mutex` 后面排队，线程大部分时间在等锁/等盘。

### 2.4 `sample`：唯一有并行区的阶段，但 I/O 受限 + 整轮扫描做两遍

- 并行区**已存在**：`sampler.cpp:918-948`，`workers = cfg.cpu_workers`（= `__workers` = 16），
  `std::thread` × 16，`next_c.fetch_add(1)`（:932）原子认领 cell；实测
  `active_compute_threads` 峰值 **17**。
- 但 **pass2**（:987 Stage C DBE-like tolerance gate）与 **pass3**（:1046 输出观测）**是串行的**。
- 且 `p2_op_sample` 把整个 `p2_sample_controls_cached` 调了**两次**：
  `module_adapters.cpp:4339`（query 容量，`out_obs=nullptr`）与 `:4349`（fill）。
  实现里没有 `out_obs==nullptr` 的早退 ⇒ 两次都跑完 pass1/pass2/pass3。
  证据：stderr 中 `[sampler] enter n_union=523 grid=8 n_frames=49 target_order=9` 恰好出现 **2 次**，
  且 `[sampler] cells resized 33472, first tile read check` 也出现 2 次。
  ⇒ **sample 阶段有一半是纯冗余 I/O**（49 帧 HiPS 再读一遍）。
- 另外 `p2_op_sample` 开头为 49 帧各算一次 `p2_node_frame_id`（:4326），同样是整帧内容哈希。

### 2.5 `write`（23.1 s，`io` class）

`p2_op_write`（:5866）走 AIO HiPS 逐 tile 写（523 tile），受同一把 cfitsio 全局锁；
`parallel_ok=false`（descriptor :1023）。占比小（2.6%），优先级最低。

### 2.6 根因判定（回答任务书四选一）

| 候选 | 判定 | 依据 |
|---|---|---|
| 任务粒度太粗（整个阶段只有 1 个 task） | **是（主因）** | `work_units=1`（:6407）+ op 内裸串行循环 |
| 有全局锁/串行段 | **是（次因）** | `cfitsio_io_mutex`（aio_hips_reader.cpp:126）；sample 的 pass2/3 串行 |
| worker 拿到 lease 但没 work unit 可领 | **是（同一件事的调度侧表述）** | `execute` 只领租约不派发（:6416-6470） |
| 依赖图把并行压成串行 | **否** | 节点间是纯链式依赖，节点**内部**本就串行；DAG 不是瓶颈 |

---

## 3. 逐阶段并行化方案（改哪个循环 / 怎么切分 / 共享累加 / 逐位可复现）

### 3.1 与 Phase1 drizzle 模式对照

| drizzle（P15a 已验证） | Phase2 对应物 | 说明 |
|---|---|---|
| `drizzle_deterministic_stripe_count(height)` :1597（只依赖 height） | **tile 划分 = 上游 artifact 里已固定的 `tile_ipix` 升序集合**（523 个） | 与 worker 数无关，天然确定 |
| `#pragma omp parallel num_threads` :1796 | 同款 `#pragma omp parallel for schedule(dynamic)`（或 `std::atomic` 认领） | 建议复用 `module_adapters.cpp:3035` 已有写法 |
| `std::atomic next_stripe` :1856 | `std::atomic<uint64_t> next_tile` / omp dynamic | 动态认领，抗负载不均 |
| 每 stripe 私有 `TileAccumulator` :1759 | 每 tile 私有输出缓冲（`sig_bin/sup_bin/...` 的 tile 切片） | 无共享写 |
| `merge_cursor` 按 stripe **升序**归约 :1961 | 按 tile 升序**写入预分配的固定 offset** | 见下：Phase2 更强 |
| FP 加法结合树随 stripe 序固定 | **不存在跨 tile 的 FP 归约** | 见 §3.7 确定性论证 |

> **关键区别**：drizzle 是「源像素 → 输出叶」的 scatter，**有跨 stripe 的浮点累加**，
> 所以必须靠「固定 stripe + 升序归约」锁住结合树。
> Phase2 的 upm_apply / reject / integrate 是「**每个输出 tile 的每个像素完全在 tile 内算完**」，
> 跨 tile **没有任何浮点归约**（只有整数计数器）。因此 tile 级并行是
> **逐位可复现 by construction**，比 drizzle 的保证更强。

### 3.2 `upm_apply`（347.7 s → 预期 ~28 s @16）

- **改哪个循环**：`module_adapters.cpp:4748` 的帧循环 + `:4780` 的 tile 循环。
- **怎么切分**：以 **(frame, tile)** 为并行单元（5609 个）；最少改动是「以 frame 为单元
  （49 个，每帧内部保持 tile 升序）」，推荐前者（负载更细，49 帧对 16 worker 有 3 轮尾巴）。
- **共享状态处理**：
  - `model`（`p2_upm_open` 的返回值）：`p2_upm_calibrate_block(const void* model, ...)`
    （`upm.cpp:1274-1303`）**只读**、无 static/可变状态 ⇒ 可跨线程共享。
  - `sky_model`：`p2_sky_plane_eval_block(const void* model, ...)`（`sky_plane.cpp:998`）同样只读 ⇒ 共享。
  - `local_lut`（:4709）：初始化后只读 ⇒ 共享。
  - `ft.data` / `ft.tiles` / `frames_j`：**每帧私有**；把 `ft.data` 预分配为
    `n_tiles*kP2TileLeafSpan`，每个 tile 写自己的 `tile_offset` 段 ⇒ 无竞争、字节序与串行一致。
  - `total_pixels`：整数求和 ⇒ `std::atomic<uint64_t>` 或 join 后按帧序相加（整数，精确）。
  - `frames_j`：join 后**按 frame 升序**（= `paths` 序）组装。
  - 每帧的 `p2_corrected_f<fid>.bin` 文件名只依赖 `fid` ⇒ 各 worker 可独立写，无顺序约束。
- **I/O 注意**：`aio_hips_open` 每 worker 独立（禁跨线程共享句柄，同 `sampler.cpp:727-729` 契约）；
  `read_tile_t` 的全局 cfitsio 锁无法绕开（§2.3）。
- **顺带可省**：每帧 `p2_node_frame_id`（:4751）的整帧内容哈希是纯冗余（sample 阶段已算过），
  可改为消费 `p2_corrected.json` 上游登记的 `frame_id`；这能省掉约 12 GB 的重复读。

### 3.3 `reject`（214.1 s → 预期 ~15 s @16）

- **改哪个循环**：`:5086` `for (const auto& [tip, refs] : union_tiles)`。
- **怎么切分**：以 **union tile** 为并行单元（523 个，升序 map 已有确定顺序）。
- **共享状态处理**（全部可做到无竞争）：
  - 只读面：`acc_all` / `nrej_all`（:5550）、`sample_mask_all`（:5559）、
    `plan_cache`（:5017，按 n 预解析的纯函数结果）、`union_tiles`（只读遍历）、`fsup` 各帧 support 句柄
    —— **注意** `fsup` 句柄需每 worker 独立（cfitsio 非线程安全），或退化为「tile 内串行读、tile 间并行」。
  - 每 tile 私有输出：`accepted_bin`(u8) / `nrej_bin`(u16) / `cand_u16`(u16) / `sample_mask`(u8)
    各切一段（`tile_span` 或 `depth*tile_span`），写在自己的 `out_offset` / `mask_offset` 上。
  - 计数器 `acc_total / rej_low_total / rej_high_total / undet_total / rej_samples_total /
    n_pixels_processed / undet_low_n_pixels / prior_unavailable_pixels`：**整数** ⇒
    `std::atomic` 累加或按 tile 序合并，**与串行逐位一致**。
  - `tiles_j`：按 tile 升序（= map 序）组装。
- **确定性**：无跨 tile FP 归约；kernel `p2_reject_stack_ex`（`rejection.cpp:1922`）是
  `(stack, plan, out)` 的纯函数、无 static 可变状态（`rejection.cpp` 里唯一的 `static` 是
  `astrocs_n_map_method`，纯查表函数）⇒ 每 tile 结果与串行完全相同。

### 3.4 `integrate`（138.9 s → 预期 ~14 s @16）

- **改哪个循环**：`:5604` `for (const auto& rt : rej_tiles)`（tile 循环）；内层
  `:5675` `for (uint64_t p=0; p<tile_span; ++p)` 保持不变。
- **怎么切分**：以 tile 为并行单元（523 个，`rej_tiles` 已是 reject 的升序 tile 序）。
- **共享状态处理**：
  - 只读面：`acc_all`/`nrej_all`/`sample_mask_all`、`cor_index`（:5590）、`ivar`、`fds`。
  - 每 tile 私有：`tile_bufs`（:5612）、`sup_v`/`ivar_v`（:5660-5661）、`vals/weights/supports/accs`、
    以及输出切片 `sig_bin / sup_bin / wsum_bin / nused_bin / nrej_plane`（各 `tile_span`，写在自己的 `out_offset`）。
  - 计数器 `zero_weight_pixels / invalid_pixels / nrej_total / nrej_pix_cursor / sample_rejected_skipped`：
    整数 ⇒ 精确合并。
  - **`sm_cursor` 连续性校验（:5633-5635）必须先做串行前置**：
    `sample_mask_offset` 必须按 tile 升序、恰好铺满 `sample_mask_all`（`sm_cursor += depth*tile_span`）。
    把它抽成**串行预检 pass**（O(523)，成本可忽略），再放并行；或对每个 tile 用 depth 前缀和
    计算期望 offset（无状态、确定）。**这是本阶段唯一需要改结构的点。**
- **确定性**：`p2_integrate_pixel`（`integrate.cpp:19`）是纯函数；每像素的样本栈完全来自本 tile；
  无跨 tile FP 归约 ⇒ 逐位一致。

### 3.5 `sample`（160.8 s → 预期 ~54 s，先省 2× 冗余）

1. **去重 query/fill（最高性价比，零科学风险）**：`module_adapters.cpp:4339/4348` 的两次全扫描
   合并为一次（保留 probe/fill 协议的容量语义，但缓存第一遍的 `obs`/`nodes`，第二遍只做拷贝）。
   直接省掉 ~50% 的 sample 墙钟与 I/O。**不改任何数值**（同一函数、同一输入、同一遍历序）。
2. **pass2（`sampler.cpp:987`）并行**：按 cell 切分；`CellStat` 各 cell 独立写；
   `stats.rejected_bright_tolerance / rejected_high_contamination / rejected_insufficient_retained`
   是整数 ⇒ 精确合并。注意 pass2 的邻域只遍历**同 tile** cells（`:990-992`），天然局部，无跨线程写。
3. **pass3（`:1046`）并行**：每 cell 产出自己的观测列表，最后**按 cell 升序**（再按 cell 内 frame 升序）
   拼接 `obs`/`sky`/`mask` —— 必须保持与串行相同的发射顺序，否则 `p2_samples.json` 字节变化。
4. **I/O 上限**：tile 读仍受 `cfitsio_io_mutex` 限制；若要更高并行度，需要
   （a）把 HiPS tile 读改为无全局锁的 mmap/pread 通道，或（b）降低读放大（去重后已减半）。
   已有位级回归：`lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp`
   （cpu_workers=1 vs 2 位精确）。

### 3.6 `write`（23.1 s）

`p2_op_write`（:5866）逐 tile 写 HiPS。可并行化，但每 tile 写都取 cfitsio 全局锁，
且 `write` 是 `io` class（:1022 `parallel_ok=false`）。预期 ≤1.6×，**低优先级**。

### 3.7 确定性保证（对齐 P15a DRIZZLE-DET-001 的强度）

**核心论证**：Phase2 这三个阶段的输出像素与 tile 一一对应 ——
第 t 个 tile 的 `tile_span` 个输出元素只由该 tile 的帧栈决定，**跨 tile 不做任何浮点累加**。
因此：

1. **输出平面**：预分配 `plane[n_tiles * tile_span]`，tile t 写 `[t*tile_span, (t+1)*tile_span)`。
   拼接顺序 = tile 升序（上游 artifact 的确定顺序）⇒ **与 worker 数、调度顺序无关，字节级一致**。
2. **整数累加**：所有 provenance 计数器是整数求和 ⇒ 结合律成立，顺序无关 ⇒ 精确一致。
3. **JSON 组装**：join 后按固定顺序（frame 升序 / tile 升序）生成 ⇒ 一致。
4. **无 FP 归约** ⇒ 不需要 drizzle 那种「升序归约 + 固定 stripe」来锁结合树；
   但**仍须固定 tile 划分与 tile 内遍历序**（保持现有 `tile_ipix` 升序、行主序），
   否则若把 tile 内像素顺序也打乱，会改变 `p2_integrate_pixel` 的样本栈顺序（该函数内部按输入序计算）。
   **方案：只并行 tile 维度，tile 内一律保持原序。**

**如果将来引入跨 tile 的 FP 归约**（例如把输出改成累积到共享叶），必须用
「per-worker 局部累加器 + 按 worker 索引升序左折叠」（drizzle `:1961 merge_cursor` 同款），
或「固定大小 chunk + chunk 索引升序归约」，并给出等价性证明 + 位级回归。

**注意一处既有例外**：`upm_fit` 的并行归约**不是**位精确 —— `upm.cpp:522-528` 注释明确
「跨 worker 数 = 1e-12 绝对容差，不是位精确（本 per-control 求和的结合顺序随 worker 切片变化）」。
这是已冻结的确定性档位；**本轮方案不触碰它**，也不应把它当作可照搬到 upm_apply/reject/integrate 的先例。

### 3.8 建议的落地顺序（供前台决策，每项独立提交 + 位级回归）

| 优先级 | 改动 | 预期 | 风险 |
|---|---|---|---|
| P0 | `upm_apply` tile/frame 并行（§3.2） | 347.7 → ~28 s | 低（无 FP 归约） |
| P0 | `reject` tile 并行（§3.3） | 214.1 → ~15 s | 低 |
| P0 | `integrate` tile 并行 + `sm_cursor` 串行预检（§3.4） | 138.9 → ~14 s | 低 |
| P0 | artifact 哈希尾部并行/去冗余（§1.2） | 439 → ~28 s | 低（只影响 manifest 字段） |
| P1 | `sample` 去重 query/fill（§3.5.1） | 160.8 → ~80 s | 极低 |
| P1 | `sample` pass2/pass3 并行（§3.5.2-3） | → ~54 s | 中（须保发射顺序） |
| P2 | `plan().work_units` 改为真实 tile 数（:6407） | 计划诚实 + 未来调度可扇出 | 低 |
| P2 | `upm_apply` 复用上游 `frame_id`，免二次整帧哈希（:4751） | 省 ~12 GB 读 | 低 |
| P3 | `write` 并行（§3.6） | 23 → ~15 s | 低 |

---

## 4. 预期收益（Amdahl）

复现：`python3 run/RELEASE-02/perf-p2/amdahl_p2.py`（输出 `amdahl_p2.out`）。
`s` = 串行比例（估计依据见脚本注释与 §3）。T=1327.88s 含 439.37 s 串行哈希尾部。

### 4.1 逐阶段（理想 N 核，未计全局 cfitsio 锁与内存带宽竞争 ⇒ **上界**）

| 阶段 | wall_s | s | N=4 | N=8 | N=16 | N=32 |
|---|---|---|---|---|---|---|
| coverage | 0.01 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| **sample** | 160.79 | 0.55 | 1.51 | 1.65 | **1.73** | 1.77 |
| upm_fit | 3.63 | 0.35 | 1.95 | 2.32 | 2.56 | 2.70 |
| **upm_apply** | 347.70 | 0.05 | 3.48 | 5.93 | **9.14** | 12.55 |
| **reject** | 214.05 | 0.01 | 3.88 | 7.48 | **13.91** | 24.43 |
| **integrate** | 138.92 | 0.04 | 3.57 | 6.25 | **10.00** | 14.29 |
| write | 23.10 | 0.60 | 1.43 | 1.54 | 1.60 | 1.63 |

### 4.2 整跑场景（T = 1327.88 s）

| 场景 | N=4 | N=8 | N=16 | N=32 |
|---|---|---|---|---|
| S0 现状 | 1327.6s (1.00×) | 1327.6s (1.00×) | 1327.6s (1.00×) | 1327.6s (1.00×) |
| **S1** 三个串行计算阶段并行 | 820.9s (1.62×) | 736.4s (1.80×) | **694.2s (1.91×)** | 673.1s (1.97×) |
| **S2** S1 + 并行 artifact 哈希尾部 | 491.4s (2.70×) | 352.0s (3.77×) | **282.3s (4.70×)** | 247.5s (5.37×) |
| **S3** S2 + sample 去重并并行 pass2/3 | 437.1s (3.04×) | 288.7s (4.60×) | **214.5s (6.19×)** | 177.4s (7.49×) |
| **S4** S3 + write 并行 | 430.2s (3.09×) | 280.6s (4.73×) | **205.8s (6.45×)** | 168.4s (7.88×) |

**读法（重要）**：

- **只把 700 s 的串行计算并行化，整跑只快 1.91×** —— 因为 439 s 串行哈希尾部（33%）与
  161 s I/O 受限的 sampler 立刻成为新瓶颈。**「16 worker 跑 0.7 核」与「整跑慢」不是同一个问题**：
  前者是纯串行代码，后者还叠加了一条监测窗之外的串行尾部。
- 真正的高杠杆组合是 **S2**：三个计算阶段 + 哈希尾部 ⇒ **4.70×（N=16）**。
- 上界提醒：所有数字未计 `cfitsio_io_mutex`（§2.3）与内存带宽。实际效率会低于 Amdahl；
  drizzle 的实测参考效率是 **15.43×/16 = 96.5%**（计算密集），Phase2 这三阶段 I/O 占比更高，
  建议按 **12–13×/16** 而不是 16× 做承诺。

---

## 5. `[sky_plane] build FAILED rc=6` 归因与处置

### 5.1 事实链

```
stderr: [sky_plane] build FAILED rc=6 reduced normal matrix not SPD -> explicit fallback to UPM C field
```

- `rc=6` = `P2_SKY_PLANE_RANK_DEFICIENT`（`lib/algorithms/coverage/include/astro/phase2/sky_plane.h:197`）。
- 触发点：`lib/algorithms/coverage/src/sky_plane.cpp:770`
  `if (!chol_spd(H_solve, n_free, L))` → :772 写错误串 `"reduced normal matrix not SPD"` → :773 返回 6。
- `chol_spd`（`sky_plane.cpp:127-146`）是**无 jitter 的 Cholesky**：
  对角元 `!(sum>0) || !isfinite(sum)` 即返回 false。**没有扰动、没有兜底**。
- 复现性：两次 mosaic 运行（`mosaic_49` 与 `mosaic_49_w1`）**都**在同一行失败 ⇒ 不是偶发。

### 5.2 数值根因：未约束的 gauge + 惩罚项的同一零空间

模型结构（`p2_sky_plane_build`）：

1. 参考天光面 `B_ref(u,v)`：`spline_degree=3` 的张量 B 样条（`sky_plane.cpp:396`）。
2. 逐帧残差 `δ_k(u,v)`：`frame_gradient_order=1`（默认，`:398`）⇒
   `m = delta_basis_size(1) = (1+1)(1+2)/2 = 3`（`:111`, `:481`），
   基函数 = `{1, ξ, η}`（`delta_basis` `:114-124`）。
3. 于是存在一个**三维 gauge 自由度**：
   `B_ref → B_ref + a + b·ξ + c·η`，同时 `δ_k → δ_k − (a + b·ξ + c·η)`（∀k≠ref），
   残差不变 ⇒ 联合正规矩阵在 (B, δ) 空间沿该方向**精确奇异**。
4. 求解矩阵 `H_solve = H_red + λ·DᵀD`（`:748-769`），其中 `DᵀD` 是二阶差分 stencil
   （`v={1,−2,1}`，`:757`）。二阶差分**恰好湮灭 {常数, 线性}** ⇒ 惩罚项在 gauge 方向上**没有贡献**。
5. 因此 `H_solve · 1 = H_red · 1 + λ·DᵀD · 1 = 0 + 0 = 0`：
   **常数方向精确奇异** ⇒ `chol_spd` 在对角元上取到 `sum <= 0` ⇒ 返回 false ⇒ rc=6。
6. `gauge_mode` 默认 = 0（`:403`），即**完全不施加 gauge 约束**。
   即使配置成 `gauge_mode=1`，其居中操作在 `:868`（**求解成功之后**）才执行 —— 救不了 `:770` 的前提。
7. 旁证：`:840` 与 `:855-863` 的 rank/κ 诊断（`rank_rtol=1e-10`、`kappa_max=1e8`）
   **永远不可达** —— 因为 :770 已经先失败。这本身就是「诊断链被短路」的信号。

**触发条件**（可判定、可复现）：`n_frames ≥ 2`（每个非参考帧都有一个自由常数项）∧
`frame_gradient_order ≥ 0`（m ≥ 1 含常数项）∧ `gauge_mode == 0`（默认）∧ 立方 B 样条。
本配置全部满足 ⇒ **该路径在当前默认参数下 100% 失败**，与数据质量无关（不是「法方程因数据病态而失去正定」，
而是**结构性秩亏**）。

### 5.3 正确处置（不得静默回退）

现状：`module_adapters.cpp:4628-4637` 在 `p2_sky_plane_build` 失败时打印
`[sky_plane] build FAILED rc=6 ... -> explicit fallback to UPM C field`，
写 `man["sky_plane_status"]="fallback_build_failed"`、`man["sky_plane_rc"]=6`、
`man["sky_plane_error"]=...`，然后**继续**（保留 UPM C 场）。

评价与建议：

1. **它确实不是「静默」**：有 stderr、有 manifest 字段。但它是**每次必然触发的固定降级**，
   等价于「天光面功能在生产链上从未生效」；下游 `upm_apply` 会因 `p2_sky_plane.bin`
   不存在而 `sky_plane_applied=false`（:4892/4891），科学输出缺少 `b_k` 扣除。
2. **正确的修法是修 gauge，而不是修回退**：
   - 在**求解前**消除 gauge 方向。三种等价做法：
     (a) **约束 Σ_k δ_k = 0**（即把 `gauge_mode=1` 的居中做成约束，而不是事后平移）；
     (b) **锚定 B_ref 的常数/线性分量**（例如固定一个节点，或加 `ε·(1ᵀB)` 的零空间锚）；
     (c) 在 `H_solve` 上对**零空间**做极小 Tikhonov 锚（不是全矩阵加 jitter，那样会掩盖真实秩亏）。
   - 无论选哪种，都必须**保持确定性**（固定节点/固定顺序的约束消元），并补一个
     「同配置重复 = 位精确」的回归；同时把 `:840`/`:855` 的诊断链恢复可达。
3. **回退语义要收紧**：rc=6 属于**结构性缺陷**，不是数据不足
   （对比 `P2_SKY_PLANE_FRAME_UNDERDETERMINED=4` 才是数据面）。建议：
   - 修 gauge 之前，把 rc=6 提升为 **fail-closed**（DATA-UNC-001 §30.1 的 no-silent-fallback
     原则），或至少在 run manifest 顶层置一个显式的 `sky_plane_degraded=true`，
     让机器消费者无法把它读成「完整 mosaic」。
   - 参考 `mosaic_49` 那次运行：`integrate` 因 weight chain 未闭合而**硬失败**（rc=2），
     说明本仓已具备 fail-closed 的先例；sky_plane 应同等对待。
4. **本轮不改代码**：以上仅为归因与处置建议，留给下一轮按最小改动面 + 位级回归实施。

---

## 6. 风险与红线

1. **科学数值不得改变**：本报告提出的所有并行化都限定在 **tile 维度**，tile 内遍历序、
   样本栈顺序、权重计算顺序**一律不变**；无跨 tile 浮点归约 ⇒ 逐位可复现（§3.7）。
2. **不要照搬 `upm_fit` 的并行归约**：`upm.cpp:522-528` 明确它是 1e-12 容差档、非位精确。
   若要动它，须先升级确定性档位并补位级回归。
3. **全局 cfitsio 锁是真实上限**：任何 tile 级并行方案的实际效率都会低于 Amdahl 上界；
   承诺收益前需按 12–13×/16 折算。
4. **`plan().work_units` 改动会影响资源门/度量**：改前先确认 `v6_budget.py` 与
   `plan_estimator` 的消费语义（当前 `estimate_plan` 无生产调用者）。
5. **哈希尾部并行只影响 manifest 字段**（`sha256`/`canonical_sha256`/`size_bytes`），
   不影响科学产品；但 `canonical_sha256` 是复现性判据，必须保证口径不变（值相同，只是算得更快）。
6. **本轮零 git 写权限**：报告与证据均落在 `reports/RELEASE-02/` 与 `run/RELEASE-02/perf-p2/`，
   未提交、未改任何源码。

---

## 7. 复现

```bash
export TMPDIR=/dev/shm/astrocs_p2; mkdir -p "$TMPDIR"
cd '/workspace/Astro CS Database'

# 1) 阶段表 + 线程归属 + 墙钟对账 + rc=6 归因
python3 run/RELEASE-02/perf-p2/analyze_p2.py

# 2) 逐阶段与整跑 Amdahl
python3 run/RELEASE-02/perf-p2/amdahl_p2.py

# 3) 代码定位证据（只读 grep）
bash run/RELEASE-02/perf-p2/code_refs.sh

# 4) 复核 1327.88s 与 888.51s 的差值（尾部 = 串行 artifact 哈希）
grep -E 'WALL=|avg_equivalent_cores' run/RELEASE-02/L4-rebuild/logs/mosaic_w1.stderr
python3 -c "import json;print(json.load(open('run/RELEASE-02/L4-rebuild/mosaic_out_w1/resource_summary.json'))['wall_seconds'])"
```

**证据清单**

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/perf-p2/analyze_p2.py` / `.out` | 阶段表 × 资源曲线、线程归属、墙钟对账、rc=6 |
| `run/RELEASE-02/perf-p2/amdahl_p2.py` / `.out` | 逐阶段 + 整跑 S0–S4 场景 |
| `run/RELEASE-02/perf-p2/probe_tile_stats.out` | 逐 tile 探针统计与负载均衡 |
| `run/RELEASE-02/perf-p2/mosaic_cpu_repro.out` | 复现任务书的阶段表（0.86/0.68/0.69/0.53/0.72 核） |
| `run/RELEASE-02/perf-p2/code_refs.txt` / `code_refs_grep.out` | file:line 定位证据 |
| `run/RELEASE-02/perf-p2/sha256_tail_probe.out` | 439s 尾部归因（体量 18.45GB / 吞吐实测） |
| `run/RELEASE-02/perf-p2/verify_refs.py` / `verify_refs.out` | 报告 40 条 file:line 引用的自动复核（40/40 OK） |

---

## 8. 一句话回答

**16 个 worker 只跑 0.7 核，是因为 Phase2 的 upm_apply / reject / integrate 三个最大阶段
在 `lib/infrastructure/scheduler/src/module_adapters.cpp` 里就是裸串行 `for` 循环
（:4748+:4780 / :5086 / :5604，探针 thread_id 单一为证），而 `P2NodeModule::plan()`
（:6407）又声明 `work_units=1`，worker 根本没有 work unit 可领；
次要原因是所有 HiPS tile 读被进程级 `aio::cfitsio_io_mutex`（`aio_hips_reader.cpp:126`）串行化。
另外还有 439 s（33%）的墙钟花在监测窗之外——`commands.cpp:1022-1030` 对 18.45 GB
artifact 的串行、冗余、纯软件 SHA-256。**
