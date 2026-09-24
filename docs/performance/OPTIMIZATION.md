# Optimization（真实热点，先 profile）

> 上游：ASTROCS_DESIGN.md §9（CPU 后端与资源）

候选（profile 后按 wall time 排序）：

```text
sampler tolerance 邻域：cell×all-cell → per-tile grid 索引
cross-tile adjacency：boundary pairwise → HEALPix neighbor/space bin
catalogue proximity：control×catalogue → HEALPix bucket / kd-tree
UPM CG：scratch buffer 复用，避免每步 vector 分配
Stage1/2 重复 FITS tile I/O
hierarchy 重建
browser：screen→sky→HEALPix 每像素映射；STF 变化不重新采样
```

重写依据 = 实测 profile；速度增益只从实现面获取，精度档位保持不变。
