# docs —— 本单元的支撑推导

本目录放**推导正文**：接缝门与天光面判据的闭式推导、以及跨单元裁决在本单元的落点。
它们不是实验结果（结果与固化读数在 `../results/`），也不是复现入口
（入口见 `../README.md` §7）。

| 文件 | 内容 | 支撑对象 | 复现腿 |
|---|---|---|---|
| `DISPUTES.md` | 本单元逐单元裁决台账（A-P5-01…A-P5-12）：冲突、终裁、依据 | `../REPORT_paper.md` 全文、`../README.md` 单元构成 | 依据面为 `../results/audit_rework/**` 与 `../results/audit_rework/p3_kcorr/` |
| `seam-gate-floor.md` | 接缝门 1e-2 的确定性下限 1.0050%（闭式 `gate/(1−gate/2)`）、统计行为、三个结构性失效面（平滑过渡、反号梯度相消、光滑法向斜坡伪阳）、`rel_step_max=0.1` 的除名依据 | `../REPORT_paper.md` §2.3、`../docs/seam-gate-floor.md` 自身的实验闭合表 | `../code/audit_rework/route1/c3_seam_gate.py`、`../code/audit_rework/route2/e3_seam_gate_1e-2.py`、`../code/seam_gate_gradient_scan.py` |
| `smooth-lambda.md` | `smoothing_lambda` 实验的**判据预登记与世界构造**（量定义、场景矩阵、λs 网格）。**其 §0 结论速览与 §3–§9 的结果段仍是未回填的占位符（`{{TABLES}}` 等），本文件不承载任何定案结论**；占位状态由 `../results/REVERSE_VERIFY_CANON.md` §1 逐项登记 | `../code/reverse_verify/smooth_lambda/CRITERIA.md`（判据预登记） | `../code/reverse_verify/smooth_lambda/run_mosaic_ls.sh`（需 49 帧 L4 产品树，该树已回收 ⇒ 不可跑） |
| `control-variance-adjudication.md` | 控制点定权公式的三口径标定与 `k_corr` 两因子查表的引用义务 | `../REPORT_paper.md` §2.2、`../code/audit_rework/supp_control_variance/` | `../code/audit_rework/supp_control_variance/*.py` |

本目录不设独立复现入口：每个文件的「复现腿」列给出对应的可跑脚本，
由 `../README.md` §7 的统一入口调度。
