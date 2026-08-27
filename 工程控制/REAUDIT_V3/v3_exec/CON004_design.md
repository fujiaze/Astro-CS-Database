# CON-004 Sampler 并行化 — 实现设计（基于已确认源码结构）
溯源: lib/phase2/src/sampler.cpp（1133 行），p2_sample_controls_impl 主循环。

## 已确认结构（grep/read 核验）
- 主循环: `for (c in n_union)`（L622 起）: 每 `c` 一个唯一 `tile_ipix`（coverage->union_cells[c].ipix），独立。）
- 帧集: `cov_frames` = 覆盖该 tile 的帧集合（frames[i].tiles.count(tile_ipix)）。
- 每 cell: `read_tile_pair(sig[f], sup[f], tile_ipix, &pairs[fi])` 读 signal+support tile。
- 子 cell: `for gy in grid` `for gx in grid` 计算 patch 统计，写入 `cells[c*grid*grid + gy*grid + gx]`（预分配稳定槽，无跨 cell 写冲突）。
- 帧级 `sig[]/sup[]/ivr[]` 为共享 AioHipsDataset 句柄（L496-530 打开，函数尾关闭）。

## 并行设计（满足 CON-004 约束）
1. 并行单位 = union cell `c`（互不共享槽位/计算）。
2. **每 worker 独立 reader**：构造 `ParallelWorker` 持有自己的 `AioHipsDataset*`（按需 open/close / 私有 LRU 仅缓存本 worker 读过的 tile）。禁止跨线程共享相同 fitsfile 句柄——cfitsio 同句柄并发读非线程安全（V2 结论: AIO 读为同步单线程 cfitsio）。
3. **无全局 critical(aio_read)**：读路径完全私有；主计算不因锁串行化。
4. **结果进预分配稳定槽**：`cells[idx]` 按 c 分块，worker 写自己的 c 范围；无竞争。
5. **固定 key 排序**：cell 循环结束后，现有 keyed sort（按 ra/dec/leaf/order）保持确定序；1T/2T 排序键相同 => 相同顺序。
6. **确定性**：每 cell 独立、读独立、写固定槽 => 逐 cell 结果与 1T 完全一致（逐层，非仅图相同）。
7. **切分**：`cpu_workers` 来自 ExecutionOptions（CON-002），`omp parallel for num_threads(effective_cpu_workers)`，仅在 `#ifdef P2_ENABLE_OPENMP` 且 `cpu_workers>1` 时启用；否则串行回退（默认 P2_ENABLE_OPENMP=OFF）。

## 测试（1T/2T 确定性 + 计数/ID/hash）
- 新增 `sample_parallel_consistency`：同一合成输入分别 1T / 2T / 重复 2T 运行 `p2_sample_controls_impl`。
- 断言: accept/reject 计数、frame ID 顺序、control 顺序、out_controls hash **exact**；接受/拒绝 reason 分布、snr/sup 数组按 SCI 容差。
- 加入 `lib/phase2/CMakeLists.txt` 测试目标注册。

## 风险与必要条件
- 必须保持 P2_ENABLE_OPENMP=OFF 默认 => 默认仍串行（安全网），仅在显式开启 + cpu_workers>1 并行。
- per-worker dataset 生命周期（open/close/异常）必须齐全，避免泄漏。
- tile 级复用从‘全局 cache’降为‘每 worker LRU’：多 worker 可能重复读，属可接受权衡（compute 为瓶颈）。
- 完成前先跑 synthetic_gate Phase2Sampler* 基线（验证串行回退不改变现有结果）。
