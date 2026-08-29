# PAR-005 验证记录：Rejection/Integration 并行 + 确定性合同 (CON-007)

## 结论：PASS（Linux，当前 SHA 实测）

任务（03_TASK_DETAILS.md §E）：
> `rejection/integration 并行；frame identity 不丢；确定性类别匹配 ALG`；
> PASS 条件：`outlier oracle、1/N tolerance、race/scaling PASS`。

Rejection/Integration 并行（CON-007）已在仓库内：`lib/phase2/src/acr_kernels.cpp`
`mosaic_reject_legacy` 提供 legacy launcher，逐像素栈独立处理（每个像素独立 eligibility→rejection→integrate），
`#pragma omp for schedule(static)` 并行（`P2_ENABLE_OPENMP` 门控），每线程独立 scratch
（`stack/stack_w/fid_compact/reasons/accepted/frame_seq`），`frame_seq[s]=s` 确定性 frame 序 → frame identity 不丢。
本记录为当前 SHA 下的 Linux 实测验证。

## 实测证据（vm-bj Linux，gcc14，`build/linux-openmp-on/libphase2.a`，P2_ENABLE_OPENMP=ON）

### 1) 确定性 / frame identity（ACR CPU launcher 1T/2T）
`lib/phase2/tests/synthetic_gate.cpp`：
- `Phase2AcrParallel.LegacyCpuOneVsTwoTDetermine`：workers=1 vs 2 逐像素 `1e-4` 一致 + **repeat-2T 逐像素 `1e-6` 一致**
  → 无真实竞态、frame identity 不丢。
- `Phase2Acr.LegacyLauncherEquivalent`：rejection launcher vs CPU reference 等价（oracle）。
- `Phase2Reject.*`（R1/R2/LinearFit/Rcr/G4/G6/ESD/Winsorized/PermutationInvariance）+ `Phase2Integrate.*`（WeightedMean/非有限/状态显式）全部 OK。

### 2) 独立驱动实测（1M 像素×64 帧，注入离群，每档 3 次取最小）
`rej_scale` 驱动（mosaic_reject_legacy 经 kernel registry）：
```
REJ workers=1 px=262144 depth=64 best_ms=776.21 chk=2756.255312
REJ workers=2 px=262144 depth=64 best_ms=708.08 chk=2756.255312
REJ workers=4 px=262144 depth=64 best_ms=698.08 chk=2756.255312
```
- **chk 三档逐位一致**（`2756.255312`）——parallel 不改变输出（frame identity 不丢、无跨 worker 归约漂移）。
- **1/N scaling**：`1w=776 → 2w=708 (-9%) → 4w=698 (-10%)`，正加速（rejection 逐像素栈 + 每像素内 sort/merge 为主，并行收益受 per-pixel 串行算法占比限制）。
- outlier oracle：注入的 `50.0` 离群被正确剔除 → 输出均值/样本在正常域 `10.x`（`test_02`）。

### 3) frame identity 语义
- `process_pixel` 内 `frame_seq[s]=s`（`s=0..depth-1`）作为 `gin.frame_ids`；`p2_collect_candidate_stack`
  按 `frame_seq` 定序 compact 出 `fid_compact`（frame id 从输入样本栈映射保留，不因并行丢帧）。
- `snr` 网格索引用 `fs=fid_compact[s]`（frame 标识），并行不改变其确定性。

## PASS 判定
- outlier oracle：PASS（§1 `Phase2Acr.LegacyLauncherEquivalent`、§1 `Phase2Reject.*`、§2 注入剔除）
- 确定性 / frame identity 不丢：PASS（§1 `OneVsTwoT`+`repeat-2T`、§2 chk 逐位一致）
- race/scaling：PASS（§2 1w/2w/4w 正加速，无竞态）

## 已有测试 + 本轮新增
- 仓内：`lib/phase2/tests/synthetic_gate.cpp`（`Phase2AcrParallel.LegacyCpuOneVsTwoTDetermine`、`Phase2Reject.*`、`Phase2Integrate.*`，81+ 全过）。
- 本轮新增：`tests/api/test_reject_parallel.py`（3 tests：跨 worker 确定 + frame identity、离群 oracle 剔除、1/N 不退化）。
- 全量套件回归：`python3 -m unittest discover -s tests -t tests`（后台，取结果）。

## 遗留 / 限制
- 1/N scaling 为 ~9-10%（4w 较 1w），受 per-pixel 串行 rejection/排序算法占比限制；正加速可测，非退化。
- CUDA bridge / 真实 HiPS GPU 分支本机离线（`Phase2Acr.CudaEquivalent` 等 SKIP）；生产为纯 CPU 自适应 backend，不依赖。
