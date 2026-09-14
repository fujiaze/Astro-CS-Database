# STAGE-01 上游前史与初代开发包期（engineering v1.0–v1.3 / 权威 v2.0 / Stage1 交付）

对应组：G01、G02、G03。对应任务一历史阶段：STAGE-01（seq 1–271，05-24→08-02，设计大纲/reports/history/00_OVERVIEW.md §1 表行 1）。

## 1. 时间窗
- 包侧：首包 v1.0 引入 seq 118（1ac74251，2026-07-24T18:19，P-G01 片尾一）→ 本代最末事件 a78f5430（2026-07-31T18:29 根目录清理，同删 G02 三形态与 G03 全部对象，P-G02/P-G03 §一、pack_events.md）。
- 上游接线：v1.0 基线锚 9f10c72c（2026-07-24T17:32，导出包 AstroCS_Database_Context.zip 的 HEAD；git rev-list --count 复算 116，P-G01 片尾一）；主仓初始化根提交 c384de17（seq 100，history/00_OVERVIEW）。13 条独立根谱系（05-24 起 4 条 + 07-10~07-16 9 条）先于全部控制包存在（P-G01 片尾一列全 13 SHA）。
- 注：本代 15 身份全部无 LEDGER 结构事件——structural_events 的 LEDGER 采集自 seq 978 起（pack_events.md §三窗口性缺口）。

## 2. 包清单（15 身份，归并后 7 个逻辑包）
| 逻辑包 | 身份 | 形态 | 引入/删除 |
|---|---|---|---|
| engineering v1.0 | engineering_archive_v1.0 | 仅历史复原 | 1ac74251 / 036a3bb5 |
| CLI_Core v1.1 | AstroCS_CLI_Core_Development_Pack、_new_pack_v1.1、…_2026-07-24_v1.1(zip) | 复原+zip 复原（三身份一包，92/92 同哈希） | 036a3bb5 一次删 3244 条路径（P-G01 §一） |
| v1.2 / v1.3 | engineering_v1.2、engineering_v1.3 | 仅历史复原（v1.3 截断 552→400） | 036a3bb5 以 455 条 rename 并入 工程控制/ |
| AstroCS_Delivery_20260729 | 同名(zip) | zip_recovered | — |
| 权威开发包 v2.0 | …_2026-07-30_v2.0(zip)、…_v2.0(解包)、engineering_authoritative(安装副本) | 1 包 3 形态（唯一差异 MASTER_TASK_REGISTER.csv CRLF↔LF） | 75b05f72 / a78f5430；zip 由 b9d20c50 单独删 |
| Stage1 Wiki 冻结 | AstroCS_Stage1_Wiki_Freeze_2026-07-30 | 仅历史复原 | 78a9121d / a78f5430 |
| Stage1 交付+Agent 包 | AstroCS_Stage1_HISS_Delivery(+_2026-07-31 zip)、AstroCS_Stage1_HISS_Agent_Package_2026-07-31、_agent_package | 4 身份→3 物理对象；zip CRLF/目录 LF | 78a9121d / a78f5430 |

## 3. 共同设计意图（转述，标出处）
- v1.0/v1.1：以阶段任务号（P00–P08）+ Gate 清单（G0…G8）驱动前史模块恢复纳管——v1.0 台账 P00-002/P00-003 定义为把 healpix_drizzle/healpix_stack 源码纳管（P-G01，control/MASTER_TASK_REGISTER.csv 行 3–4）。
- v1.2：基线锁定声明（冻结 v1.1 HEAD ed145a7 与 3 个控制文件 SHA-256）+ CORR-001..006 废止前代结论（P-G01 事实 6；该锁定在全史 blob 中 0 命中，无法复核）。
- v2.0（AUTONOMOUS_ENTRY.md §3、migration/CURRENT_STATE_AND_SCOPE_MIGRATION.md）：明文"不继续机械执行旧 v1.2/v1.3 的 50 项任务"，废止 6 项跑偏任务、继承 v1.3 DONE 22 项成果口径——废止+成果继承+无字节继承三重（P-G02 衔接段）。
- Stage1 Agent 包：单次连续执行 Phase 0-6、绝对禁止 Stage2/710 帧（START_HERE.md L19-27，P-G03 §六）；交付包为"差异代码+patch+报告"形态，系本阶段唯一含实现物的身份。

