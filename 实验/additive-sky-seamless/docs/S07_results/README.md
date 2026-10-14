# S07（P5 乘性增益研究）实测结果

本目录保存 S07 的机器可读结果，供 `REPORT_paper.md`（P5）与回传报告引用。
一键复跑入口见同单元 `code/s07_*.py`；生成本目录各件的运行清单与哈希见
[`../S07_P5_GAIN_evidence.json`](../S07_P5_GAIN_evidence.json)。

| 文件 | 内容 |
|---|---|
| `S07_wire_audit.json` | 接线审计：`k_photo·m(x,y)` 是否在生产数据流被用、MA 标量求解器调用点、`multiplicative_gain_applied` 读写点，逐条给 `file:line` 与三态判定 |
| `S07_gain_main.json` | 主条件（无帧间色差、PSF 逐帧变化）下阴性/阳性世界四臂读数 |
| `S07_gain_psf_uniform.json` | PSF 均匀条件对照 |
| `S07_gain_color.json` | 注入帧间色斜率条件对照 |
| `S07_boundary_coverage.json` | 边界覆盖审计：真实 196 条边界中 114 条有效、82 条因 `not_interior` 排除 |

**结论状态（见证据 JSON 的 `status`）为 `FAIL`**：阴性世界（无真实增益）下联合 A+G 拟合仍给出统计显著的
「改善」（CI 不含 0），而 oracle 增益给出**严格为零**的改善 ⇒ 预声明判据 C3 不成立，
**不得**默认启用乘性增益，生产默认保持恒等（`P5_multiplicative_gain = IDENTITY`）。
