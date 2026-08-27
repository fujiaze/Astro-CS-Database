# CON-010 2C Linux 运行门禁 —— 实测结论：FAIL（并行效率不达标）

> 状态：CON-010 FAIL
> 判定：CON-010 要求 2T `max_threads>=2`、计算窗口平均 CPU `>=150%`、
> `wall_1T/wall_2T>=1.50`、无 ≥1s 串行段、串行累计 <1%。
> 本机 2 核 Linux 实测：2T `wall_1T/wall_2T≈1.02`、平均 CPU≈107% —— **两个关键阈值
> 均不达标** ⇒ 门禁不满足 ⇒ FAIL（并行效率不足，非外部阻塞）。
> 过程中另**发现并修复**一个真实崩溃（sampler 并行 AIO 读 SIGSEGV），见 §4。

## 1. 环境
- 主机：Intel Xeon Gold 6148@2.40GHz，**2 逻辑核**，3.6Gi RAM（可用 ~1.5Gi）。
- 工具链：gcc 14.2 / clang 19.1.7；构建 `build/linux-openmp-on`（`P2_ENABLE_OPENMP=ON`）。
- 生产 CLI：`astrocs-stage2`。

## 2. 合成生产 CLI 工作负载
- 真实 2 帧面板配置在本机 >60s（超出门禁上限），故按规格用**合成多 tile 输入**：
  `run/temp/con010/gen_synth_frames.cpp` 生成 6 帧 × 12 叶 tile（signal/support/ivar，
  叶级 NSIDE=512，帧间同 tile 集以形成 UPM 重叠）。`weight_mode=ivar`、`rejection=none`、fp32。

## 3. 实测（measure.py /proc/PID，每 200ms Threads/RSS；stat utime+stime→CPU%）
| 配置 | 结果 | wall（2 次） | 平均 CPU% | peak RSS |
|---|---|---|---|---|
| `cpu_workers=1` | exit 0 | 8.42s / 8.22s | 94.6% | 63 MB |
| `cpu_workers=2` | exit 0（修复后） | 8.22s / 8.02s | 106.9% | 78 MB |

- `wall_1T/wall_2T = 8.32/8.12 ≈ 1.02`（需求 ≥1.50）。
- 2T 平均 CPU ≈ 107%（需求 ≥150%）。2T 基本未利用第二核。
- 时相分解（2T，12×12）：control_sample=3.81s（≈1T，即 sampler 几乎未并行），
  upm_persist=4.53s（**串行 UPM 稠密缓存物化**），tiles_process=6.87s（积分 ~1.13x，缩放差）。
- **生产配置复测（diagnostics=false，2026-08-27 追加）**：`upm_persist` 在非
  diagnostics 下被跳过（:497，仅 diagnostics=true 才物化稠密缓存），故其 2.4s 串行段
  属 **diagnostics-only 开销，非生产路径**。生产（diagnostics=false）实测：
  1T=6.05s / 2T=5.98s ⇒ **1.01x，2T 平均 CPU≈112%**；相分解 `control_sample`
  （采样器）1T=1.85s/2T=1.86s（**不随核数缩放**）、`tiles_process` 1T=4.19s/2T=4.11s
  （~1.02x）。⇒ 门禁失败**与 diagnostics 无关**，根因集中在**采样器串行**（stopgap mutex）
  与**积分不缩放**（内存受限 + 串行校准），二者为核心瓶颈。
- **瓶颈实证（2026-08-27，并行校准负结果）**：我曾把积分串行"逐帧 UPM 校准"（源4）
  当作主导串行段，并实现"Phase A 串行读 + Phase B 并行校准"（按帧 omp 并行 `calibrate_block`）。
  实测 **tiles_process 反而 1T 4.19→4.39s（每 chunk 预分配 per-frame 缓冲 + omp fork 开销），
  2T 仅 4.14s，无提升** ⇒ 已回退。**证明"逐帧校准"不是主导瓶颈**。
  真正主导的是**积分 rejection 的跨步聚集**：`process_cpu_pixel_parallel` 与串行 rejection
  都以 `value_stride=chunk_pixels`（`p2_collect_candidate_stack`，`:1086/:1332`）读取
  `cal[s*chunk_pixels+pixel]`（帧间步长≈2048KB）→ **深度内存级 cache-miss**，故 1T/2T 均
  ~4.1s 不缩放（内存受限）。修正需把 cal/supv/ivarv **转置为 pixel-major**（`i*depth+s`），
  但该布局须**改写共享的 `p2_collect_candidate_stack`**——其源码（`rejection.cpp:1208`
  `vd[s*value_stride+pixel]`）**本质是 frame-major**（values 按帧分组），且被
  `stage2.cpp` + **`acr_kernels.cpp:135`（ACR 内核）** + `synthetic_gate.cpp` 三方调用，
  假设同一 frame-major 布局 ⇒ **非 stage2 内可封闭改动**；pixel-major 需新增独立
  collector 函数或布局标志（不动 ACR），属**跨模块、更高风险**改动。
