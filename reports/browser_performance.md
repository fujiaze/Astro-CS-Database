# Browser 性能（V14）

```text
GC wide 截图（含 Qt 启动）3 次：2.58 / 2.39 / 2.43 s，median 2.43s
sim-zoom 20 帧：6.0s（0.30s/帧）
sim-pan  20 帧：4.43s（0.22s/帧）
```

## STF 延迟（V14 增补，GC wide 960×720，3 次 × 10 帧）

```text
Auto STF 重算（robust median/MAD + 亮端 clip + tone-map，复用 leaves）：
  p50 median 45.79 ms（45.21 / 45.79 / 45.83），p95 median 46.47 ms
stretch-only redraw（仅 tone-map，不重新采样/FITS decode）：
  p50 median 14.40 ms（14.40 / 14.89 / 14.05），p95 median 15.49 ms
峰值内存：72 MB（3 次一致）
```

stretch-only 重绘比完整 pan 帧（约 220 ms）快约 15 倍，比 robust STF 重算快
约 3.2 倍——证明“STF 改变不重新做 sky→HEALPix→FITS decode”的设计目标达成。
证据：`evidence/performance/browser_stf_summary.json` + `browser_stf_run{1..3}.json`
（browser_cli --stf-bench）。

## STF 交互（G6）

已实现：Auto Global（默认，pan/zoom 不闪）、Auto View（--stf-mode view）、
Auto STF / Reset STF / Lock STF（菜单 Ctrl+A / Ctrl+R / Ctrl+L）、
manual black/white/midtones（面板滑块）、stretch-only redraw、tile LRU（64）、
support 图层固定显示不走 signal STF。

Lock STF 行为由 `browser_cli --stf-lock-probe` 验证 PASS：
锁定后 pan/Reset/模式切换均不重算（lo/hi 冻结、重算计数不变），解锁后恢复
重算。证据：`evidence/performance/browser_stf_lock_probe.json`。

## 未完成

- 无（G6–G7 全部证据已齐）。

## 10 分钟内存有界 soak（G6，browser_cli --soak 600）

```text
时长 600s，16,952 帧（pan/zoom + FOV 0.5°~14.75° 扫描）
RSS: 初始 72 MB → 峰值 157 MB → 结束 157 MB（median 154 MB）
tile 解码 4,688 次，LRU 淘汰 4,624 次 —— 缓存封顶后内存保持平坦
mem_bounded_pass = true
```

证据：`evidence/performance/browser_soak_600s.json`（每 5 秒采样，119 个
样本）。内存先随缓存填充升至 ~155MB，之后被 LRU（64 tile 上限）封顶，
10 分钟无单调增长。
