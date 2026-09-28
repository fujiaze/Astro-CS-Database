# L2 性能基线：Phase2（`mosaic`）线程扩展 / RSS / I/O（PERF-401）

本目录是 PERF-401 的 L2 性能基线归档。所有数字都是**本机实测**，不是估算。

## 0. 机器指纹与关键常量

见 `machine_fingerprint.json`。要点：

| 项 | 值 |
|---|---|
| CPU | Genuine Intel(R) CPU 0000 @ 1.70GHz，16 逻辑核 |
| 内存 | 24 607 476 kB ≈ 23.5 GiB |
| 内核 | Linux 6.12.107+deb13-amd64 |
| 块设备 | `vda` 256G、`vdb` 512G（均 ROTA=1，虚拟盘） |
| **顺序写吞吐（实测）** | **113 MB/s**（`dd bs=1M count=1024 conv=fdatasync`，1 GiB / 9.49 s） |
| 代码版本 | `git rev-parse HEAD` = `bb508dea9507ddde627a97e16651ecc4ec37dd30`（+ PERF-401 工作区改动，未提交） |

## 1. 工作集

- 真实数据：`testdata/M42_T2T3_mosaic_Flying_dutchman/T2` 的 16 帧 Red；
- Phase1 产物：`run/PERF-401/work/p1_blk{1..4}/`（4 块 × 4 帧，含 HiPS signal/support/variance/ivar）；
- Phase2 输入：`run/PERF-401/p2_real16.json`（16 个 `hips_paths`）；
- 命令：`./build/astrocs mosaic --json <cfg> -y`，外层 `taskset -c 0..N-1` + `eng/ci/resource_monitor.py --gate-required --gate-workers N`。

## 2. 线程扩展（16 帧）

| workers | wall_s | cpu_avg% | 等效核 | 加速比 | peak RSS | wchar | cfitsio 取锁次数 |
|---|---|---|---|---|---|---|---|
| 1 | 923.41 | 81.08 | 0.81 | 1.00× | 5.37 GB | 16.59 GB | 0 |
| 4 | 497.93 | 228.91 | 2.29 | 1.85× | 5.56 GB | 16.59 GB | 0 |
| 16 | 389.28 | 392.44 | 3.92 | 2.37× | 6.54 GB | 16.59 GB | 0 |

**1 worker 只有 0.81 等效核** ⇒ Phase2 的墙钟由 I/O 而非 CPU 决定（写盘 16.59 GB ÷ 113 MB/s ≈ 147 s 下界）。

原始证据：`gates/real16_w{1,4,16}_gate.json`、`worker_balance/real16_w*_worker_balance.csv`、
`timeseries/real16_w*_resource_timeseries.csv`、`probes/real16_w*.probe.jsonl`。

## 3. RSS 曲线（16 worker，只变帧数）

| 帧数 | wall_s | peak RSS (MB) | MB/帧 | peak_commit (MB) | 结束 RSS (MB) | reclaim_frac | wchar (GB) |
|---|---|---|---|---|---|---|---|
| 4 | 83.25 | 2407.1 | 601.8 | 3666 | 751 | 0.688 | 5.33 |
| 8 | 171.34 | 3497.3 | 437.2 | 4494 | 1430 | 0.591 | 8.83 |
| 16 | 389.28 | 6544.1 | 409.0 | 7329 | 2793 | 0.573 | 16.59 |

帧数 4×、peak RSS 只 2.72×，**每帧 RSS 单调下降** ⇒ 工作集随块而非总帧数增长。

## 4. cfitsio 全局锁（修复前 → 修复后）

| 口径 | 修复前（锁回插 + 计数） | 修复后 |
|---|---|---|
| 16 帧 w16 wall | 379.97 s | 389.28 s |
| `cfitsio_lock.hold_ns`（被强制串行的临界区总长） | **89.48 s（= wall 的 23.5%）** | **0** |
| `cfitsio_lock.wait_ns`（线程阻塞时间总和） | **455.23 s** | **0** |
| `cfitsio_lock.acquisitions` | 205 962 | **0** |

同二进制交错 A/B（2 帧 probe，16 worker，3 轮中位数）：lock=off 48.42 s / lock=on 49.53 s（±2.2%，噪声带内）。

原始证据：`gates/real16_lock1_w16_gate.json`、`resource_summaries/real16_lock1_w16.json`、`ab2f_ab.json`。

## 5. 1/N worker 逐位一致

`real16_bitwise_verdict.json`：verdict = **PASS**。3094 个文件中 3077 个 sha256 逐字节相同；
FITS 头块差异 **0**、数据段差异 **0**；分类后的真实差异 `real_differences = {}`。
唯一被点名的非科学差异是 `p2_samples.json:/sampler_config/cpu_workers`（1 vs 16，被测参数本身的 provenance）。

## 6. Gaia 同组外部请求计数

`gaia_external_requests.json`：一次 2 帧 Phase1 全程 `strace -f -e trace=openat` → `xpsd_openat_total=20, distinct=20`
（`gaia/GaiaDR3SP` 共 20 分片，每片恰好 1 次）= **1 次数据集加载 / 同组**。

## 7. worker_balance.csv 的**已知退化**（如实登记）

`worker_balance/real16_w*_worker_balance.csv` 的 `active_workers` 与 `runnable_workers` 恒等于**已分配 worker 数**
（`lib/infrastructure/cli/commands.cpp:663,682` 调 `recorder.set_workers(budget, budget)`，两字段同源），
因此 `utilization_pct = active/(active+runnable)*100` **恒为 50.00**，无法反映真实不平衡。
**权威的实测口径是** `timeseries/*_resource_timeseries.csv`（`cpu_pct` / `runnable_workers` / `io_wait_pct`）
与 `gates/*_gate.json` 的 `queued_work_runnable_p50` / `active_threads_stat`。本目录两者都归档，避免只留退化件。

## 8. 复现命令

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASTROCS_PROBES=ON
flock /tmp/astrocs_build.lock ninja -C build

# 1 worker / 4 worker / 16 worker（16 帧）
python3 run/PERF-401/run_rest.py scale
flock /tmp/astrocs_build.lock python3 run/PERF-401/measure.py --config p2_real16 --tag real16_w16 --workers 16 --probe

# 修复前基线（锁回插 + 计数）
python3 run/PERF-401/make_v1.py && bash run/PERF-401/use_variant.sh locked_instr
python3 run/PERF-401/run_rest.py attrib
bash run/PERF-401/use_variant.sh fixed && flock /tmp/astrocs_build.lock ninja -C build

# RSS 曲线 / 逐位一致 / Gaia 计数
python3 run/PERF-401/run_rest.py rss
python3 run/PERF-401/bitwise.py run/PERF-401/out/real16_w1 run/PERF-401/out/real16_w16 real16_bitwise
python3 run/PERF-401/bitwise_classify.py
python3 run/PERF-401/gaia_count.py run/PERF-401/probe_p1_2f.json

# 汇总 / 归档
python3 run/PERF-401/summarize.py
python3 run/PERF-401/archive_l2.py
```

完整报告：`run/PERF-401/EVIDENCE.md`；回执：`run/PERF-401/RECEIPT.md`。