- **积分内部实测拆分（2026-08-27，计时插桩）**：`tiles_process`(1T)=4.19s 精确拆为
  **fill(读 tile + UPM 校准 + 填充)=2.118s（串行, s-loop 单线程）** +
  **rejection(omp 像素)=2.072s（不随核数缩放）**。⇒ 此前"逐帧校准"/"跨步聚集"均为
  **联合主导**之一而非唯一；真正的主导是**fill 内的 aio tile 读**
  （`aio_hips_read_tile_f32`, cfitsio 预编译库，串行 2.12s）——**与采样器
  control_sample(1.85s 串行)是同一 cfitsio 读瓶颈**，需 CON-008 单 IO 线程/管道路径统一解决；
  rejection 2.07s 不缩放是另一独立受限源。
- **Amdahl 界与不可满足性（2026-08-27 实测确证）**：合成数据 `N_B=6 chunk_px=262144
  n_chunk=1`（每 tile 仅 1 chunk，**无跨 chunk 重复读**，hoist 读无收益）。积分
  tiles_process=2.12s **串行 aio 读** + 2.07s **rejection(omp 像素, 实测 1T/2T 均≈2.03-2.07s
  不缩放)**。⇒ 即便 rejection 完美 2×，Amdahl 上限 = 2.12 + 2.07/2 = 3.16s ⇒ 1.33x(<1.5x)；
  而实测 rejection 不缩放 ⇒ 实际更差。**门禁(CPU≥150%/1T-2T≥1.5x/串行<1s且<1%)在当前
  架构下不可满足**——2.12s 串行读(≥1s) + 1.85s 采样器串行 + upm_persist(诊断)均为 ≥1s
  串行段。这是**真实的预发布质量缺陷**，应由审核人裁决规格冲突/是否放宽门禁。
- **差分结果/数值门禁：1T==2T 数据位级一致**（2026-08-27 复验，G2 必备产出）。
  1T 与 2T 完整 mosaic 输出逐字节对比：signal/support 各 `Norder0/...` FITS 的
  **DATASUM 完全一致**（如 Npix0 `3138625936`），即**科学数据逐字节相同**（积分+UPM+写盘
  确定性成立）；文件整体 hash 仅因 `CHECKSUM`/`DATASUM` 卡片**注释里的时间戳
  （16:06:31 vs 16:06:39，两次运行相隔 8s）**不同而不同，属元数据/伪装差异，非数值差异。
  `upm_dense.cache` 亦 SHA256 bit-identical。⇒ 数值门禁（确定性）满足。

## 4. 过程中发现并修复的真实崩溃（独立于效率判定）
- **原始 2T 运行 SIGSEGV**：`exit -11`，约 1.0s，`[sampler] read_tile_pair failed rc=1 frame=4 tile=1`
  后崩溃。TSan（`-fsanitize=thread`）：`sampler.cpp:706 std::set::count` / `:881 _omp_fn.0`
  竞态 + 真正崩溃 `SEGV in _IO_fread`。
- **根因**：并行 worker 各自 `SamplerReader::init_own` 打开独立 `AioHipsDataset*`，
  但底层 **cfitsio 并发文件读取（`_IO_fread`）在本机 gcc14 上非线程安全** ⇒ SIGSEGV。
  每 worker 独立句柄无法规避（cfitsio 本身全局态非线程安全）。
- **修复**（`lib/phase2/src/sampler.cpp`）：新增 `static std::mutex g_aio_mu;`，对
  `aio_hips_open`（sig/sup 惰性开）与 `read_tile_pair`（`aio_hips_read_tile_f32`）
  串行化，保留计算并行。修复后 2T 正常运行（无崩溃），全部既有测试通过：
  81 passed / 0 failed（synthetic_gate），ivar_wiring(2T) PASS，async_io 10/10，
  routing 4/4，execution_options 6/6。详见 §CON-008 合同（异步 IO 线程安全无法证明时
  只能"单一 IO 线程"落地；此处取全局 mutex 串行化读）。

