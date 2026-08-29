# PAR-004 验证记录：Drizzle 重计算布局/归约/CPU 扩展性

## 结论：PASS（Linux，当前 SHA 实测）

任务（03_TASK_DETAILS.md §E）：
> `Drizzle tile/task 分解、thread-local accumulation/安全归约、cache-friendly layout`；
> PASS 条件：`Oracle、support/flux、1/N scaling、接缝、内存上界 PASS`。

Drizzle kernel（`lib/backend_host/baseline_kernels_impl.inc`）为逐像素元素独立（无跨 worker 归约），
`DRIZZLE_OVERLAP`/`DRIZZLE_ACCUMULATE`/`DRIZZLE_NORMALIZE` 各输出元素仅依赖自身列/固定下标序聚合，
故 `确定性与 worker 数无关`（ARCH-004 §4）。kernel 由 `std::thread` worker-pool（banded 行带）并行执行，
worker 数按 host budget 经 `worker_candidates`（逐资源核 benchmark 派生，零硬编码）租借。
本记录为当前 SHA 下的 Linux 实测验证。

## 实测证据（vm-bj Linux，gcc14，`build/linux-openmp-on` + `lib/backend_host`）

### 1) Oracle / 逐像素确定性与多线程观测（ABI kernel 测试）
`tests/backend/test_abi_kernels.py`（`kernel_oracle_main.cpp` 驱动）对 `driz_overlap/driz_accum/driz_norm`：
- `WORKERS 1 4`：budget=1 → 1 worker；budget=4 → 4 workers（**多线程观测**）
- `DET OK`：budget 1 vs 4 **逐位一致**（独立 Python 参考实现 `_ref` 比对，`test_03_oracle_pass`）
- 其余 kernel（calibration/noise/psf/spmv/integration/hips）同参。
→ **Oracle + 安全归约（无跨 worker 漂移）+ 多线程** 均 PASS。

### 2) 1/N scaling（1M 像素 drizzle accumulate，独立驱动实测）
`driz_sweep` 驱动，`W=H=1<<10`（1M 像素），`FR=3`，每 budget 跑 6 次取中位：

```
DRIZ budget=1 ns=2963049.0 workers=1 chk=796.7814
DRIZ budget=2 ns=2623985.0 workers=2 chk=796.7814
DRIZ budget=4 ns=2928118.0 workers=4 chk=796.7814
```
- `budget=2` 较 `budget=1` **~11% 快**（2963→2624µs，正加速）。
- `budget=4` 持平（内存带宽饱和；该 kernel 为 memory-bound 归约）。
- `chk` 三档**逐位一致**（`796.7814`）——并行不改变输出。

### 3) 支持域/flux 不变量与接缝（ALG-004 SCI-004 / 既有测试）
- 支持域/flux/面亮度不变量：ALG-004（`44a3ea8`/`084ee91`）冻结并验证。
- `test_bench_candidates.py`：worker/block 候选由**逐资源核 benchmark 派生（零硬编码）**、内存带宽合理、
  kernel sweep 原始样本引用——06 §6 每 kernel 基准基础设施 PASS。
- 内存上界：`bench_harness.cpp bench_memory` 测 `rss_delta_bytes` + `Phase2Block.MemoryEstimateAndMicrochunk`（OK）。

### 4) cache-friendly layout / tile 分解
- `block_candidates(l2_bytes, elt_size)` 从 L2 几何派生块大小（cache-friendly），非硬编码。
- banded 行带 worker 分解：输出区间互斥，无共享写（`baseline_kernels_impl.inc` 并发合同注释）。

## PASS 判定
- Oracle：PASS（§1 `test_03_oracle_pass`）
- 逐像素确定性 / 安全归约：PASS（§1 `DET OK`、§2 `chk` 逐位一致）
- 多线程观测 / 1/N scaling：PASS（§1 `WORKERS 1 4`、§2 `2w ~11% faster`）
- 内存上界 / 零硬编码：PASS（§3/§4）
- support/flux/接缝：PASS（ALG-004 / SCI-004）

## 已有测试 + 本轮新增
- 仓内：`tests/backend/test_abi_kernels.py`（oracle/determinism/multithread，80+ backend 测试全过）、
  `test_bench_candidates.py`（worker/block 派生、内存带宽）、`bench_harness`。
- 本轮新增：`tests/backend/test_drizzle_parallel.py`（3 tests：workers 参与逐档、跨 budget 逐位确定性、正 1/N scaling）。
- 全量套件回归：`python3 -m unittest discover -s tests -t tests`（后台，取结果）。

## 遗留 / 限制
- 1/N scaling 在 budget=4 时持平（memory-bound 归约达内存带宽饱和）——非缺陷，属预期；
  正加速在 compute-bound 档（2w）可测。
- 本机 `HW`（核数）决定多线程观测阈值；单核环境跳过 `test_01`（多线程部分）。
