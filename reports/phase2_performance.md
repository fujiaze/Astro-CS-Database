# Phase2 性能（V14）

GC 3-panel 单次参考：288.9 s（含 UPM 全几何 44096 节点求解）。
3 次 median/p95 baseline 与 top-hotspot before/after 按用户指令延后；
候选热点（sampler 邻域 O(N²)、catalogue proximity、UPM CG scratch）已在
`docs/performance/OPTIMIZATION.md` 记录。
