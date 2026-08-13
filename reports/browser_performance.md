# Browser 性能（V14）

```text
GC wide 截图（含 Qt 启动）3 次：2.58 / 2.39 / 2.43 s，median 2.43s
sim-zoom 20 帧：6.0s（0.30s/帧）
sim-pan  20 帧：4.43s（0.22s/帧）
```

已实现：stretch-only redraw（STF 改变不重新采样）、tile LRU（64）、
robust Auto Global（pan/zoom 无闪烁）、--reset-stf。
交互式 UI 延迟与 10 分钟内存有界完整验证留待 GUI 轮。
