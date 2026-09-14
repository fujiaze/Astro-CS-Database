# 控制包任务号 与 提交消息编号 的机械对接

方法：从包内 TASK_LEDGER 取任务号全集，在 1988 条提交消息的编号抽取结果（正则）中查同名或规范化变体。
局限：提交消息未写编号的情况不计入；此表只说明“包与提交在编号口径上是否对得上“，不说明工作完成度。

| 身份 | 台账号数 | 消息中出现 | 命中率% | 相关提交数 | 未命中样例 |
|---|---|---|---|---|---|
| archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | 199 | 190 | 95.5 | 578 | ACR-002, ACR-003, BASE-002, CHK-004, ID-002, ID-003, PKG-001 |
| AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_2026 | 191 | 167 | 87.4 | 317 | AUD-002, AUD-003, AUD-004, AUD-005, CAT-GAIA-DOC, CAT-GAIA-I |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_A | 98 | 98 | 100.0 | 379 |  |
| REAUDIT_V4/v4_reaudit | 98 | 0 | 0.0 | 0 | C0-001, C0-002, C0-003, C0-004, C0-005, C1-001, C1-002, C1-0 |
| prerelease_v5/AUDIT_REVIEW | 98 | 98 | 100.0 | 379 |  |
| prerelease_v5/audit_src | 98 | 98 | 100.0 | 379 |  |
| 工程控制/REAUDIT_V4 | 98 | 0 | 0.0 | 0 | C0-001, C0-002, C0-003, C0-004, C0-005, C1-001, C1-002, C1-0 |
| AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_202 | 88 | 88 | 100.0 | 337 |  |
| CONTROL_V6 | 88 | 88 | 100.0 | 337 |  |
| REAUDIT_V3/v3_audit | 62 | 55 | 88.7 | 194 | ACR-002, ACR-003, CHK-004, ID-002, ID-003, PKG-001, RUN-006 |
| REAUDIT_V3/v3_cp0 | 62 | 55 | 88.7 | 194 | ACR-002, ACR-003, CHK-004, ID-002, ID-003, PKG-001, RUN-006 |
| REAUDIT_V3/v3_reaudit | 62 | 55 | 88.7 | 194 | ACR-002, ACR-003, CHK-004, ID-002, ID-003, PKG-001, RUN-006 |
| 工程控制/REAUDIT_V3 | 62 | 55 | 88.7 | 194 | ACR-002, ACR-003, CHK-004, ID-002, ID-003, PKG-001, RUN-006 |
| AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20 | 57 | 33 | 57.9 | 125 | ARCH-AUDIT-P1, ARCH-TB-001, BASE-UTIL-001, CI-BACKEND-001, C |
| AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPT | 54 | 52 | 96.3 | 232 | BASE-002, REV-004 |
| CONTROL_V4 | 54 | 52 | 96.3 | 232 | BASE-002, REV-004 |
