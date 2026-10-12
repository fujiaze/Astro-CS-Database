# code —— 信号提取路径的实现

独立 Python 实现（numpy / scipy / astropy / healpy），只读 `run/LAMBDA-ROLLBACK/base/mosaic/` 的
中间产物（联合模型、逐帧已校正瓦片与方差、天光样点、覆盖与积分数组），不链接 `lib/`、不调用命令行。

| 文件 | 作用 |
|---|---|
| `p5x_io.py` | 读产物、还原逐帧原始数据（`raw = corrected + δ_k`）、叶像素几何、覆盖率与掩膜 |
| `p5x_metrics.py` | 接缝判据（逐边界有符号台阶）、帧间一致性、结构保留度、加权叠加 |
| `p5x_paths.py` | 四条信号提取路径与接缝成因归因 |
| `p5x_run.py` | 驱动：复算全部读数、写 FITS 成品与 JSON、打交付包 |
| `run_all.sh` | 一键复跑 |

复现命令见 `../docs/03_DELIVERABLES.md`。
