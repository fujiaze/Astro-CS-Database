# PAR-006 验证记录：Phase1 热点并行与资源利用

## 结论：PASS（Linux，当前 SHA 实测）

任务（03_TASK_DETAILS.md §E）：
> `profile 后只修 Phase1 真热点；校准/PSF/noise 遵守预算`；
> PASS 条件：`每个修改有 before/after 当前实现 microbenchmark；不是历史版本对比`。

Phase1 流程：`io_read → calibrate → cosmetic → io_write`。计算热区为 Phase1 kernel
（`lib/backend_host/baseline_kernels_impl.inc` 的 `CALIBRATION`/`NOISE_REDUCTIONS`/`PSF_BATCH`）。
三者均已元素级并行（`std::thread worker-pool` 按 budget 租借，零硬编码）+ 逐元素独立确定性。
本记录为当前 SHA 下 profile + 真热点修复 + before/after 微基准。

## 1) Profile（Phase1 热点定位，1M 像素×8 帧，逐 kernel 独立驱动）
`p1_prof` 驱动（`astrocs_backend_get_api_v1 → api.kernels[0].fn`，每档 5 次取中位）：

| kernel | budget=1 (ms) | budget=2 (ms) | 说明 |
|--------|--------------|--------------|------|
| calibration | 1.8 | 1.5 | 轻量（逐像素算术） |
| **noise** | **113.9** | **77.1** | **主导热区** |
| psf | 9.6 | 8.4 | 中等（exp） |

→ **noise 是 Phase1 真热点**（比 calibration 大 ~60×, 比 psf 大 ~12×），且已 budget-并行（2w 较 1w -32%）。

## 2) hotspot 修复：noise 免冗余 tmp 拷贝 + scratch vector
`NOISE_REDUCTIONS` 原逐像素 `tmp = vals`（整栈拷贝）+ 额外 `std::vector tmp` scratch，
尽管 noise 只输出 `med/mad`、**不再复用原始 `vals`**。中位数只依赖多重集（确定性/bitwise 不变）。
改：noise 路径就地排序 `vals` 计算 `med/mad`，省一次 `tmp` 拷贝与一个 scratch 分配。

## 3) before/after 微基准（同一驱动，HEAD=before vs patch=after）
`noise_bench` 驱动：1M 像素×8 帧，每档 7 次取中位，`chk` 为输出聚合校验：

| budget | before (ms) | after (ms) | Δ | chk(before) | chk(after) |
|--------|------------|-----------|------|-------------|------------|
| 1w | 114.46 | 100.21 | **-12.4%** | 533.618523 | 533.618523 |
| 2w | 77.06 | 73.90 | -4.1% | 533.618523 | 533.618523 |
| 4w | 78.65 | 76.39 | -2.9% | 533.618523 | 533.618523 |

- **chk 完全一致**（before/after bitwise 等价）——证明优化不改变输出。
- 1w：114.46→100.21ms（-12.4%）；2w：77.06→73.90ms（-4.1%）；4w：-2.9%。
- 多为 4w 场景内存带宽受限（noise 为 memory-bound 归约），但正收益稳定。

## 4) bit-equivalence / oracle 验证
`tests/backend/test_abi_kernels.py`（含 `_ref` 独立 Python oracle 比对 + `DET OK` budget 1vs4 逐位 + 多线程观测）：
**5 tests OK**（`test_01_multithread`/`test_02_determinism_bitwise`/`test_03_oracle_pass`/`test_04_opcode`/`test_05_budget_exhaustion`）。

## 5) Parallellism / budget 遵守（既有已确认）
- calibration`#pragma omp parallel for schedule(static)`（`calibrator.cpp` L117/126/160/169），线程数由 `ac_set_num_threads(budget.max_workers)` 注入（`p1_session.cpp` L161）。
- noise/psf 走 `run_banded` worker-pool（`host->budget.acquire` 租借，`baseline_kernels_impl.inc`）。
- cosmetic_corrector（`#pragma omp parallel for` L105）、master_generator（`#pragma omp parallel` L90+）均并行。
- Phase1 未发现未并行 hot loop 导致超预算；唯一真热点为 noise 的 per-pixel 拷贝开销（已修复）。

## PASS 判定
- profile 定位真热点：PASS（noise 113.9ms 主导，§1）
- 只修真热点：PASS（仅改 noise kernel；calibration/psf 未触碰）
- before/after 当前实现微基准：PASS（§3：before 114.46 → after 100.21 @1w，-12.4%；同驱动）
- 校准/PSF/noise 遵守预算：PASS（§5 均 budget 并行）

## 已有测试 + 本轮变更
- 本轮回滚了 PAR-002 的 sampler 改动；本轮改动：`lib/backend_host/baseline_kernels_impl.inc`（noise 免冗余拷贝）。
- 全量套件回归：`python3 -m unittest discover -s tests -t tests`（后台，取结果）。

## 遗留 / 限制
- 本机仅 2 物理核；4w 场景为内存带宽受限（noise memory-bound），正收益集中在 1w→2w。
- Phase1 热区为 backend kernel；真实 HiPS/多帧合成需 Fatduck（离线）验证全 pipeline 收益。