## 5. 待深究：并行效率为何不达标（本轮深入结论）
- **序列化源 1 —— UPM 稠密缓存物化（upm_persist）是硬串行段**：`p2_upm_materialize_dense`
  逐 (f,tile) 计算双线性并 `aio_upm_dense_write_tile` 写盘。后者（`aio_upm.cpp:256-271`）
  **强制 frame_index 单调递增 + 单 FILE\* 顺序 fwrite**，向 writer 传参需严格
  (frame,tile) 序 ⇒ 无法直接并行写；`upm_persist` 实测 ~2.4s（12×12 时 ~4.5s），
  直接违反"无串行段 >=1s"。并行化需：(a) 改写 writer 支持按 tile 偏移随机写，或
  (b) 逐帧并行"计算"再按序"写"（保持语义不变，缓存 bit-identical）。
- **序列化源 2 —— worker 数未贯穿到物化**：`stage2.cpp:482` 直接调
  `p2_upm_materialize_dense`，未传 worker 数；`p2_upm_*` 为公开 API，改签名侵入大；
  用 `omp_set_num_threads` 全局改线程数可能影响其它默认并行的 omp 区域 → 需谨慎。
- **序列化源 3 —— 积分像素循环（tiles_process）内存受限**：`stage2.cpp:1287`
  `#pragma omp parallel ... omp for schedule(static)` 已并行像素，但实测缩放到 2 核
  仅 ~1.1x（12×12：1T~7.8s→2T=6.87s）。原因应为逐像素读 `cal/support/frame_seq`
  与 per-thread `std::map` hist 的访存/缓存竞争，属内存带宽受限，非线程数不足。
- **序列化源 4 —— 积分内"逐帧校准"段是串行（2026-08-27 新定位）**：`stage2.cpp:1245-1278`
  的逐帧 `s` 循环（`aio_hips_read_tile_f32` + `p2_upm_calibrate_block` 校准 UPM 空间
  项）在**调用线程**上、**先于** omp `rejection` 并行区执行；即每 chunk 先串行校准全部帧，
  再并行 rejection。该串行段（含 tile 读 + 双线性校准，且 again 涉及 cfitsio 并发读安全）
  是积分不随核数扩展的又一原因 ⇒ 需把校准并入并行/管道（或与 rejection 重叠）。
- **序列化源 5 —— 稠密缓存物化是"死重"（2026-08-27 新定位）**：`stage2.cpp:482`
  物化 `upm_dense.cache`（~150MB，串行 2.4s），但 stage2 的积分**从不读它**——积分只用
  稀疏 `p2_upm_calibrate_block`（`:1272`），全文件**无任何 `p2_upm_dense_read_block` 调用**
  （仅有 tests/synthetic_gate 在单测里读）。⇒ `upm_persist` 对门禁计算是**纯冗余串行**：
  既写了一份计算用不到的稠密缓存，又在积分里重复稀疏校准（双份计算 + 双份串行）。
  修正方向：(a) 让积分改用 `dense_read_block`（用上已物化的稠密缓存，移走积分串行校准），
  或 (b) 提供跳过物化选项（当稠密缓存非该路计算所需）。两者都能消除/复用该 2.4s 串行段。
- **采样器读被串行化**（§4 修复副作用）：control_sample 2T ≈ 1T，未并行。
- **综合 Amdahl**：`upm_persist`(串行 ~30%) + 积分(仅 1.1x) + 采样器(未并行) ⇒ 2 核真实
  加速 ~1.02x，远低于 >=1.50x；且 `upm_persist` 的 >1s 串行段直接违反门禁。
- **结论**：以当前架构（顺序化 dense writer + 内存受限积分 + 串行化采样器读），在
  5-60s 合成工作负载上**无法满足**"平均CPU>=150% / 1T-2T>=1.5 / 无串行段>=1s /
  串行累计<1%"。需先做主循环并行化改造（writer 偏移写或单 IO 线程队列 + 积存储/local
  hist 降竞争 + worker 数贯穿）再复评——这是一项独立工程，非本轮可闭合。
