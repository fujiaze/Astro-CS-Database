# Browser STF（V15 单状态）

## 唯一 DisplayTransformState

```text
mode        = AutoGlobal / AutoView / Manual
locked      = bool
black       = 显示空间暗部 [0,1)
white       = 显示空间亮部 (0,1]
midtones    = MTF 中点 (0,1)，0.5=线性
curve       = linear/sqrt/asinh/log
compression = asinh/log 压缩强度 [0,1]
generation  = 状态变更计数（stale 结果丢弃）
```

UI（STFPanel/STFBar/MainWindow）只编辑 state；renderer
（HipsSkyView::rasterize）只消费 state。MainWindow 的 `stf_locked_` /
`hips_auto_range_` 副本已删除（原多 owner 风险）。

## Auto Global / Auto View

- Auto Global（默认）：全 dataset 一次 robust 扫描（median/MAD 污染守卫 +
  p1/p99），进程内缓存，pan/zoom 稳定不闪；
- Auto View（可选）：viewport 自适应，debounce/worker 语义由 CLI 驱动；
- Lock：冻结当前标尺，禁止 auto/reset/模式切换重算（首次计算仍允许）。

## Support 层

固定 linear [0,1]（V15 移除 `sqrt(support)`），不走 signal STF。

## 性能

- STF 变化只做 display transform（stretch-only p50 1.41ms / p95 2.60ms），
  不重新 sky projection / HEALPix sampling / FITS decode；
- LRU 有界 tile 缓存；soak 峰值 17 MB。

## 验证

```text
test_stf_engine          : PASS
test_browser_backend     : PASS
test_hips_browser_backend: PASS（mismatch=0）
test_geometry_truth      : PASS
browser_cli --stf-manual-probe（GC view）: bright(m=0.05)=0.527
                                          dark(m=0.95)=0.009  PASS
```
