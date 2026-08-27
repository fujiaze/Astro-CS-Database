# CON-006 Stage2 integration 并行化 — 实现设计
溯源: lib/phase2/tools/stage2.cpp（1497 行）主 tile 循环。

## 已确认结构（grep 核验）
- 外层 tile 循环: `for (ci in cov.n_union_cells)`（L646）。每 union cell:
  - 读帧（aio signal/support/ivar, L527-563 打开; 循环内读 tile）；
  - `p2_upm_calibrate_block(...)`（L901）；
  - `p2_reject_stack_ex(...)`（L1059）；
  - `p2_integrate_pixel(...)`（L1262）；
  - `aio_hips_write_signal_tile(...)`（L1368）+ finalize（L1377）。
- 内层 chunk/pixel: `for (c in n_chunk)`（L802/L879/L1039）；`for (i in p0..p1)` pixel（L807），逐 pixel 独立积分写预分配输出缓冲。
- `mark("tiles_process" L1396)`; `p2_block_plan`（L505/L763）分块；`register_phase2_acr_kernels`（L603）。

## 并行设计（满足 CON-006 约束）
1. **外层 tile 并行**：并行单位 = union cell `ci`（每 cell 独立 tile，写该 tile 输出，跨 cell 无写冲突）。每 worker 独立 aio reader（sampler.cpp CON-004 已用 `SamplerReader` 模式）。
2. **单 tile 内 chunk/pixel 并行（预算约束）**：`p2_upm_calibrate_block/reject/integrate` 内 `for (i in p0..p1)` 逐 pixel 独立；用 `num_threads=effective_io_workers/cpu_workers` 并行，禁止嵌套过量线程（外层 tile 并行 + 内层 pixel 并行 = 总线程数受预算约束）。
3. **每 worker 独立 rejection scratch / source index / 统计 buffer**：禁止像素热循环内创建 vector（复用预分配 scratch）。
4. **tile 输出按稳定 tile id 提交**：写入阶段可串行但累计 <= 总计算 1%，否则改**有界 writer 队列**（producer=计算worker, consumer=固定 writer 线程；队列容量由 memory_budget_bytes + 单 tile 输出字节推导）。
5. **确定性**：每 tile 独立计算写固定 tile 槽；最终输出按稳定 tile id 序提交。逐层差分验证。
6. **门控**：`#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)` 且 worker>1；默认 OFF 串行。worker 来自 ExecutionOptions（CON-002）。

## 测试（逐层差分）
- signal/support/variance/rejection/valid-depth 图层逐像素差分：1T vs 2T（结构性计数/ID exact, 浮点按 SCI 容差）。
- 复用 synthetic_gate `Phase2Integrate*` / `Phase2Reject*` + `Phase2Upm*`；CON-010 用 5-60s 合成生产 CLI 负载做机器门禁。

## 风险与必要条件
- aio 读 per-worker reader（禁跨线程共享 fitsfile 句柄）；写用有界队列防串行化）。
- 外层 tile 并行 + 内层 pixel 并行 => 线程总数 = min(预算, 每层并发)，防嵌套过量线程。
- 反序列化 frame_id/order 保持；每 tile 固定槽写入无竞争。
## 补记：per-pixel 积分并行化（单 tile 内 chunk/pixel 并行，已确认结构）
- 逐 pixel 积分循环（stage2.cpp ~L1270-1290）：每 pixel 构建 stack/weights/support_v/acc/fid_stack/reasons（per-pixel），
  调 p2_integrate_pixel → signal/support，写 fluxD[p]/fluxF[p]/areaD[p]/areaF[p]/valid[p]（逐 p 不相交）。
- 并行单位 = per-pixel（独立写输出），但需：
  1. **per-thread scratch**：stack/weights/support_v/acc/fid_stack/reasons/frame_seq 移入并行区（每线程独立副本）；
     共享 sccfg/参考信号缓冲不可被并发 pixel 复用。
  2. **共享计数器用原子或 per-thread+定序合并**：total_rejected/reject_hist/total_fallback/underdetermined_px/
     px_integrated/dbg_reject_px/dbg_fallback_px/total_pixels。
  3. **门控**：P2_ENABLE_OPENMP 且 worker>1；默认 OFF 串行。
  4. 确定性：逐 pixel 独立 + per-thread 计数按线程序合并 → 结构性计数 exact、float 按容差。
- 遗留：外层 tile 并行 + 有界 writer 队列（aio_hips_write 线程安全未验证，先保守 per-tile 串行写）。

## 补记2：逐 pixel 循环完整共享态（已读全貌，供精确实现）
- 并行单位 = 逐 leaf/pixel p（独立写 fluxD[p]/fluxF[p]/areaD[p]/areaF[p]/valid[p]）。
- **共享 scratch（per-tile，必须 per-thread 化）**：stack/weights/support_v/acc/fid_stack/reasons/frame_seq、src_idx、ivarv、ivar_valid、buf_val/buf_w/buf_sup/buf_lo/buf_hi/buf_elig/buf_nvalid。
- **共享计数（必须 atomics 或 per-thread+定序合并）**：total_rejected、reject_hist[]、total_fallback、underdetermined_px、px_integrated、dbg_reject_px、dbg_fallback_px、dbg_zero_px、px_depth_0、total_pixels。
- **错误路径**：p2_validate_candidate_weights 失败 → log + p2_upm_close(model) + return 6。parallel 内不可 return → 需 atomic fail 标志，区后检查并统一 close+return。
- candidate-weights 校验/诊断（src_idx/ivarv/ivar_valid 映射）为 fatal 分支，需保证 parallel 下诊断正确 + 只 fail 一次。
- 建议先做（最小安全增量）：仅把 **p2_integrate_pixel 之后** 的输出写 + 计数原子化并行；否则保持串行。
