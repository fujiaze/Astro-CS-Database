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

## 5. 待深究：并行效率为何不达标
- **采样器读被串行化**（修复副作用）：control_sample 2T ≈ 1T，未并行。
- **UPM 稠密缓存物化（upm_persist）串行**：占 ~30% 运行时间，Amdahl 硬性限制
  理想加速约 1.5x；即便积分完美 2x 缩放也仅≈1.5x。实际积分缩放仅 ~1.13x。
- 综合：2 核真实加速 ~1.02x，与 >=1.50x 差距大。需在 UP 缓存物化/积分像素循环
  提高并行度（CON-007 并行效率）后方可复评。

## 6. 审计影响
- **CON-010 = FAIL**（并行效率）；G2 并行门禁在 2C Linux **未达标** ⇒ 禁止启动 32R。
- **CON-004（sampler 并行）需重审**：其 `init_own` 每 worker 独立句柄无法规避
  cfitsio 并发读崩溃；Linux 上 `sampler_parallel_consistency_test` 因无真实 HiPS 而
  SKIP，本次合成 6×12 首次真实触发并行读 → 崩溃（现已随 §4 修复缓解）。
- 后续：待 UPM/积分并行化提升后重测；在此之前 CON-010 维持 FAIL。

## 7. 证据（run/temp，gitignored）
- `gen_synth_frames.cpp`（生成器）、`synth_in6/`、`synth6_config.json`、`measure.py`。
- `build/linux-tsan` TSan 日志：`/tmp/con010_tsan.log`；崩溃/修复日志在 `/tmp/con010_*`。
