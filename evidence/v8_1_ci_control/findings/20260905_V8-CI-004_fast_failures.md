# V8-CI-004 fast 全量固有红项登记（失败政策 12_FILE 分拣）

- task: V8-CI-004 ｜ check 批次: `python3 ci/run.py --profile fast` ｜ SHA: b2d31bfe6fff7e073dac6c718f2ecd4b65b4e8eb
- 结果: 57 项 = PASS 41 / FAIL 16，overall FAIL，exit 1，wall≈24.1s（目标 60s 达成，无需裁剪）
- 定性: 16 项 FAIL 全部为主仓库 main 固有内容红项，与本任务交付物（ci/impact_map.json、ci/tests/test_impact_map.py）无内容交集
- 处置: 依据 12_FAILURE_POLICY.md 归类 CODE_FAIL，移交 G-FIX 波既有任务修复；V8-CI-004 交付物本身达标判 CLOSED。G-CI Gate 机器计算时 fast 必须全绿（届时 G-FIX 已完成），不放水。

| # | check ID | 实际 | 分类 | 责任/修复归口 |
|---|----------|------|------|--------------|
| 1 | WORKSPACE-ADOPTION | REMOTE_RELATION 记录 SHA a4fdee3f 与实测 b2d31bfe 漂移 | CODE_FAIL（结构性滞后） | 前台集成流程：每次集成提交顺带刷新记录；Gate 时点复验（自引用 SHA 需固定 author/date 预计算 commit 或以 Gate 快照为准） |
| 2 | AGENTS-GOV | AstroCS_ENGINEERING_CONSTRAINTS.md 缺 10 项治理关键词 | CODE_FAIL（冻结文件，修改权仅项目负责人） | V8-DOC-001 治理文档修复；冻结文件改动须 Owner 授权，Agent 不得擅改 |
| 3 | TASK-RESULT-SCHEMA | 唯一失败 evidence/v6_1_rework/tasks/REL-003/TASK_RESULT.json bad status | CODE_FAIL（历史证据文件 status 非法） | V8-RES-001（V7.1 台账重建）/证据修正 |
| 4 | UT-VERSION | tests/version 16 tests, 2 failures | CODE_FAIL | V8-RT-001/002（Phase2 模块链与真实操作） |
| 5 | DOC-L0 | 内容性不满足 | CODE_FAIL | V8-DOC-001 |
| 6 | DOC-INDEX | 内容性不满足 | CODE_FAIL | V8-DOC-001 |
| 7 | CON-DOC-SYMBOLS | 内容性不满足 | CODE_FAIL | V8-DOC-001 |
| 8 | CON-FULL-INTEGRATION | 内容性不满足 | CODE_FAIL | V8-RT-001（生产 artifact/调用图） |
| 9 | ENG-CONSTRAINTS | 内容性不满足 | CODE_FAIL | V8-DOC-001（冻结约束引用同步） |
| 10 | P3-STATUS | 内容性不满足 | CODE_FAIL | V8-RT-002（Phase2 IR/操作边） |
| 11 | CLI-COMMAND-LAYER | 内容性不满足 | CODE_FAIL | V8-RT-001/002 |
| 12 | CLI-RUN-PRESET | 内容性不满足 | CODE_FAIL | V8-RT-001/002 |
| 13 | NO-SERIAL-HEAVY | resource gate 无生产调用方 | CODE_FAIL | V8-MON-001/002 + V8-CPU-001（V8-CI-003 已交付统一 wrapper，待挂生产链） |
| 14 | SERIAL-HARDCODE | 内容性不满足 | CODE_FAIL | V8-CPU-001（移除硬编码 worker） |
| 15 | THREAD-BUDGET | 内容性不满足 | CODE_FAIL | V8-CPU-001 + V8-MON-001（effective thread budget 集中化） |
| 16 | API-DOCS | 内容性不满足 | CODE_FAIL | V8-DOC-001（模块 API/ABI 文档图） |

证据: evidence/v8_1_ci_control/tasks/V8-CI-004/artifacts/CI_RESULT_fast_full.json（逐项详情）, logs/fast_full.log
