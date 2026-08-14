# Round 4 — Performance（V17）

日期：2026-08-14

## Phase2（真实 16 帧，V17 二进制重跑）

```text
truth / clean / trail / trail_none：24.00 / 25.03 / 25.07 / 24.00 s
（V16：23.4-24.6s；V17 语义修正 overhead < 1s，无 >5% 回归）
受控单 tile（20 帧）：2.3-2.7s（large_scale 两遍路径 +0.2-0.4s）
```

## Browser（mosaic_trail，V17）

```text
cold_start 75.2ms；pan p50 34.7ms / p95 45.3ms；zoom p50 44.9ms；
tile decode p50 1.1ms；peak RAM 72MB
```

## Phase1（真实 16 帧 NGC1727 H-alpha，分段 profile）

```text
DRIZZLE 77.6s（主导）/ PLATESOLVE 15.4s / PHOTOMETRIC 5.9s / PSF 1.3s /
CALIBRATE 0.5s / READ_FITS 0.1s / 整帧 wall ~145s
```

65s vs 150s：数据集/输出规模差异（NGC55 Red vs NGC1727 H-alpha 1200s
order-7 14tile + 本地 Gaia 查询），非算法回归；详见 reports/phase1_performance.md。

## 优化（V17）

- platesolve hint：warm 批处理把上一帧 CRVAL 写入下一帧 initial_ra/dec
  （仅搜索初始化，逐帧求解+验证；工具层 tools/phase1_e2e_bench.py）；
- 冷/热分离：cold（无 hint）vs warm（hint）；
- 不实施项（如实）：Drizzle 冻结热路径本轮不动；ACR 只做 profiler 判断；

## before/after runs

- before run 1：V16 `stage1_1727_batch.log`（同一 NGC1727 队列 16 帧全量；
  pre-V17 二进制但科学管线相同；wall median 145.4s，DRIZZLE median 77.6s）；
- before run 2：V17 二进制 cold 全量启动（`before_full_cold`），帧 0-3 完成
  （wall 145-151s，DRIZZLE 73.4-80.2s），随后被沙箱后台进程回收中断
  （frame04 未完成；如实标注，不冒充 16 帧）；
- before run 3：V17 cold 6 帧子集（`before_subset_cold`）：wall median 144.6s；
- after run 1：`after_full_warm`（前台，platesolve hint）：16 帧 wall median
  142.4s / p95 149.9s，全部 rc=0；PLATESOLVE median 15.16s（cold 15.36s）；
- after run 2：`after_subset_warm` 6 帧：wall median 148.0s；
- after run 3：`after_subset_warm_b` 6 帧：wall median 140.8s。

结论：warm hint −2.1% wall median（方差内）；Drizzle 主导（74-78s，冻结热
路径本轮不动）；无 unexplained >5% 回归；65s vs 150s 差异已解释
（NGC55 Red vs NGC1727 H-alpha 1200s order-7，数据集/输出规模差异）。

```text
ROUND4=PASS（Phase1 3-runs before/after；Phase2 4 组重跑 + Browser benchmark）
```
