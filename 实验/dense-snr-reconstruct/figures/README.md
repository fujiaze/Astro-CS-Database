# P4（dense-snr-reconstruct）实测图

本目录保存 P4 本轮**实际跑出**的图，供 `REPORT_paper.md` 的最低图表集引用。
图由 `实验/dense-snr-reconstruct/code/s06/` 的 canonical 脚本产生（一键入口
`code/s06/s06_run_all.sh`），生成时的运行清单与逐图哈希见 `S06_figures_manifest.json`。

| 文件 | 内容 | 对应判据 |
|---|---|---|
| `fig_S06_b_overshoot.png` | 节点 `[0,2,0,0]` 上的自然三次样条越界（过冲/下冲）曲线 | P4 过冲反例 |
| `fig_S06_c_domain_out_of_domain.png` | 2×2 / 3×3 控制网格的域外占比 | P4 静默外推反例 |
| `fig_S06_c_arm_var_ratio.png` | 六臂留出 CV 的方差比 `var_ratio = E_eff + 1` | P4 重建算子对照 |
| `fig_S06_c_dense_vs_cell.png` | 逐场型「cell / dense / frame」三口径对比 | P4 历史 `E_cell < E_dense < E_frame` 复核 |

无实测数据的图不放本目录；论文中此类图只保留图题与「待跑」说明。
