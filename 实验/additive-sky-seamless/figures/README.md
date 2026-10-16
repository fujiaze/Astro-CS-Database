# P5 实测图

本目录保存 P5 小论文引用的实测图。全部图由 `make_p5_figures.py` 从本单元已落盘的机器可读证据
（`../docs/S07_results/S07_gain_main.json`、`../docs/S07_results/S07_boundary_coverage.json`、
`../docs/S03_P5_COMPONENT_evidence.exec.json`）直接绘制，不重算、不内插、不臆造；逐图与逐输入
`sha256` 见 `P5_figures_manifest.json`。未运行的图（概念图、法向边界曲线）不在本清单内，
只在论文中保留图题与待跑说明。

| 文件 | 内容 |
|---|---|
| `fig_P5_arms_rms.png` | 阴阳两世界四臂留出散度中位数（A / A+G 联合 / A+G oracle / A+G shuffle） |
| `fig_P5_negative_control.png` | 阴性对照：`A−(A+G 联合)` 的中位数与 95% 区间（无真实增益时 CI 不含 0） |
| `fig_P5_boundary_coverage.png` | 真实 196 条边界的判据覆盖与 `ctrl_shift` 依赖 |
| `fig_P5_component_overlap.png` | S9b 分量重叠图：四帧覆盖区间、两个连通分量与不可辨识间隙 |

复跑：`python3 实验/additive-sky-seamless/figures/make_p5_figures.py`
