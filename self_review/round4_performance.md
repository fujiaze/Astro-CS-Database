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

## before/after 3-runs

_待 E2E runs 完成更新_（before_full_cold 运行中；after warm 队列与子集
重复随后）。

```text
ROUND4=PENDING_RUNS（Phase2/Browser 数据已定；Phase1 after 待 runs）
```
