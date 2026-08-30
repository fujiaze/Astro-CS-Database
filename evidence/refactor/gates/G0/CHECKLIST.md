# G0 Checklist — 起点冻结

生成时间: 2026-08-30T15:28:37Z
commit: e4fa8a368f3737dcb36da58e8a0310c08ea8073e

| 项 | 状态 | 证据 |
|---|---|---|
| 控制包 hash 与 CONTROL_PASS | PASS | CONTROL_PASS files=46 tasks=88; zip sha256 见 BAS-001 TASK_RESULT |
| HEAD/main/origin/main 相同 | PASS | HEAD=main=origin/main=e4fa8a368f3737dcb36da58e8a0310c08ea8073e (BAS-001 快进 push) |
| 外部修改全部登记 | PASS | EXTERNAL_CHANGES.md: 205 porcelain 条目; 5 tracked modified 保留 |
| build target/link/entry inventory 现场生成 | PASS | BAS-002 BUILD_TARGET_GRAPH.json/.dot + PRODUCTION_ENTRY_INVENTORY.csv (clean build exit0 + nm) |
| scheduler/thread/global lock inventory 现场生成 | PASS | BAS-003 SCHEDULER_INVENTORY(15)/SERIAL_HEAVY_FINDINGS(5)/GLOBAL_LOCKS(4) |
| P0-001..007 有最小证据和后续 Task | PASS | BAS-004 P0_OWNERSHIP_AND_SCIENCE_RISKS.csv (P0-003 动态 NOT_RUN 记录工具链阻塞) |
| 未修改科学/生产代码 | PASS | G0 四任务均为 evidence; 0 行生产代码改动 |
