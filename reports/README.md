# Reports 目录说明

本目录存放**报告与证据面**。历史上按版本/阶段堆积的一次性报告已于 CLEAN-402
（历史治理工件清理，2026-09-21）甄别清理：有长期价值的结论已并入正式文档
（对照表见 `run/CLEAN-402/CONCLUSION_MIGRATION.md`），过程由 git 历史承载。

## 现行保留内容（每条都必须是"活锚"，不得再堆历史批次）

| 路径 | 保留理由 |
|---|---|
| `README.md`（本文件） | 本目录的锚（`ci/impact_map.json` 路径域探针 + `ci/root_manifest.json` required_dirs），不得删除 |
| `v6/science-adjudication/`、`v6/qa-design/` | V6 产品族**活动设计档案**（`docs/{science,contracts,algorithms,validation}/v6/**`）的裁决与 QA 证据锚 |
| `v6/release-review/01_CONVERGENCE_CORRECTIONS.md`、`v6/contract-review/{03_SUPERSEDED_SCI_SECTIONS,04_OPEN_ITEMS_AND_SIGNOFF}.md` | 同上（v6 文档逐条反引号引用的收敛订正/签字记录） |
| `v6/performance/PERF-SCALE-001.md` | 活测试 `tests/monitoring/test_frozen_gate.py` 判据依据（16-worker 实测 65.09%） |
| `v6/{real-science,review-audit}/`、`v19r7_quality/` | 保留候选（未裁决的 P1/P2 闭环状态与真实科学轮结论），处置见 `run/CLEAN-402/RETAIN_CANDIDATES.md` |
| `PROJECT-GOVERNANCE-01/research/` | 六份正式科学/算法/合同文档引用的**证据锚**（R-1/R-2/R-3/R-5/MASK-001/DOC-SCI-001 等） |
| `PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md` | 退役工具/注册项的复原坐标唯一记录（4 处工具 docstring 引用） |
| `PROJECT-GOVERNANCE-01/security/EXPOSURE_NOTE.md` | 凭据暴露面处置裁决的唯一落点 |
| `RELEASE-01/science/exp_math.py` | `docs/science/NOISE_MODEL.md` 所引数值实验脚本的唯一 tracked 副本 |
| `v19r2/evidence/quality/traceability_check.json` | `ci/checks.json`（CHK-SCI-REF）登记输出，删除会使该门 `FAIL(missing_output)`；待 FIX-404 把登记改指 `run/ci/quality/traceability_check.json` 后可删 |
| `REAUDIT_V3/v3_exec/CON00{6,9,10}*.md`、`review-package-20260915/{02_待裁决清单.md,04_设计大纲综述/}` | 未决裁决/口径冲突的唯一落点，保留候选 |

## 目录规则

- 新报告一律落 `run/<task>/`（gitignore，不入库）；只有被正式文档引用为**证据锚**的内容才允许进本目录；
- 新增/删除本目录条目时，必须同步核对 `ci/impact_map.json` 的 `reports/**` 探针与
  `ci/root_manifest.json` 的 `required_dirs`（本目录不得为空目录出库）。
