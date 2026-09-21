# ACCEPTANCE — RELEASE-04 验收记录

> 执行时填写。PASS 仅由前台独立验证后写入；证据给可复跑命令与路径。

## 任务状态总表

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-401 | PASS | 五份根文档与包内逐字节一致（cmp 全 OK）；无日期/任务编号/历史叙事 | 提交 344bf67f | 前台复跑一致 |
| DOC-402 | PASS | doc402_gate.py scan findings=0（基线 1829）；selftest 8/8；canon 6/6；check_no_weight_mode.py PASS（files=311） | 提交 d1ccfa1b；run/DOC-402/ | 前台复跑 scan/selftest/canon/no-weight-mode 全一致；10 项跨域项已转派 |
| DOC-403 | NOT_STARTED | | | |
| DOC-404 | PASS | check_pack_refs.py rc=0（doi 45/45、arxiv 10/10、url 24/24）；check_pack_oss_anchors.py rc=0（36/36 锚逐字命中） | 提交 dde5c4bd；run/DOC-404/ | 前台独立复跑一致 |
| SCI-401 | NOT_STARTED | | | |
| SCI-402 | NOT_STARTED | | | |
| SCI-403 | NOT_STARTED | | | |
| CLEAN-401 | NOT_STARTED | | | |
| CLEAN-402 | PASS | 工程控制/ 仅剩 RELEASE-04（find 实测 32 文件）；check_doc_index.py --strict PASS 15/15；check_doc_symbols.py 0 findings | 提交 ae0f1e63；run/CLEAN-402/ | 前台复核删除清单；12 组保留候选已裁决保留 |
| CLEAN-403 | NOT_STARTED | | | |
| FIX-401 | NOT_STARTED | | | |
| FIX-402 | NOT_STARTED | | | |
| FIX-403 | NOT_STARTED | | | |
| FIX-404 | NOT_STARTED | | | |
| FIX-405 | NOT_STARTED | | | |
| FIX-406 | NOT_STARTED | | | |
| PERF-401 | NOT_STARTED | | | |
| BLD-401 | NOT_STARTED | | | |
| E2E-401 | NOT_STARTED | | | |
| VIS-401 | NOT_STARTED | | | |
| FIN-401 | NOT_STARTED | | | |

## 发布门（ACCEPTANCE_SPEC §8）核对

| # | 门 | 结果 | 证据 |
|---|---|---|---|
| 1 | SCI-A/B/C 全部成立并审稿通过 | | |
| 2 | L1 科学性全绿 | | |
| 3 | L2 性能门通过 | | |
| 4 | L3 端到端跑通 | | |
| 5 | L4 两组视觉验收负责人确认 | | |
| 6 | P0 机器门全绿、零 waiver | | |
| 7 | 仓库整洁（清理完成） | | |
| 8 | 版本纪律（0.0.1alpha） | | |

## BLOCKED / 上呈记录

（任务 ID、原因、上呈时间、负责人裁决）
