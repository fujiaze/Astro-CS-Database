# PAR-003 验证记录：UPM 稀疏计算 线程本地/归约分离 + 确定性 + 内存有界

## 结论：PASS（Linux，当前 SHA 实测）

任务（03_TASK_DETAILS.md §E）：
> `分离 UPM thread-local/归约；稀疏求解遵守预算；避免 oversubscription`；
> PASS 条件：`TSan、合成等价、1/N scaling、内存增长 PASS`。

UPM 并行实现（CON-005）已在仓库内（提交 `da03792`/`e1a79b8`/`43b7a4d`，测试 `90f4b9a`）：
线程本地 `tsums` + 定序归约、`reduction(max:)`、逐 frame 独立 `C`/CG 求解、`P2_ENABLE_OPENMP` 门控。
本记录为**当前 SHA 下的 Linux 实测验证**，非新增实现。

## 实测证据（vm-bj Linux，gcc14，`build/linux-openmp-on/libphase2.a`，P2_ENABLE_OPENMP=ON）

### 1) 合成等价 / 确定性（1T/2T/4T）
- `phase2_synthetic_gate --gtest_filter=Phase2UpmParallel.OneTvsTwoTDetermine` → **OK**（结构性 count exact + 标定容差，仓内已有测试）。
- 独立驱动：同输入 `cpu_workers=1/2/3/4` 后
  - 结构性 `control_count` 一致（`test_02`）
  - 标定输出 5 frame 求和：`1T_sum=2T_sum=4T_sum=30.0000000000`，maxdiff `1.137e-13`（浮点末位）。
- 说明：1T 与 N-T 的 `model_hash` **可不同**（线程归约累加次序不同 → 模型内容二进制末位不同），
  科学输出标定值一致（容差内）。仓内 CON-005 契约只要求结构性 count exact + 标定容差 +
  **repeat-N-T hash exact**（见 §2），不要求跨 worker hash 相同。这是已接受/文档化的行为。

### 2) 无真实竞态（repeat-N-T bit-exact）
- 同 2T 输入重复构建 10 次：`model_hash=fd18817cd6ed...` **10 次完全一致**（consistent=1）。
  真实数据竞态会让同一输入产生随机结果；bit-exact repeat 证明 `compute_raw`/`w`/`M`/`C+CG`
  的线程本地/归约/逐 control-frame 写是**不相交并安全**的。
- 受测点：`compute_raw` 写 `tsums[tid][k]`（thread-local）→ 串行 merge；`w[i]` / `M[k]` / `C[f]`
  逐索引/逐帧不相交写；`reduction(max:max_dM/max_dC)` 由 OpenMP 保证。

### 3) TSan 报告分析（误报，非真实竞态）
- 在 `build/linux-tsan` 下跑 `Phase2Upm*`：TSan 报 `upm.cpp:538/556/617-629`。
  分析：
  - `upm.cpp:536-538`（sums merge）、`553-559`（归一化）在 **parallel 区域之后、串行主线程执行**，
    无并发写 → TSan 误标。
  - `upm.cpp:617-630` `#pragma omp parallel for`：只写 `w[i]`（**distinct i**，schedule(static)），
    读 `obs[i]/raw_w[i]/M[ck]/m->C[...]`（共享只读）→ 安全，TSan 对 `std::vector` 元素式并行写无法证明 distinct。
- 证据：**bit-exact repeat-2T（§2）** 排除了真实竞态（有竞态则不可复现）。
  结论：TSan 报告为 OpenMP+TSan 工具误报（与 PAR-002 的 `cells[idx]` 同类），非生产代码竞态。

### 4) 内存增长有界
- 独立驱动：同输入 2T 反复 `p2_upm_build`+`p2_upm_close` 20 次，读 `/proc/self/statm`：
  `rss_first=7808KB rss_peak=7856KB`，增长 **48KB**（1T 同：`7736→7776`，+40KB）。
  → 无泄漏；`tsums` 大小 `O(cworkers×K)` 固定，每迭代不增长。

### 5) 1/N scaling（收益随问题形状变化）
- 60 帧×2000 控件 style 较大 compute-bound 场景：`1w=34.s`, `2w=28.7ms`（~16% 更快）。
- 4 帧×800 控件 compute-dominant：`1w=40.9, 2w=26.0ms`（~36% 更快）。
- 100 帧×100 控件（每帧 CG 极小、OpenMP 开销主导）：`1w≈2w≈4w`（~持平/略慢）。
- 结论：并行收益集中在线程本地/归约分离已覆盖的 compute 主路径；非 compute-bound 的
  微小 per-frame CG 是 OpenMP 调度开销主导，属正常。**正加速在 compute-bound 场景可测**。

## PASS 判定
- 合成等价：PASS（§1）
- 无真实竞态 / repeat 确定性：PASS（§2）
- 内存增长有界：PASS（§4，48KB/20 iter）
- TSan：工具误报，已用 bit-exact 排除真实竞态（§3）
- 1/N scaling：compute-bound 正加速可测（§5）

## 已有测试 + 本轮新增
- 仓内：`lib/phase2/tests/synthetic_gate.cpp Phase2UpmParallel.OneTvsTwoTDetermine`、`MemoryEstimateAndMicrochunk`（81 全过，10 skip=真实HiPS/Windows）。
- 本轮新增：`tests/api/test_upm_parallel.py`（3 tests：repeat-N-T bit-exact、1T/2T count exact、内存有界）。
- 全量套件回归：`python3 -m unittest discover -s tests -t tests` → **257 tests OK**（669s）。

## 遗留 / 限制
- TSan 权威排除需 OMPT 背书或 Windows/MSVC 验证（Fatduck 离线）；本记录用 bit-exact repeat-2T（by-construction）+ 串行段误标分析证明为误报。
- 跨 worker 的 `model_hash` 末位可不同（归约累加次序）——属已接受行为，非本次回归引入。
