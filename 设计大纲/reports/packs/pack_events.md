# 控制包事件序列对齐（pack_events）

> 生成：AstroCS 控制包取证分析 · 包事件序列对齐专项。全程只读；本报告为唯一写入文件。
> 输入：设计大纲/_evidence/commits/structural_events.csv（seq,sha8,date,kind,count,detail,subject，用 python3 csv 模块解析）；身份与组：设计大纲/_evidence/packs/pack_inventory.csv、设计大纲/_evidence/packs/dispatch_groups.json；辅助：设计大纲/_evidence/packs/pack_lineage_table.md、设计大纲/reports/history/structural_events.md。
> 口径：seq 为 CSV 拓扑序（非严格日期序）；每条事件引用格式 seq｜sha8｜日期；本文不评价、不推测，规则归一处一律显式标注。

## 一、方法说明

1. **身份集**：pack_inventory.csv 身份列去重 = 49 个身份；dispatch_groups.json 的 G01–G14＋G99 组表提供组归属（身份名中的 __ 与台账路径的 / 互为别名；AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 与 AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 同时登记于 G05 与 G06，记 G05/G06）。
2. **marker 路径→身份归一规则（PACKNEW/PACKDEL 的 detail 去"新增包文件/删除包文件"前缀后处理）**：
   - R1 尾匹配：取 marker 文件的父目录链，若某身份（按 / 拆成组件序列，如 REAUDIT_V3/v3_audit）恰等于目录链后缀序列，归该身份；多个身份命中时取组件序列最长者（最深包目录）。单字组件身份 acr 仅当左邻目录为 tasks 时命中（防误配 lib/docs 下同名词）。
   - R2 容器（嵌套）匹配：身份目录名出现在父目录链中段时（如 工程控制/CONTROL_V4/内包/00_READ_FIRST.md 的 CONTROL_V4），记为"嵌套容器、随内包同提交引入"，与内包共同持有该事件；命中容器共 6 个：CONTROL_V4、CONTROL_V6、_agent_package、_new_pack_v1.1、工程控制/REAUDIT_V3、工程控制/REAUDIT_V4。
   - R3 别名：目录 engineering/（seq128｜ba4f0d34｜2026-07-25 引入，seq203｜036a3bb5｜2026-07-29 删除）归一为 engineering_archive_v1.0。证据：git show 036a3bb5 name-status 删除 engineering/AUTONOMOUS_ENTRY.md、engineering/START_PROMPT.txt；pack_lineage_table.md 记该身份首次 2026-07-25（与 seq128 同日）；pack_inventory.csv 记实例 history/036a3bb5/engineering_archive_v1.0。
   - 无法归一时处置：保留原 detail 路径并列入§三缺口，不强行分配。本次 PACKNEW 52 条（主命中 50 条 + R3 别名 2 条；其中 13 条同时按 R2 归予容器身份）与 PACKDEL 12 条（含 R3 别名 4 条）全部归一，事件级零残留。
