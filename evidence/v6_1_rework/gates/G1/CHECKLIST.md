# G1 Gate Checklist — 治理、版本和检查器 (V6.1)

> 由 05_GATE_CHECKLISTS.md 复制；每项 PASS/FAIL + evidence path + SHA-256。

| # | 检查项 | 结果 | Evidence |
|---|---|---|---|
| 1 | Task 结果、commit、push 与 origin/main 可机器绑定，不能自证或引用未来 commit | PASS | COMMITS.csv（GOV-001 生成自 git log --first-parent）；parent/result 链连续；check_commits_csv.py SELFTEST_PASS（tamper/swap/unpushed 全抓）。evidence/v6_1_rework/tasks/GOV-001/logs/c01.log sha256=8607e0c780278fbb6d17b157b5f34145657fccec7198a41045b288ef19c8140c |
| 2 | 故意伪造 PASS、漏依赖、非法状态、未 push commit 均被负面测试拒绝 | PASS | validate_task_ledger.py 12 负例全抓（非法状态/双 IN_PROGRESS/PASS 依赖 WAITING/环/缺任务/REL 提前 PASS/任务数不符）；check_commits_csv.py 负例。GOV-002 logs sha256=5cb8a26dedc62017467c59f43fab7fcafd4554736fb7d54c020435c86be31ab3 |
| 3 | 版本唯一为 0.10.0-alpha.2，二进制、包、文档均从同一源生成 | PASS | VERSION=0.10.0-alpha.2；check_version_consistency.py VERSION_CONSISTENCY_PASS base=0.10.0-alpha.2；CLI --version=0.10.0-alpha.2+g<commit>；生产源码 0.1.0/alpha.1 清零。VER-001 logs/c02.log sha256=4a7c47b2fb6817bd9881eb92d1283036ae11ec26e9e1208518735ada22b04705 |
| 4 | 生产可达调用图能识别 CLI 直连 session/Drizzle 和 Runtime 死代码 | PASS | check_prod_reachability.py 基于 compile_commands+nm：负例抓 hp_drizzle_run_hips/p*_session_* 直连；真实扫描列出 12 项违规基线；PROD_REACHABILITY.json/dot 生成。CHK-001 logs/c01.log sha256=c3fb551ea648aa52f7d8bdc02b211a5580ef056730c9e72f59c6cf0a2197a961 |
| 5 | 静态 Pipeline 图与 observed trace 的 node/edge/port/artifact/version/backend/workers 全量比较 | PASS | check_pipeline_graph.py：--ir/--module-index/--trace 必填；7 负例全抓（少节点/换 artifact/换 class/隐藏节点/空 trace/版本不匹配）。CHK-002 logs/c01.log sha256=60e00c3a649eababd37419b652a875e2dba9854c49c9cfa7ca9c3bea664d87d3 |
| 6 | 空 trace、伪造 node、漏 edge、漏 artifact、重复 producer 均失败 | PASS | check_pipeline_graph.py 负例覆盖 empty_trace/hidden_node/fewer_nodes/swapped_artifact |
| 7 | serial-heavy、OpenMP 宏关闭、MSVC 排除、资源 gate 无生产 caller 均被识别 | PASS | check_serial_heavy.py：P2 宏关/P3 双循环/无 gate caller 负例全抓；真实扫描 8 项基线。CHK-003 logs/c01.log sha256=db8600e1c500ae2580d9e01265d4fdbc8f83de9ad7fdf8299487d036bc4b21a4 |

## 结论

G1 全部 7 项 PASS。任务—commit 绑定、状态机、版本单源、生产可达性、图/trace 一致性、serial-heavy/资源接线检查器均已建立并带负例。

创建时间：2026-08-31T05:10:00Z
