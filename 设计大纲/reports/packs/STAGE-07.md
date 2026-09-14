# STAGE-07 宪章对齐与救援期（宪章对齐包 V1 / cprun 模板 / 09-09 审核包 / 发布救援 V3 / 元控制层 / RQS）

对应组：G13（含并入的 G99）、G14、G15、G16。对应任务一历史阶段：STAGE-08（seq 1786–1988，09-10→09-14，reports/history/00_OVERVIEW.md §1 表行 8）。**P-G13 已落盘并完成与前台实测的交叉核对（本节数字为两组一致值 + P-G13 增量实测）。**

## 1. 时间窗（同期交叠，非线性）
宪章冻结入库 seq 1786（d8c821db，09-10）→ CONSTITUTION_ALIGNMENT_CONTROL_V1 worktree 首入库 e9547c02（09-10 15:14:01）、最后 f5f944a5（09-12 18:52:52）（P-G13 §一实测；digest/inventory 记反方向，已订正）→ cprun-v4-pack-template 09-09 → AstroCS_AUDIT_REVIEWPACK_20260909T133841Z（zip，09-09 13:38，未入库）→ 包规范.md mtime 09-04（从未入库，仅 19ef3722 09-09 消息提及）→ ACTIVITY_STATE.md 首入库 8e03d7da（09-10）末更新 5a250999（09-13 13:15）→ RESCUE_V3 zip 内部时间戳 09-12 08:19–09:50、登记 ACTIVE 5a250999（09-13）、RESCUE 提交窗至 e60edcec（09-14 03:06）→ RQS 建区 43c5c488（09-13 21:18）至 HEAD b67c32fa（09-15 02:50，148 提交）。以上按 seq/日期如实并列，交叠即事实（前台指令同此）。

## 2. 包/件清单
| 对象 | 形态 | 关键实测 |
|---|---|---|
| AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 | worktree 74 文件 | 台账 57 行：PASSED 29 / NOT_STARTED 23 / IN_PROGRESS 5；状态字面量 PASSED（非 V3/V5 时代 PASS）；无 SHA256SUMS（自 09-09 代起不再有逐文件清单）。根件 20 项（00–08 编号规格 + CHANGELOG/control-pack.json/OWNER_BINDINGS.yaml/TASK_LEDGER.csv/TASK_LEDGER_V1_ARCHIVE.csv/schemas/tasks/validators）；control-pack.json：schema=cprun/v4、id=astrocs-constitution-alignment-v1、max_parallel=12、lanes=11、gates=5、tasks=57；07_FRONT_DESK_RULINGS 含裁决 R-01..R-14 与 BLOCKED_EXTERNAL（均前台实测，P-G13 落盘已复核一致；P-G13 增量：首入库版 CONTROL_FAIL——台账 25 行 vs 图 29 任务，缺 WCS-003/BASE-UTIL-001/P1-HIPS-DIGEST-001/ARCH-AUDIT-P1，末版 CONTROL_PASS tasks=57 files=74；宪章引用 7 文件、§14.5 六步顺序句在包内 0 处逐字出现，改由依赖链 CI-002→REAL-001→VIS-001→DOC-001→AUD-001→PACK-001+WIN 支链表达；包登记宪章 DRAFT 哈希 37fefdcc… 全史不可复现——宪章首入库 d8c821db 即 FROZEN/0f25544e…，P-G13 记未证实） |
| cprun-v4-pack-template | worktree 10 文件 | CPRun 派发模板；与 包规范.md 的一致性核对归 G15/P-G15（规范件槽位 vs 模板槽位） |
| AstroCS_AUDIT_REVIEWPACK_20260909T133841Z（G99 并入 G13） | zip 14,556 条目/14,554 唯一路径、58,795,448 B、sha 3b5d9c230473…；未跟踪亦未被忽略 | 与本组内容交集 0（zip 内 V1 条目 0、无宪章文件）；归并依据=同基线 789c5b6c + 同时间窗（P-G13 §二）；其被采 98 号台账经核为 V5 系（与 prerelease_v5/AUDIT_REVIEW/TASK_LEDGER.csv 逐字节同，98/98 命中日期全在 08-06→08-31）；另复现 G08 的"46 份 V6 文件"口径（zip 内 CONTROL_V6 恰 46 条）|
| AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | worktree+zip（24 文件逐字节一致，sha 14ee6592） | 两形态均未入库；无台账（00 L37 明文"不建立复杂台账"）；宪章条款号引用 0 次、版本号字面 0 处 |
| 工程控制/包规范.md / ACTIVITY_STATE.md | 非包形元控制件 | 规范件 git 痕迹仅消息 19ef3722；状态件 0600、§5 冻结 9 哈希全可核但指针 evidence/BASE-001/… 系框架证据域路径（实体 /workspace/.dsh/control-pack-runs/Rmtucy2cqced995/） |
| 问题扫描/（RQS-2026-01） | 非包形工作区件 | 742 文件全入库；INDEX v4 在快照 539626f0 处全量复现（520 条/224 份复合口径/P0 81）；FIX_LEDGER 实测 682×24 与 findings 全等集；有账无据=0、有据无账 1（c1959436） |