3. **台账（LEDGER）归一**：detail 只给台账文件名（289 条中 146 条 TASK_LEDGER.csv、134 条 02_TASK_LEDGER.csv、其余 CONTROL_TASK_LEDGER.csv/V7_1_STATIC_TASK_LEDGER.csv/05_TASK_LEDGER.csv/TASK_LEDGER_V1_ARCHIVE.csv 组合）。做法：对每条事件的 sha8 做只读 git show --name-only，取该提交实际改动的台账文件全路径，再按 R1/R2 归身份——150 条据此路径实证归一；139 条台账位于包外执行态路径（evidence/refactor/TASK_LEDGER.csv 80 条，seq1261–1355；evidence/v6_1_rework/TASK_LEDGER.csv 60 条，seq1361–1458，两段 seq 区间无重叠），按 subject 任务号族（BAS:/G10-WIN: 等 V6 88 任务族 vs R0-/RT- 等 V6.1 族）＋日期窗＋目录名自指（v6_1_rework；R0-003 题述"V6.1 证据台账"）规则归一并标注"推断"；剩余 0 条不能唯一归属。台账末次落账只计 M/A（内容修改/入库）事件，随档移动（R）单列入废止/归档列。
4. **废止/归档**：PACKDEL，及 DELETE/RENAME50 事件对应提交的 git name-status 中该身份路径文件出现 D（删除）或 R（改名/移档）的最晚 seq 事件；subject 含"归档/superseded/移除/整理"仅作核对，不单独构成事件。10 条 DELETE 中 6 条（seq39、95、243、310、322、1785）整包外文件与任何身份路径无交集，判"与控制包无关"。
5. **zip 上传事件**：CSV 无 zip 上传 kind；全史 name-status 中 工程控制/_control_packs/*.zip 与 artifacts/*.zip（除报告交付产物）0 条入库记录（zip 原件按目录规范不入库），故 zip-only 身份的引入一律记"CSV 未捕获"。

## 二、主表（49 身份，组序→引入日期序）

| 身份 | 组 | 包引入事件（PACKNEW 最早） | 台账最后落账事件（M/A 最晚） | 包废止/归档事件（最晚） |
|---|---|---|---|---|
| AstroCS_CLI_Core_Development_Pack | G01 | seq128｜ba4f0d34｜2026-07-25 PACKNEW 标记 AUTONOMOUS_ENTRY.md | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq203｜036a3bb5｜2026-07-29 DELETE/RENAME50 文件移除+改名 |
| _new_pack_v1.1 | G01 | seq128｜ba4f0d34｜2026-07-25 PACKNEW 标记 AUTONOMOUS_ENTRY.md；嵌套容器——标记文件在内包目录下，随内包同提交引入 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq203｜036a3bb5｜2026-07-29 DELETE/RENAME50 文件移除+改名 |
| engineering_archive_v1.0 | G01 | seq128｜ba4f0d34｜2026-07-25 PACKNEW 标记 AUTONOMOUS_ENTRY.md；别名规则 R3：目录 engineering→该身份 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq203｜036a3bb5｜2026-07-29 DELETE/RENAME50 文件移除+改名（036a3bb5 根目录整理：标记删除、内容转档 engineering_archive_v1.0） |
| engineering_v1.2 | G01 | seq162｜0d1857de｜2026-07-27 PACKNEW 标记 AUTONOMOUS_ENTRY.md | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq203｜036a3bb5｜2026-07-29 DELETE 提交内改名移走 |
| engineering_v1.3 | G01 | seq175｜f4ec8b24｜2026-07-28 PACKNEW 标记 AUTONOMOUS_ENTRY.md | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq203｜036a3bb5｜2026-07-29 DELETE/RENAME50 文件移除+改名（2 枚标记 R 改名为 工程控制/ 根，其余文件删除） |
| AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1 | G01 | CSV 未捕获（zip-only，zip 上传不产生 git 路径事件） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_Delivery_20260729 | G01 | CSV 未捕获（zip-only，同上） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_Authoritative_Development_Pack_v2.0 | G02 | seq204｜75b05f72｜2026-07-30 PACKNEW 标记 AUTONOMOUS_ENTRY.md | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| AstroCS_Stage1_Wiki_Freeze_2026-07-30 | G02 | seq226｜78a9121d｜2026-07-31 PACKNEW 标记 MANIFEST.json | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0 | G02 | CSV 未捕获（zip-only（history_zip 复原件），同上） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| engineering_authoritative | G02 | CSV 未捕获（v2.0 安装目录复原件，包内无标记文件；seq204/237 事件归内包 AstroCS_Authoritative_Development_Pack_v2.0） | CSV 无该包台账事件（包目录内台账文件未被任何 LEDGER 提交触及） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| AstroCS_Stage1_HISS_Agent_Package_2026-07-31 | G03 | seq226｜78a9121d｜2026-07-31 PACKNEW 标记 MANIFEST.json | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| AstroCS_Stage1_HISS_Delivery | G03 | seq226｜78a9121d｜2026-07-31 PACKNEW 标记 MANIFEST.json | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| _agent_package | G03 | seq226｜78a9121d｜2026-07-31 PACKNEW 标记 MANIFEST.json；嵌套容器——标记文件在内包目录下，随内包同提交引入 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq237｜a78f5430｜2026-07-31 DELETE 提交删除文件 |
| AstroCS_Stage1_HISS_Delivery_2026-07-31 | G03 | CSV 未捕获（zip-only，同上） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| acr | G04 | seq293｜49ea5c1e｜2026-08-02 PACKNEW 标记 00_AGENT_START_PROMPT.txt；同类标记重复注册：seq550｜198d69e0｜2026-08-10 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| ACR_FOCUSED_CONTROL_PACKAGE | G04 | seq406｜0030c3a5｜2026-08-05 PACKNEW 标记 00_AGENT_START_PROMPT.txt；同类标记重复注册：seq550｜198d69e0｜2026-08-10 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| ACR_FOCUSED_CONTROL_PACKAGE_V2 | G04 | seq429｜40c9d216｜2026-08-06 PACKNEW 标记 00_AGENT_START_PROMPT.txt；同类标记重复注册：seq550｜198d69e0｜2026-08-10 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| ACR_FOCUSED_CONTROL_PACKAGE_V3 | G04 | seq438｜a36c4825｜2026-08-06 PACKNEW 标记 00_AGENT_START_PROMPT.txt；同类标记重复注册：seq550｜198d69e0｜2026-08-10 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| ACR_FOCUSED_CONTROL_PACKAGE_V4 | G04 | seq460｜38b85702｜2026-08-06 PACKNEW 标记 00_AGENT_START_PROMPT.txt；同类标记重复注册：seq550｜198d69e0｜2026-08-10 | CSV 无该包台账事件（LEDGER 采集窗口自 seq978 2026-08-27 起，早于窗口该包已废止） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| REAUDIT_V3/v3_audit | G05 | seq978｜3703650d｜2026-08-27 PACKNEW 标记 00_READ_FIRST.md | seq978｜3703650d｜2026-08-27 台账 TASK_LEDGER.csv（仅随引入/入库入账一次，此后无内容落账事件） | seq979｜ac2ced53｜2026-08-27 RENAME50 移档/改名（ac2ced53：报告移出至 reports/REAUDIT_V3） |
| REAUDIT_V3/v3_cp0 | G05 | seq978｜3703650d｜2026-08-27 PACKNEW 标记 00_READ_FIRST.md | seq978｜3703650d｜2026-08-27 台账 TASK_LEDGER.csv（仅随引入/入库入账一次，此后无内容落账事件） | seq979｜ac2ced53｜2026-08-27 RENAME50 移档/改名（ac2ced53：报告移出至 reports/REAUDIT_V3） |
| REAUDIT_V3/v3_reaudit | G05 | seq978｜3703650d｜2026-08-27 PACKNEW 标记 00_READ_FIRST.md | seq1036｜6a947f51｜2026-08-28 台账 02_TASK_LEDGER.csv | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| 工程控制/REAUDIT_V3 | G05 | seq978｜3703650d｜2026-08-27 PACKNEW 标记 00_READ_FIRST.md；嵌套容器——标记文件在内包目录下，随内包同提交引入 | seq1036｜6a947f51｜2026-08-28 台账 02_TASK_LEDGER.csv | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 | G05/G06 | seq1039｜b12305ed｜2026-08-28 PACKNEW 标记 00_READ_FIRST.md | seq1039｜b12305ed｜2026-08-28 台账 02_TASK_LEDGER.csv（仅随引入/入库入账一次，此后无内容落账事件） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名（先 1039 归位，后 1480 随容器归档） |
| AstroCS_CP0 | G05 | CSV 未捕获（zip 为交付产物：首入库 seq978 3703650d 2026-08-27 于 工程控制/REAUDIT_V3/cp0_out/，非标记文件，PACKNEW 不覆盖） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826 | G05 | CSV 未捕获（zip 原件归 _control_packs/，全史 0 条入库记录，不产生路径事件） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 | G05/G06 | CSV 未捕获（用户上传 zip 原件归 _control_packs/，不入库；其执行体目录事件归 REAUDIT_V3 系身份） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z | G05 | CSV 未捕获（zip 原件归 _control_packs/，不入库） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| CONTROL_V4 | G06 | seq1039｜b12305ed｜2026-08-28 PACKNEW 标记 00_READ_FIRST.md；嵌套容器——标记文件在内包目录下，随内包同提交引入 | seq1039｜b12305ed｜2026-08-28 台账 02_TASK_LEDGER.csv（仅随引入/入库入账一次，此后无内容落账事件） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名（先 1039 归位，后 1480 随容器归档） |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 | G07 | seq1042｜f99e80d8｜2026-08-28 PACKNEW 标记 00_READ_FIRST.md | seq1251｜3ca7f974｜2026-08-30 台账 02_TASK_LEDGER.csv | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| REAUDIT_V4/v4_reaudit | G07 | seq1037｜020cdc99｜2026-08-28 PACKNEW 标记 00_READ_FIRST.md | seq1038｜ae643541｜2026-08-28 台账 02_TASK_LEDGER.csv | seq1039｜b12305ed｜2026-08-28 DELETE 提交删除文件 |
| 工程控制/REAUDIT_V4 | G07 | seq1037｜020cdc99｜2026-08-28 PACKNEW 标记 00_READ_FIRST.md；嵌套容器——标记文件在内包目录下，随内包同提交引入 | seq1038｜ae643541｜2026-08-28 台账 02_TASK_LEDGER.csv | seq1039｜b12305ed｜2026-08-28 DELETE 提交删除文件 |
| prerelease_v5/AUDIT_REVIEW | G07 | seq1232｜ef0858c5｜2026-08-30 PACKNEW 标记 00_READ_FIRST.md | seq1256｜fd4e11ed｜2026-08-30 台账 TASK_LEDGER.csv | 无废止/归档事件（当前树活跃） |
| AUDIT_PACKAGE_587fe0e341a7 | G07 | CSV 未捕获（zip 原件归 artifacts/，全史 0 条入库记录） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828 | G07 | CSV 未捕获（zip 原件归 _control_packs/，不入库） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| prerelease_v5/audit_src | G07 | CSV 未捕获（当前树工作副本，从未入库，git 全史该路径 0 文件） | CSV 未捕获（工作副本未入库，无台账路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 | G08 | seq1261｜4b1b948e｜2026-08-30 PACKNEW 标记 00_READ_FIRST.md | seq1355｜8be6eb4e｜2026-08-31 台账 TASK_LEDGER.csv（规则归一·推断：包外执行态台账 evidence/refactor/TASK_LEDGER.csv；候选：CONTROL_V6（外层容器）） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| CONTROL_V6 | G08 | seq1261｜4b1b948e｜2026-08-30 PACKNEW 标记 00_READ_FIRST.md；嵌套容器——标记文件在内包目录下，随内包同提交引入 | seq1355｜8be6eb4e｜2026-08-31 台账 TASK_LEDGER.csv（规则归一·推断：包外执行态台账 evidence/refactor/TASK_LEDGER.csv；候选：AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830（同内容内包）） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| AstroCS_V6_1_REWORK_CONTROL_20260831 | G09 | CSV 未捕获（包入库提交 8c71f7ab 2026-09-02 仅注册 1 个非标记文件 02_FINDINGS.csv，包内无标记文件入库） | seq1458｜faad602d｜2026-09-02 台账 TASK_LEDGER.csv（规则归一·推断：包外执行态台账 evidence/v6_1_rework/TASK_LEDGER.csv；证据：目录名 v6_1_rework 自指＋R0-003 任务题述 V6.1 证据台账） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名（唯一入库文件 02_FINDINGS.csv 随 GOV-002 移档） |
| AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3 | G10 | CSV 未捕获（zip 形态：zip 随 V8.1 baseline 于 seq1521 a4fdee3f 2026-09-05 入库，属普通文件入库而非标记引入） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 | G10 | CSV 未捕获（当前树工作副本，从未入库，git 全史该路径 0 文件） | CSV 未捕获（工作副本未入库，无台账路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | G11 | CSV 未捕获（归档容器：由 seq1480 b7b2dea7 2026-09-02 GOV-002 批量 mv 成组，非标记引入） | 无内容落账；随档移动 seq1480｜b7b2dea7｜2026-09-02（R 改名，非落账） | seq1480｜b7b2dea7｜2026-09-02 RENAME50 移档/改名 |
| AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905 | G12 | seq1521｜a4fdee3f｜2026-09-05 PACKNEW 标记 00_READ_FIRST.md | seq1521｜a4fdee3f｜2026-09-05 台账 CONTROL_TASK_LEDGER.csv（仅随引入/入库入账一次，此后无内容落账事件） | seq1787｜8e03d7da｜2026-09-10 RENAME50 移档/改名 |
| 2026-09-09_superseded_V8.1_CI_CONTROL_20260905 | G12 | CSV 未捕获（归档副本：由 seq1787 8e03d7da 2026-09-10 GOV-002 移档 mv 成组，非新引入） | 无内容落账；随档移动 seq1787｜8e03d7da｜2026-09-10（R 改名，非落账） | seq1787｜8e03d7da｜2026-09-10 RENAME50 移档/改名 |
| cprun-v4-pack-template | G13 | seq1784｜4d3feaf4｜2026-09-09 PACKNEW 标记 control-pack.json | CSV 无该包台账事件（包目录内台账文件未被任何 LEDGER 提交触及） | 无废止/归档事件（当前树活跃） |
| AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 | G13 | seq1796｜e9547c02｜2026-09-10 PACKNEW 标记 00_READ_FIRST.md；目录本体首现于 seq1797 85a5fc60 2026-09-10（先建目录后补标记） | seq1832｜a628bc21｜2026-09-12 台账 TASK_LEDGER.csv | 无废止/归档事件（当前树活跃） |
| AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | G14 | CSV 未捕获（zip 归 _control_packs/ 不入库 + 当前树工作副本未入库） | CSV 未捕获（zip + 工作副本均未入库，无台账路径事件） | 无废止/归档事件（从未入库或 zip 形态） |
| AstroCS_AUDIT_REVIEWPACK_20260909T133841Z | G99 | CSV 未捕获（zip 原件归 artifacts/，全史 0 条入库记录） | CSV 未捕获（zip-only，台账随 zip 不入库，无路径事件） | 无废止/归档事件（从未入库或 zip 形态） |

### 关键对照：工程控制/REAUDIT_V4（自造包）× CONTROL_V4（用户上传原包归位）

同一提交 seq1039｜b12305ed｜2026-08-28 同时完成"删一归一"（subject：chore(v5): 按用户指示移除自造V4重审控制包并归位用户上传的V4_CPU_ADAPTIVE原包）：
- 自造包引入：seq1037｜020cdc99｜2026-08-28 PACKNEW 工程控制/REAUDIT_V4/v4_reaudit/00_READ_FIRST.md；台账唯一改动 seq1038｜ae643541｜2026-08-28（02_TASK_LEDGER.csv，98 任务账本）。
- 自造包移除：seq1039｜b12305ed｜2026-08-28 PACKDEL 同标记文件 + DELETE 该目录 21 文件（含其 02_TASK_LEDGER.csv D）。
- 上传原包归位：同 seq1039｜b12305ed PACKNEW 工程控制/CONTROL_V4/AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828/00_READ_FIRST.md，同提交内该包 02_TASK_LEDGER.csv 以 A 入库（54 任务）；CSV 中该包无独立 zip 上传事件（zip 归 _control_packs/ 不入库）。
- 后续同轨：两包（CONTROL_V4 容器与其内包）均于 seq1480｜b7b2dea7｜2026-09-02 随 GOV-002 归档移入 legacy 容器；对照包 V5 引入紧随其后 seq1042｜f99e80d8｜2026-08-28。

## 三、事件缺口清单

### 3.1 条数差比（52/12 vs 49 身份）
- PACKNEW 52 条 → 主归一 50 条 + R3 别名 2 条（engineering），事件级归一 52/52；其中 10 条为 seq550｜198d69e0｜2026-08-10 对 acr 与 ACR V1–V4 五个目录的同类标记重复注册（每目录 2 条）。
- 52 条收敛后覆盖 30 个身份（61.2%）：直接命中 23 + 别名 1（engineering_archive_v1.0）+ 容器嵌套 6（CONTROL_V4、CONTROL_V6、_agent_package、_new_pack_v1.1、工程控制/REAUDIT_V3、工程控制/REAUDIT_V4）。
- PACKDEL 12 条 → 覆盖 8 个主身份（AstroCS_CLI_Core_Development_Pack、engineering_archive_v1.0（别名）、engineering_v1.2、AstroCS_Authoritative_Development_Pack_v2.0、AstroCS_Stage1_HISS_Delivery、AstroCS_Stage1_HISS_Agent_Package_2026-07-31、AstroCS_Stage1_Wiki_Freeze_2026-07-30、REAUDIT_V4/v4_reaudit）另携带 3 个容器身份（_new_pack_v1.1、_agent_package、工程控制/REAUDIT_V4）。
- 49 − 30 = 19 个身份在 CSV 中无任何引入事件，逐条见 3.2。engineering_v1.3 未被 PACKDEL 覆盖：其 2 枚标记在 seq203｜036a3bb5 以改名（R）迁往 工程控制/ 根，其余文件删除。

### 3.2 无引入事件的 19 身份（含原因类别；均属正常缺口的写明类别）
| 身份 | 组 | 缺口原因类别 |
|---|---|---|
| AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1 | G01 | A：zip-only，zip 上传不产生 git 路径事件 |
| AstroCS_Delivery_20260729 | G01 | A：zip-only（history_zip 复原件） |
| AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0 | G02 | A：zip-only（history_zip 复原件） |
| engineering_authoritative | G02 | C：历史复原安装目录，包内无标记文件（其引入=seq204 内包事件） |
| AstroCS_Stage1_HISS_Delivery_2026-07-31 | G03 | A：zip-only（history_zip 复原件） |
| AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826 | G05 | A：zip 原件归 _control_packs/，全史 0 条入库 |
| AstroCS_CP0 | G05 | A2：zip 于 seq978｜3703650d｜2026-08-27 以交付产物路径入库，非标记 |
| AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 | G05/G06 | A：用户上传 zip 不入库；执行体事件归 REAUDIT_V3 系 |
| AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z | G05 | A：zip 原件归 _control_packs/，不入库 |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828 | G07 | A：zip 原件归 _control_packs/，不入库 |
| AUDIT_PACKAGE_587fe0e341a7 | G07 | A：zip 归 artifacts/，全史 0 条入库 |
| prerelease_v5/audit_src | G07 | B：当前树工作副本，从未入库（git 全史该路径 0 文件） |
| AstroCS_V6_1_REWORK_CONTROL_20260831 | G09 | E：入库提交 8c71f7ab｜2026-09-02（pack_inventory.csv）仅注册 02_FINDINGS.csv 1 个非标记文件，无标记可触发 PACKNEW |
| AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3 | G10 | A2：zip 随 V8.1 baseline 于 seq1521｜a4fdee3f｜2026-09-05 入库，属普通文件入库 |
| AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 | G10 | B：当前树工作副本，从未入库 |
| archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | G11 | D：归档容器由 seq1480｜b7b2dea7｜2026-09-02 批量 mv 成组，非标记引入 |
| 2026-09-09_superseded_V8.1_CI_CONTROL_20260905 | G12 | D：归档副本由 seq1787｜8e03d7da｜2026-09-10 mv 成组 |
| AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | G14 | A+B：zip 不入库、工作副本未入库（pack_inventory.csv 无任何提交号） |
| AstroCS_AUDIT_REVIEWPACK_20260909T133841Z | G99 | A：zip 归 artifacts/，不入库 |

### 3.3 无法归一到任何身份的事件
- PACKNEW/PACKDEL：0 条（64/64 归一，R3 别名 4 条已闭合）。
- LEDGER：0 条（路径实证 150 条 + 规则归一 139 条；规则段的身份候选已逐一写入主表括号注，最紧 ambiguity 为 CONTROL_V6×AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 同内容对与 evidence/refactor↔V6、evidence/v6_1_rework↔AstroCS_V6_1_REWORK_CONTROL_20260831 的目录名自指推断）。
- RENAME50：6 条全部涉及身份路径（seq128、203、979、980、1480、1787）。
- DELETE：10 条中 6 条与控制包路径零交集，属工程侧批量删除而非包事件——seq39｜55d69dd0｜2026-07-12、seq95｜7604fdca｜2026-07-16、seq243｜428746c6｜2026-07-31、seq310｜23d07ed4｜2026-08-03、seq322｜8653bd54｜2026-08-03、seq1785｜789c5b6c｜2026-09-09；触及包路径的 4 条为 seq203、237、332（仅 acr，8 文件）、1039。

### 3.4 台账列缺口归类（34 个身份无台账落账事件）
- 窗口性缺口（20）：LEDGER 采集窗口自 seq978｜3703650d｜2026-08-27 起——G01–G03 的 15 身份在窗口开始前已废止（seq203｜036a3bb5｜07-29、seq237｜a78f5430｜07-31）；G04 的 5 身份存续跨窗口但台账文件名（TASK_LEDGER*/02_TASK_LEDGER*）从未出现在任何 LEDGER 提交（其 ACR-00x 任务号由 REAUDIT_V3 系台账承载，见 pack_commit_link.csv 未命中样例列）。
- 形态性缺口（14）：3.2 表中 A/A2/B 类身份（zip 内台账或未入库副本无路径事件），其中 AstroCS_V6_1_REWORK_CONTROL_20260831 以规则归一的执行态台账补足（seq1458｜faad602d｜2026-09-02）、archive 容器与 superseded 仅有 R 随档移动。

