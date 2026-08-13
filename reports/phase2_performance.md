# Phase2 性能（V14，已完成 baseline + 热点优化）

同一机器/数据/配置，3 次 median：

```text
GC 3-panel: 292.0s -> 234.6s（-20%）
t4 overlap: 87.9s  -> 70.8s（-19%）
```

## 热点 profile（GC）

```text
upm_persist   114.3s -> 60.9s（dense 物化：逐 tile 8×8 节点表 + 数组双线性，
                           消除 5.4e8 次 std::map 查找）
control_sample 66.2s -> 62.0s（tolerance 邻域 per-tile 分组，消除 O(cells²)）
tiles_process 105.3s -> 103.6s（科学路径，未动）
```

## Science 等价

```text
C[1]/C[2]/M/signal tiles 抽查：maxdiff = 0.0（逐位等价）
```

未安全优化项：tiles_process（集成+写 tile）保持原实现；catalogue
proximity 与 UPM CG scratch 留待下一轮（当前占比低）。