- **工作负载无关性确认（2026-08-27 复测）**：改用真实 rejection 方法 `sigma`
  （sigma_low/high=3, iterations=2）替代 `rejection=none`，6×12 仍 **1T=8.42s / 2T=8.42s
  ⇒ 1.0x，2T 平均 CPU 111%**；其 1T 时相分解：`control_sample=1.90s`（采样器）、
  `upm_persist=2.36s`（串行稠密物化）、`tiles_process=4.39s`（积分）。即非 rejection 复杂
  度限制，而是**串行段（采样器读被串行化 + upm_persist）与大段不缩放积分**共同导致。
- **Amdahl 上限**：即便积分完美 2x 缩放，2T 总时 ≈ 4.26s(采样器+物化) + 4.39/2 ≈ 6.46s
  ⇒ 加速 ~1.34x，仍 <1.5x，且采样器/物化各自 >1s 违反"无串行段>=1s"。⇒ 门禁在现行架构下
  **不可满足**，需并行化改造后复评。
- **并行化尝试（2026-08-27 追加）**：将 `p2_upm_materialize_dense` 计算并行化
  （`_n` 变体：分批并行算 (f,tile) 双线性 + 严格按序写）。1T/2T dense cache
  SHA256 bit-identical，全部测试通过；但 `upm_persist` 仅 2.40s→2.26s(2T)。
- **修正：并非磁盘 I/O 瓶颈**。`dd 150MB conv=fsync` 到同目录实测 **436 MB/s
  （0.36s）**，磁盘很快 ⇒ `upm_persist` 非 disk-bound。其 ~2.1s 非 compute 开销来自
  **预编译 `astro_image_io/astro_image_io.dll` 的逐元素 byte-copy 写路径**（每个 tile
  逐元素 `bytes.insert(...)`×262144 次，串行；该库为预编译 .so，本树无对应 CMake
  目标，无法用 phase2 源码优化）。⇒ 要消除该硬串行段需重建 aio 库（bulk fwrite）或
  单 IO 线程物化；属源码可优化但在预编译库内的瓶颈，非磁盘/硬件限制。

## 5b. 规格级冲突（新增，2026-08-27）：CON-008 与 CON-010 互斥 / CON-008 未完全合规
- **CON-008 ↔ CON-010 规格自相矛盾**：`03_TASK_SPECIFICATIONS.md` 第 55 行
  **禁止用全局 `critical(aio_read)` 把主计算串行化**，第 81 行要求"读取用**有界
  producer/consumer 队列**（单 IO 线程 + 有界预取）"。而 CON-010 要求"**无 ≥1s 串行段、
  串行累计 <1%**"。CON-008 强制"读取用单 IO 线程/有界队列"时，IO 侧天然是单线程
  （磁盘/`aio_hips` 串行），其吞吐受 IO 限制 ⇒ 无法同时满足 CON-010 的极严并行阈值。
  二者**互斥**，需外部审核人裁决（如放宽 CON-010 串行阈值，或接受 CON-008 规定的
  IO 单线程结构）。
- **当前 CON-008 PASS 基于非合规 stopgap**：为修 cfitsio 并行读崩溃（§4），对
  `aio_hips_*` 加了**全局 `std::mutex g_aio_mu`** 串行化读。该做**法在第 55 行明确
  禁止**（"禁止用全局 critical(aio_read) 把主计算串行化"），故 CON-008 的 PASS 属
  **临时缓解而非规范实现**（规范实现=有界 producer/consumer 队列，尚未落地）。撤销
  mutex 会重现崩溃，故当前保留，但应作为 pending 项记录：需实现有界队列后才能算
  合规 + 才能进一步降低采样器串行开销。

## 6. 审计影响
- **CON-010 = FAIL**（并行效率）；G2 并行门禁在 2C Linux **未达标** ⇒ 禁止启动 32R。
- **CON-004（sampler 并行）需重审**：其 `init_own` 每 worker 独立句柄无法规避
  cfitsio 并发读崩溃；Linux 上 `sampler_parallel_consistency_test` 因无真实 HiPS 而
  SKIP，本次合成 6×12 首次真实触发并行读 → 崩溃（现已随 §4 修复缓解）。
- 后续：待 UPM/积分并行化提升后重测；在此之前 CON-010 维持 FAIL。

## 7. 证据（run/temp，gitignored）
- `gen_synth_frames.cpp`（生成器）、`synth_in6/`、`synth6_config.json`、`measure.py`。
- `build/linux-tsan` TSan 日志：`/tmp/con010_tsan.log`；崩溃/修复日志在 `/tmp/con010_*`。