## 四、代际对齐速览（按 G01→G14 的引入提交时间窗，供层三归纳引用）

| 组 | 引入事件时间窗（PACKNEW 日期 min→max） | 锚点提交（seq｜sha8｜日期） | 备注 |
|---|---|---|---|
| G01 上游前史与初代 | 2026-07-25 → 2026-07-28 | 128｜ba4f0d34｜07-25；162｜0d1857de｜07-27；175｜f4ec8b24｜07-28 | 全组 5 身份于 seq203｜036a3bb5｜07-29 一并废止/移名 |
| G02 权威开发包 v2.0 | 2026-07-30 → 2026-07-31 | 204｜75b05f72｜07-30；226｜78a9121d｜07-31 | seq237｜a78f5430｜07-31 废止 |
| G03 Stage1 HISS | 2026-07-31（单日） | 226｜78a9121d｜07-31 | seq237 废止 |
| G04 ACR 聚焦 V1–V4 | 2026-08-02 → 2026-08-06 | 293｜49ea5c1e｜08-02；406/429/438/460｜08-05/06 | 重复注册 550｜198d69e0｜08-10；归档 980｜e1604f14｜08-27 与 1480｜b7b2dea7｜09-02 |
| G05 用户上传 V2/V3 与 REAUDIT_V3 三件套 | 2026-08-27（单日，5 标记一提交） | 978｜3703650d｜08-27 | zip 类身份无引入事件（3.2 A 类） |
| G05/G06 V4 上传包 | 2026-08-28 | 1039｜b12305ed｜08-28 | 与自造 REAUDIT_V4 移除同提交（见§二对照小节） |
| G06 CONTROL_V4 归位 | 2026-08-28 | 1039｜b12305ed｜08-28 | 嵌套容器引入 |
| G07 V5 发布控制与审核件 | 2026-08-28 → 2026-08-30 | 1037｜020cdc99｜08-28；1042｜f99e80d8｜08-28；1232｜ef0858c5｜08-30 | REAUDIT_V4 自造包引入于此窗、同窗废止 |
| G08 V6 系统重构 | 2026-08-30 | 1261｜4b1b948e｜08-30 | 执行台账窗 08-30→08-31（seq1261–1355） |
| G09 V6.1 返工 | 无 PACKNEW（CSV 缺口，E 类） | 登记锚 8c71f7ab｜2026-09-02（pack_inventory.csv） | 执行台账窗 08-31→09-02（seq1361–1458，规则归一） |
| G10 V7 重筑基 FINAL/FINAL3 | 无 PACKNEW（A2/B 类） | zip 入库锚 1521｜a4fdee3f｜09-05（随 V8.1 baseline） | 台账 191 行随 zip，无路径落账 |
| G11 09-02 legacy 归档容器 | 无 PACKNEW（D 类） | 成组锚 1480｜b7b2dea7｜09-02（GOV-002） | 收纳 9 子包（pack_inventory.csv 容器列） |
| G12 V8/V8.1 CI 控制 | 2026-09-05 | 1521｜a4fdee3f｜09-05 | superseded 副本 1787｜8e03d7da｜09-10 成组 |
| G13 宪章对齐 V1 与模板 | 2026-09-09 → 2026-09-10 | 1784｜4d3feaf4｜09-09；1796｜e9547c02｜09-10 | 宪法包台账活跃至 seq1832｜a628bc21｜09-12 |
| G14 发布救援 V3 | 无 PACKNEW（A+B 类） | 日期戳 2026-09-12（pack_inventory.csv；zip 未入库） | 台账 0 行 |
| G99 待判身份 | 无事件 | zip 日期戳 2026-09-09 | 唯一未归组身份 |

---
*方法附注：全部归一脚本置于 /tmp（不入仓）；git 侧核对仅用只读命令（git log/show --name-status）；未修改任何既有文件、未做任何 git 写操作、未解压 zip 入仓库。台账"最后落账"不含 R（随档移动）事件；引号内路径均相对仓库根。*
