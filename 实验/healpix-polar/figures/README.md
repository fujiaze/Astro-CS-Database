# P3（healpix-polar）实测图

本目录保存 P3 逐 drop 完备性 `ε_j = Σ_p a_jp/A_drop,j − 1` 实验**实际跑出**的图。
全部曲线来自生产库 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` 的探针读数，
不含示意图形；生成脚本 `make_figures.py`（本目录），一键命令：

```bash
python3 实验/healpix-polar/figures/make_figures.py
```

脚本只读 `run/ACSDSDOC-02/S02/results/` 下由
`实验/healpix-polar/code/s02_build_and_run_matrix.sh` 与
`run/ACSDSDOC-02/S02/code/s02b_build_pair.sh` 产生的真实读数文件；
输入缺失时脚本报错退出，不补造数据。判定门 `|ε_j| ≤ 1e-6` 与探针口径在本轮未改动。

| 文件 | 内容 | 对应判据 | 输入 |
|---|---|---|---|
| `fig_P3_matrix_arms.png` | 单文件受控 300 格矩阵与独立并行 1500 格矩阵的三臂 `max\|ε_j\|` | 核心统计量的不同臂比较 | `s02b_matrix_cells.csv`、`docs/S02_P3_acceptance_matrix.json` |
| `fig_P3_epsilon_heatmap.png` | 几何位置误差热图（位置类 × nside，格内取 pixfrac/尺度最大） | 逐 drop 完备性的位置与分辨率依赖 | `s02b_matrix_cells.csv` |
| `fig_P3_negative_control.png` | 注入缺陷臂的 `\|ε_j\|` 分布（对默认路径） | 阴性/阳性对照与失败域 | `s02b_matrix_cells.csv` |
| `fig_P3_domain_scale.png` | `\|ε_j\|` 对 drop 尺度（0.02″–3600″，nside 8192、pixfrac 0.8） | 适用域与门可达边界 | `s02b_theta_{pre,post}.json` |

读图要点：热图中 `leaf interior`、`face seam`、`pole` 三类在两个臂上都不破门，
破门只出现在 `leaf edge mid` 与 `face vertex` 两类；适用域曲线的 290″ 峰（`3.29e-7`）来自
叶四角弦表示与切平面/球面面积分支**切换**，该分支项在本轮**未修**，峰值仍低于门。
