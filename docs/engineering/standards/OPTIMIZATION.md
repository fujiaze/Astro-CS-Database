# 优化标准

上游：最高设计的 CPU 后端与资源一章；标定常数与有效宽度模型见
`../resources/PERFORMANCE_MODEL.md`。

优化只能从实测剖面出发、只改实现不改精度档位的工程正本。

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

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/resources/PERFORMANCE_MODEL.md`，同层相关正本。
