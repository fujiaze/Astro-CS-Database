# CON-010 2C Linux 运行门禁 —— 实测结论：FAIL（生产 CLI 2T 崩溃）

> 状态：CON-010 FAIL
> 判定依据：CON-010 规格要求 2T `max_threads>=2` / 计算窗口平均 CPU `>=150%` /
> `wall_1T/wall_2T>=1.50`；但本机 2 核 Linux 上 `cpu_workers=2` 生产 CLI 直接
> **SIGSEGV**，无法测量任何 2T 指标 ⇒ 门禁不满足 ⇒ FAIL（代码缺陷，非外部阻塞）。

## 1. 环境
- 主机：Intel Xeon Gold 6148 @ 2.40GHz，**2 逻辑核**，3.6Gi RAM（可用 ~1.5Gi）。
- 工具链：gcc 14.2 / clang 19.1.7；构建 `build/linux-openmp-on`
  （`P2_ENABLE_OPENMP=ON`，阶段2 OpenMP 并行）。
- 生产 CLI：`astrocs-stage2`（`run/temp/con010/synth6_config.json`）。

## 2. 合成生产 CLI 工作负载（构造）
- 因真实 2 帧面板配置（完整 rejection profile）在本机 >60s 且为真实 32R 数据，
  改用**合成多 tile 输入**：`run/temp/con010/gen_synth_frames.cpp` 生成
  N 帧 × M 叶 tile（signal/support/ivar 产品，叶级 NSIDE=512，帧间同 tile 集以形成
  UPM 重叠）。使用 `weight_mode=ivar`、`rejection=none`、`fp32`。
- 工作负载：6 帧 × 12 tile（每帧 12 个叶 tile，整块重叠）。
- 生成器编译链接 `lib/astro_image_io/astro_image_io.dll`（Linux ELF .so）。

## 3. 实测（measure.py：/proc/PID 采样，每 200ms Threads/RSS；stat utime+stime→CPU%）
| 配置 | 结果 | wall | max_threads | 平均 CPU% | peak RSS |
|---|---|---|---|---|---|
| `cpu_workers=1` | exit 0，正常出图 | 12.0s | 2（主+内部） | 82.8% | 63 MB |
| `cpu_workers=2` | **exit -11 (SIGSEGV)** | **1.00s** | 1（崩溃前） | 77.8%（崩溃前） | 10 MB |

- 1T 正常：`[stage2] HiPS mosaic written: tiles=12 pixels=3145728`，`stage2 done in 12.0s`。
- 2T 崩溃：`[sampler] read_tile_pair failed rc=1 frame=4 tile=1` 后立即 SIGSEGV。

## 4. 根因（TSan 定位 + 崩溃点）
- `build/linux-tsan`（`-fsanitize=thread`，P2_ENABLE_OPENMP=ON）跑同负载：
  ```
  WARNING: ThreadSanitizer: data race ... sampler.cpp:881 in p2_sample_controls_impl._omp_fn.0
  WARNING: ThreadSanitizer: data race ... stl_tree.h std::set::count (frames[i].tiles.count @ sampler.cpp:706)
  ERROR: ThreadSanitizer: SEGV on unknown address 0x1 ... in _IO_fread
  SUMMARY: ThreadSanitizer: SEGV in _IO_fread
  ```
- **SEGV 在 libc `_IO_fread`**：并行 worker 各自 `SamplerReader::init_own` 打开
  独立 `AioHipsDataset*`（`aio_hips_open`），但底层 cfitsio **并发文件读取不是线程安全**；
  `_IO_fread` 读到非法 FILE* 状态 → SIGSEGV（地址 0x1）。
- 并行路径结构（`sampler.cpp`：`#pragma omp parallel num_threads(workers)`，
  `rdr.init_own(hips_paths, n)` 每 worker 独立句柄，`#pragma omp for` 分发 union cell）。
  每个 cell 在 `pass1_cell` 内对 `cov_frames` 逐帧 `read_tile_pair`。
- 结论：破坏点是 **cfitsio / AIO 并发读的非线程安全**，而非 cell 写竞争或归约。
  这正是 CON-008 progress note 明确记录的约束：*AIO 读取路径在非主线程/多线程并发下
  不确定安全*；"若无法证明线程安全，异步 IO 只能作为‘有界预取队列 + 单一 IO 线程’
  形态落地"。

## 5. 对审计的影响
- **CON-010 = FAIL**：2T 生产运行崩溃，`max_threads/CPU>=150%/1T-2T>=1.50/串行段`
  全部不可测量 ⇒ G2 并行门禁在 Linux 上**未达标**。
- **CON-004（sampler 并行）需重审**：其 `init_own` 每 worker 独立句柄并不能规避
  cfitsio 并发读崩溃。之前 Linux 上 `sampler_parallel_consistency_test` 因无真实 HiPS
  而 SKIP，未实际覆盖并行多 tile 读；本次合成 6×12 实测首次真正触发并行读 → 崩溃。
- **CP2 gate 阻断**：在修复并行读线程安全前，禁止进入 G3/G4，也禁止启动 32R。

## 6. 修复方向（按 CON-008 合同）
- 采用"**单一 IO 线程 + 有界预取队列**"（`BoundedAsyncQueue`，CON-008 已落地基座）：
  worker 线程只做计算，cfitsio 调用集中在单一 IO 线程串行化，消除并发读 SEGV。
- 或证明 cfitsio/`aio_hips_*` 并发读安全（本机 GCC 14 下不成立）。

## 7. 证据文件（run/temp，gitignored）
- `run/temp/con010/gen_synth_frames.cpp`（生成器）
- `run/temp/con010/synth_in6/`（6 帧合成输入）
- `run/temp/con010/synth6_config.json`（stage2 配置）
- `run/temp/con010/measure.py`（线程/CPU/RSS 采样）
- `build/linux-tsan` TSan 运行日志：`/tmp/con010_tsan.log`