## 3. 共同设计意图（转述）
- 从"zip 派发+自校验"到"宪章为最高约束 + 活动状态登记"：救援包以 FRONT_DESK_RUNBOOK/TASK_LIST/18 任务组织（00/02 号），废止登记走 ACTIVITY_STATE §2（负责人 09-13 指令"使用该工程包替代以前的工程包，作为权威并执行"，经 5a250999 写入；DOCUMENT_INDEX.yaml L600/L604 同步）——P-G14 事实 6。
- RQS 形态：负责人直接指令 + 前台自建工作区 + 三级代理流水线（00_README/10_PROTOCOL/20_AGENT_PLAN 自述）；9 类别（A_SCI_DEF…I_DOC_HYGIENE）×P0/P1/P2 + finding schema + 禁改清单；协议外第 10 类别 C_ALG_IMPL 与第四档 p3 无授权文本（P-G16 结构偏差 1）。
- 派发协议演化痕迹：cprun 队列（V7 期 140 任务队列、cp 状态文件）→ 控制包模板（cprun-v4-pack-template）→ 包规范.md 的硬性结构规定——三者构成"规范件层"（P-G15/P-G13）。

## 4. 任务规模与状态字面量口径
宪章对齐 57 号（57.9% 命中率、PASSED 新字面量）→ V8.1 36 号（全 NOT_STARTED）→ RESCUE 18 任务无台账 → RQS 682 条账本（fix_state 实仅 OPEN 660/FIXED 20/PARTIAL 2，声明枚举 7 值）。状态机字面量在本期完成第三轮换血：PASS→PASSED（宪章对齐）、WAITING_WINDOWS→WAITING_RESOURCE（V7）→FATDUCK_PENDING（V8.1 留痕，越出 V7 枚举）；REVIEW_PENDING 被 V6 校验器判非法、被宪章期台账验证器（tools/quality/validate_task_ledger.py）判非法（AGENTS.md 状态机映射节）。

## 5. 门禁与验收口径
宪章条款成为门禁语义源（§10.5/§14.5/§18.2 数值与顺序；AGENTS-GOV 十要素机器门）；waiver 机制实体化：ci/checks.json 132 项全带 waivable（true 仅 7）+ci/known_failures.json——救援期只减不增（2→1，唯一变更 ccc7a933 为删除，P-G14 事实 5，与包内"不得扩 waiver"禁令同向）。

## 6. 与上一代差异
包不再自证完整性（无 SHA256SUMS/MANIFEST 逐文件清单：宪章对齐无、救援包亦无——对照 V5–V8.1 全程保留）；控制件的 git 存在性继续退化（zip 不入库成常态，本代三件全部未入库）同时出现反向——RQS 742 文件全部入库、账本随 HEAD 活体更新；审核封装件反向吞包（09-09 AUDIT_REVIEWPACK 内嵌 V6/V5/V7 材料 14,556 文件）。

## 7. 执行留痕概况
- 宪章对齐 57.9%（CSV 33/57、125 提交）系正则低估：P-G13 复现 33/57 同值，整号口径 50/57=87.7%、限包窗 49/57=86.0%；假阴 22（含 ARCH-AUDIT-P1、CI-BACKEND-001——两号在 57 台账内且各有 per-task 件，RE_TASKID 抽不出多段号）、假阳 5（RT-001A/B、MOD-001A/B、CLI-001B 整号 0 命中被母号折叠）；RESCUE 无号可核（无台账），改以"RESCUE-"前缀提交 17 枚 + run/release-rescue/ 65 条目对 18 任务映射：一致 14、有账无据 4（阶段 8–11）、有据无账 49；RQS 有账无据=0、有据无账 1（c1959436）；含 RQS 字样提交 157 枚。
- V7 账在 V8.1 留痕收口、宪章期把"台账-镜像口径差"制度化登记（ACTIVITY_STATE F4 两条参照链 32e3e414/524ab719，reconcile_state.py 实跑 9/9 PASS，P-G15 事实 4/5）。

## 8. 与任务一阶段对照
本代 = history STAGE-08（seq 1786–1988）；关键 seq：宪章冻结 1786、对齐包期 ~1796、RESCUE 1850、RQS 1859、CI 转绿 1944、FIX_LEDGER 1945（history/00_OVERVIEW §1 行 8 与 §2"控制包与治理"线）。

## 9. 缺口（P-G13 20 + P-G14 10 + P-G15 10 + P-G16 10 条，共 50）
P-G13 关键 20 条含：宪章对齐包 digest/inventory 首末提交方向倒置、adds_n/mod_n（6/39）与实测（74A/30M）不符、BASE-001 PASSED 但 evidence/BASE-001/freeze_snapshot_r1.json 工作树与全史均不存在（P-G15 事实 3 证其实体在框架证据域——两报告互证成对）、08 §3/§4 点名 8 号（MON-FIX-001 等）不在台账、三本台账（V1 10 列/V8.1 12 列/G99 内 V5 8 列）均被现行 tools/quality/validate_task_ledger.py 判 ledger columns mismatch、模板 2 份 zip 内嵌副本未入 inventory、V1 影子副本 4 份为早期版本快照未计。
其余：救援包"响应审核失败"叙事未证实（零痕迹）；RQS V 层 161 条不在 INDEX 口径；FD-*×29 与 rqs-fix 有据无账归属待裁（P-G14 缺口 5/9）；Rmtuajdhv73d430 归因与框架 package-ref 矛盾（P-G15）。coverage.md D-1 已闭合。
