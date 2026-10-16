# P1（photometric-magnitude）实测图

本目录保存 P1 真实帧误差预算实验**实际跑出**的图，供 `REPORT_paper.md` 的最低图表集引用。
全部曲线来自合规 8 帧（M42 M1 天区、Red、300 s）与 Gaia XP 锥的逐星读数，无合成曲线、无示意图形。
生成脚本在 `../code/s04/`（一键入口为按 `../code/s04/README.md` 列出的顺序运行 7 个脚本），
生成时的输入、seed 与逐图哈希见 `FIGURES_MANIFEST.json`。

| 文件 | 内容 | 对应判据 | 生成脚本 |
|---|---|---|---|
| `fig1_sigma_obs_per_frame.png` | 逐帧 `σ_obs` 与两套上界（实测散度 vs 已登记项合成的下包络） | P1 核心统计量的逐帧臂比较 | `s04_strata.py` |
| `fig2_residual_strata.png` | 逐星残差对参考色 `R−B`、Gaia G、径向距离、SNR 的分布与分层 | 颜色轴归因与星等/位置轴不自洽 | `s04_strata.py` |
| `fig3_selection_psf_mismatch.png` | 判据样本存活率对候选 `χ²` 中位数 | 判据样本集可比性（适用域） | `s04_strata.py` |
| `fig4_sigma_flat_sensitivity.png` | `σ_ceiling(σ_flat)` 曲线、翻转阈值与独立测量值 | `σ_flat` 假设的灵敏度（正问题） | `s04_sigmaflat.py` |
| `fig5_repeat_position.png` | 重复位置配对的 `Δd` 对 `Δflat` 与位置差 | 平场净贡献的上界 | `s04_sigmaflat.py` |
| `fig6_linearity.png` | 同一夜 600 s/300 s 通量比随信号（未饱和/饱和分离） | 探测器线性区（`NOT_MEASURED`） | `s04_linearity.py` |
| `fig7_negatives.png` | 五个负例的 `σ_obs` 与空间场阶数过拟合曲线 | 阴性/阳性对照与失败域 | `s04_negatives.py` |

`fig6_linearity.png` 只给出**已扫信号范围**内的比值趋势，不含线性区拐点；拐点在本轮不可达，
引用时不得读作「线性区已测」。