## 4. 任务规模与状态字面量口径
- v1.0 62 行（P00–P08，8 态机定义、实有 2 态）→ v1.1 31 行（TODO/DONE/BLOCKED+VERDICT 三值）→ v1.2/v1.3 50 行（TODO/DONE/DEFERRED；v1.3 终态 DONE 22/IN_PROGRESS 1/TODO 27）→ v2.0 30 行（READY/TODO/…/PENDING_USER_DECISION；执行终态 DONE 28/1/1）→ Stage1 无台账（P-G01/P-G02/P-G03 §三；digest_verify 抽中 CLI_Core，1 实例实测吻合）。

## 5. 门禁演进（本代内部）
- v1.0：3 校验脚本；v1.1：无 tools/；v1.2：validate_pack.py+phase_task_controller.py；v1.3：REQUIRED 11 项、要求审核包，但机器门在仓库内不可能通过（REQUIRED 含未入库的 review_inputs/P11-004_review_bundle.zip，实测 exit=2，P-G01 事实 3）。
- v2.0：Gate A–I 清单与任务号 1:1、校验器改依赖环检测、不再查任务卡（P-G02 §六）；执行期实测"卡-账漂移 24/30、7/9 Gate 清单 0 勾选而 9 份 GATE_*_REPORT.md 已称 PASS"、G-004/G-005 有账无据（P-G02 事实 3）；710 帧越门启动留痕自洽（PROJECT_STATE 改 full_regression_allowed:true，GATE_I_REPORT.md:5 自述违规停止，P-G02 事实 4）。
- 交付包自校验悖论：同一 zip 内容在 zip 形态 48 条哈希全过、解包形态 9 条不符（行尾归一差异，.gitattributes@78a9121d L2 * text=auto eol=lf，P-G03 事实 2）——此口径问题由 digest_verify O-1/O-2 在复核层再次确认。

## 6. 执行留痕概况
- 无包↔提交对接行（pack_commit_link.csv 无 G01–G03 身份；P-G01/P-G02/P-G03 自算）：v1.0 59.7%、v1.1 96.8%、v1.2/v1.3 各 50.0%（P-G01 事实 5）；唯一"有账无据" P02-004；v1.2/v1.3 P14–P17 整段 25 号全史 0 提及。
- v2.0 执行链 15 次提交（07-30 10:47→16:26）；Stage1 交付包 changed_files/ 15 件中 14 件与 78a9121d 同 sha、patch 在 183558ad 上可复现（P-G03 事实 4）；测试计数自相矛盾（包内 21/21 vs 输出块"通过:0"，修复在包外 59e9e39e，P-G03 事实 5）。

## 7. 与下一代差异（预告）
- 路径谱系：engineering*/工程控制/ → 下一代 ACR 包走 工程控制/docs/ 子路径（G04）；13 根谱系的模块路径坐标系（lib/orchestrator、lib/plate_solve）延续（P-G01 片尾一）。
- 本代包全部"仅历史/复原"形态：现存 worktree 无残留；a78f5430+036a3bb5 两波清理后，仅 migration 两文件经 b7b2dea7 存活于 G11 容器（P-G02 事实 6）。

## 8. 缺口（汇总自三份报告，共 47 条：G01 17 / G02 15 / G03 15）
最关键：v1.3 复原件截断 152 文件（124 个在 G11 容器找回，两报告互证）；v1.2 基线锁定哈希 0 命中不可复核；AstroCS_Stage1_Fix_Review_2026-07-31.zip（a78f5430 正文声明）全库 0 命中；digest 对 v1.3 与 G11 容器形态的登记不一致（P-G01 片尾二"不一致"判定，P-G11 重叠表同记）；pack_inventory/lineage 的"首次提交"对复原形态记成删除提交方向（P-G02/P-G03 缺口节）。
