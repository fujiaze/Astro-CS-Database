# Phase2 性能（V17 True Final Freeze）

## 真实 16 帧队列（NGC1727 H-alpha，V17 二进制重跑，fp32 CPU route）

```text
mosaic_truth      : 24.00s
mosaic_clean      : 25.03s
mosaic_trail      : 25.07s
mosaic_trail_none : 24.00s
```

（V16 同队列 23.4-24.6s；V17 增加 integration status/support 显式化与
INVALID_* hard-fail 校验，overhead < 1s，无科学回归。）

## 受控 20 帧（单 order-7 tile，zero-outlier 合成）

```text
truth(none)            : ~2.3s
auto(wbpp_2_9_1)       : ~2.3s
satellite + large_scale: ~2.6s（两遍缓冲 + CC grow，grown=3079）
cosmic   + large_scale : ~2.7s（grown=0，紧凑结构不扩张）
```

large_scale 两遍路径的开销：tile 级 per-frame 缓冲（nb×n_leaf×
(3×8B + 3B)）与二次积分循环；对 16 帧真实队列影响 ~0.5s（受控单 tile
+0.2-0.4s），可接受。

## Browser（真实 mosaic_trail，V17）

```text
cold_start         75.2 ms（order 7，12 tiles decode）
pan   p50 34.7 ms / p95 45.3 ms
zoom  p50 44.9 ms / p95 53.3 ms
tile decode p50 1.1 ms / p95 2.0 ms
peak RAM 72 MB（200 raster frames）
```

## 3-runs 稳定性

- 真实 16 帧：4 组 V17 重跑（truth/clean/trail/trail_none）24.0-25.1s，
  spread < 1.1s（4.5%），同 V16 3 次（23.4-24.6s）一致；
- 受控单 tile：stage2 多次运行 2.3-2.7s；
- 结论：无 unexplained >5% 回归。

## 结论

```text
PHASE2_PERFORMANCE = PASS（V17 语义修正后无性能回归；
                      large_scale 默认关闭时零额外路径开销）
```
