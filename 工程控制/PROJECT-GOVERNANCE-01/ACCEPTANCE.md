# 验收记录（ACCEPTANCE）

> 本包尚未执行。`PASS` 只可由前台（调度员）独立复跑后填写（见 `OPERATOR.md §3`）。
> 填写要求：证据路径必须指向本次复跑产生的日志或报告；不得复用执行 Agent 的自述。

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| BASE-001 | NOT_STARTED | — | — | — |
| GOV-001 | NOT_STARTED | — | — | — |
| DOC-001 | NOT_STARTED | — | — | — |
| DATA-001 | NOT_STARTED | — | — | — |
| CFG-001 | NOT_STARTED | — | — | — |
| MOD-001 | PASS | 门1 23/23 唯一行 rc=0；门2 --selftest PASS(8 cases) + 五类负例 rc≠0（含 facade/no-op）；门3 tests/quality 失败集合 ⊆ 冻结基线（基线 3 条仍在、skip 集合逐条相同、无新增归因本任务；新增 test_05 独立复现归因 build/ 清运）；门4 正向状态 0（NOT_IMPLEMENTED 22 / CONTRACT_READY 1，真实仓库诚实红灯 rc=1） | run/PROJECT-GOVERNANCE-01/MOD-001/verify-logs/（独立复跑）+ run/PROJECT-GOVERNANCE-01/MOD-001/logs/（执行）；冻结基线 run/PROJECT-GOVERNANCE-01/BOUNDARY-w1/quality-baseline.txt（sha256 2a373529…） | 独立验证 Agent 5/5 门 PASS（锚定 HEAD=900916fb），调度员抽查一致（HEAD=b46a316f）：23 行/23 唯一、checker rc=1 且正向状态 0、ci/checks.json 0 删除且 CHK-ROOT-CLEAN 完整、越界=0（足迹=4 新文件 + 1 条登记项）；遗留：① 该登记项误随 ROOT-002 的 e5fba371 入仓，前台将做归属修正提交；② ci/known_failures.json 未登记本门当前红灯（CI-001 域，需负责人定豁免口径）；③ 映射表 7 个旧 DATA-* ID 待 DATA-001 补 canonical 映射 |
| ARCH-001 | NOT_STARTED | — | — | — |
| AIO-001 | NOT_STARTED | — | — | — |
| RT-001 | NOT_STARTED | — | — | — |
| CLI-001 | NOT_STARTED | — | — | — |
| CLI-002 | NOT_STARTED | — | — | — |
| CLI-003 | NOT_STARTED | — | — | — |
| P1-001 | NOT_STARTED | — | — | — |
| P1-002 | NOT_STARTED | — | — | — |
| P2-001 | NOT_STARTED | — | — | — |
| P2-002 | NOT_STARTED | — | — | — |
| P3-001 | NOT_STARTED | — | — | — |
| P3-002 | NOT_STARTED | — | — | — |
| CPU-001 | NOT_STARTED | — | — | — |
| OBS-001 | NOT_STARTED | — | — | — |
| INT-001 | NOT_STARTED | — | — | — |
| CI-001 | NOT_STARTED | — | — | — |
| QA-001 | NOT_STARTED | — | — | — |
| PKG-001 | NOT_STARTED | — | — | — |
| REAL-001 | NOT_STARTED | — | — | — |
| FINAL-001 | NOT_STARTED | — | — | — |
| ROOT-001 | PASS | 账本覆盖 133/133；分类 A68/B4/C0/D4/E34/F23 | run/PROJECT-GOVERNANCE-01/ROOT-001/logs/ + reports/PROJECT-GOVERNANCE-01/root/** | 前台复跑：根条目 133→61、git ls-tree 同次执行 5367/5367 不变、删除项指纹齐全；A/B 清运已执行、设计大纲 344 tracked 由前台提交 4511712b |
| ROOT-002 | PASS | 检查器正例 rc=0（verdict PASS、violations=0）；1 正 3 负 Ran 5 OK；注册表 145→147 项错误数 1→1（无新增） | run/PROJECT-GOVERNANCE-01/ROOT-002/logs/ + run/ci/root-cleanliness/ | 前台复跑一致；tests/quality 全量 rc=1 的失败为既存/外部（归 TEST-GREEN-001 与 CI-001），不计入本任务；ci/checks.json 写入已冻结交棒 |
| ROOT-003 | PASS | RETENTION.md 221 行；27 个被引用路径 EXISTS=27/MISSING=0；70 个无引用目录清运 | run/PROJECT-GOVERNANCE-01/ROOT-003/logs/ | 前台复跑一致；run/ 102G→97G；>1G 目录（perf-fix 57.5G / release-rescue 27.6G）因被 tracked 报告引用而保留，登记 GAP-029 |
| ROOT-004 | IN_PROGRESS | 分片 26/26、条目 785/785、列数不合格 0、重复 ID 0；四态 OPEN 733/RESOLVED 41/VOID 7/UNVERIFIABLE 4；P0 93 全覆盖 | reports/PROJECT-GOVERNANCE-01/root-scan/**（CONTINUATION.md 491 行） | 分片层门已满足；剩余=合并出表（REBASE_TABLE/SUMMARY/P0_RECHECK）+ 第 8/9 列按当时树重跑；任务已移交负责人另派 agent |
| ROOT-005 | PASS | 根清洁检查器 rc=0（verdict PASS、violations=0、根条目 54） | run/PROJECT-GOVERNANCE-01/ROOT-005/logs/ + run/ci/root-cleanliness/rc2.json | 前台复跑：CS/、worktrees/ 已删；AstroCS.wiki/ 保留（待 WIKI-001）、engineering/ 保留（root_manifest required_dirs）；logs/ 旧日志移入 run/archive/ |
| ROOT-006 | PASS | 【归属更正】23acb453 的 stat 含 ARCH-001 在途 rename（git mv 入索引 + 前台提交未带 pathspec），内容合法但归属挂错，不改历史，见下方流程修正记录；排除保证自测 rc=0（8/8）；检查器正例 pack rc=0×3、tracked rc=0；负例 22 项 OK | run/PROJECT-GOVERNANCE-01/ROOT-006/logs/FINAL_EVIDENCE.txt + reports/PROJECT-GOVERNANCE-01/security/** | 前台独立复跑一致（22 tests OK、pack 连跑 3 次 rc=0/0/0）；裁决：不轮换（正文无凭据）、不 git rm --cached（52 处路径引用）；CHK-SECRET-HYGIENE 并入 CI-001 收敛；待负责人确认旧审计包是否已外发 |
| ROOT-007 | PASS | 删除 4 个作废世代根文档 + evidence/**（2799 文件）+ 9 个旧审计脚本 + 1 探活日志；artifacts/** 按裁决删除 | reports/PROJECT-GOVERNANCE-01/root/deleted-manifest/** + git 历史 | 前台执行并提交（01db973b/7940d70e/4e95f841/b1290525/d414c3e0）；归档副本按负责人第二轮裁决一并删除；根条目 61→54 |
| WIKI-001 | NOT_STARTED | — | — | 依赖 GOV-001（权威收敛）；push 属发布类动作须负责人批准 |
| TEST-GREEN-001 | IN_PROGRESS | 原 4 项红灯已全绿（锚点 rc=0 808 锚 0 error；API-DOCS fail-closed + 3 新例；单位歧义 rc=0；R10 次序 error_count=0） | run/PROJECT-GOVERNANCE-01/TEST-GREEN-001/logs/ | 进行中：六层追溯门退役（GAP-032 裁决）+ tests/api 夹具重接线 + docs/ci 登记；前台复跑 `validate_registry --strict` → error_count=0（147→146 系 TRACEABILITY-CODE 退役） |
| RETIRE-001 | NOT_STARTED | — | — | 依赖 TEST-GREEN-001 交出 ci/checks.json 写权；对象=引用已删 artifacts 路径的旧世代打包/审计工具 |

## 控制包级门

- [ ] 所有任务 PASS（且每条 PASS 都有前台独立复跑证据）；
- [ ] GitHub Linux/Windows CI 在同一 SHA 全绿；
- [ ] Linux 最终 SHA 真实数据流通过；
- [ ] 图像审核（Agent 初审）通过，Owner 终审留白；
- [ ] Windows/Fatduck 同 SHA 复验通过（未完成则如实记 AWAITING_WINDOWS_VALIDATION）；
- [ ] FINAL-001 汇总经负责人审阅；
- [ ] 无 FAIL / BLOCKED / UNRESOLVED 冒充完成。

## 登记模板（复制到对应行）

```text
状态：PASS / FAIL / BLOCKED
机器门结果：<逐门一句话 + rc>
证据路径：run/PROJECT-GOVERNANCE-01/<TASK>/logs/…
前台结论：<复跑是否与执行 Agent 自述一致；有无越界；遗留项>
```
