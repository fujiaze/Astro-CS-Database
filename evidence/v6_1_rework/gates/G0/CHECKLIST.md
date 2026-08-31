# G0 Gate Checklist — 起点与证据完整性 (V6.1)

> 由 05_GATE_CHECKLISTS.md 复制；每项写 PASS/FAIL + evidence path + SHA-256。

| # | 检查项 | 结果 | Evidence |
|---|---|---|---|
| 1 | 控制包解包后 `CONTROL_PASS`，控制包 SHA 与任务结果一致 | PASS | 工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831 解压；`validate_control.py` → CONTROL_PASS files=47 bytes=214426 tasks=67 findings=40；zip sha256=903018212bfb584b1aaf0dd05b318d8171d1a5b3af43014ab020834e9359ede6（R0-001 TASK_RESULT） |
| 2 | `HEAD == main == origin/main`；仅使用 main；remote URL 已脱敏 | PASS | evidence/v6_1_rework/tasks/R0-001/logs/identity.log sha256=1c61e7838d392968d9031d2949b93084773068e0021a105b543697bdbd7d7721（三 SHA=30a3516...，仅 main，remote 无 token） |
| 3 | b16d422 与实际起点祖先关系已现场核实 | PASS | `git merge-base --is-ancestor b16d422... HEAD` exit=0（identity.log） |
| 4 | dirty/untracked 逐项判定来源；未知修改导致 G0 失败 | PASS | evidence/v6_1_rework/tasks/R0-001/WORKTREE_CLASSIFICATION.md — 205 项全分类，0 未知 |
| 5 | 从 CMake、public headers、registry、test registry 生成完整自有源码清单 | PASS | evidence/v6_1_rework/tasks/R0-002/SOURCE_INDEX.csv sha256=7283473dcd8f14280a03a8fe4a092a51b9d7b6bc186f7e106bedbc900401eadf（1120 文件） |
| 6 | 所有生产 target 引用路径存在；第三方、生成文件和数据单列 | PASS | TARGET_SOURCE_GRAPH.json（58 targets missing=0）；third_party_sources.csv（239 vendored 单列） |
| 7 | V6.1 ledger 原始 hash 冻结；状态机和依赖图验证通过 | PASS | 冻结台账 sha256=c6f21304d1835ca0816af600c565223cbab129ada5ce7d5a84caf3d285736822（未变）；tools/quality/validate_task_ledger.py 10 负例 selftest PASS + 工作台账 TASK_LEDGER_PASS |
| 8 | F-001 至 F-040 每项有现场 REPRODUCED/NOT_REPRODUCED 证据 | PASS | evidence/v6_1_rework/tasks/R0-004/KNOWN_FAILURES_BASELINE.json sha256=dbe859b83c88a89958f84b8f90f84b7995b823899782a8e475b0caeee9031679（40/40 REPRODUCED） |

## 结论

G0 全部 8 项 PASS。控制包完整性、仓库身份、源码清单、台账冻结与 40 项已知问题复现均满足。

创建时间：2026-08-31T04:20:00Z
