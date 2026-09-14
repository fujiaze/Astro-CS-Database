# P-G16 · RQS-2026-01（问题扫描/ 现行工作程序，根目录只读审计工作区）取证报告

> 组：G16（前台追加派工）。身份：**非包形控制件**——根目录工作区 `问题扫描/`（代号 `RQS-2026-01`，见 `问题扫描/00_README.md:3`）。
> 本组不在 pack_inventory.csv / digest 体系内，无 zip、无 digest（派工令自述；本报告实测印证：`设计大纲/_evidence/packs/pack_inventory.csv`、`pack_lineage_table.md`、`pack_commit_link.csv` 与 `设计大纲/reports/packs/pack_events.md` 对「RQS / 问题扫描」grep 命中均为 **0**）。
> 全部实测在 HEAD=`b67c32fa`（2026-09-15 02:50:35 +0800）工作树执行；该提交即本路径最后一次提交。断言均带证据指针；「自述 vs 实测」逐条标注。

## 一、组内包清单与身份归并

| 项 | 值（实测） | 证据 |
|---|---|---|
| 身份 | `问题扫描/`（RQS-2026-01），负责人显式指定建立的只读审计工作区 | `00_README.md:3-6`（"负责人直接要求『在根目录创建一个文件夹…』"） |
| 存形态实例数 | 工作区本体 **1**；zip **0**；解包件 **0**；归档件 **0**；影子树实例 **0**；仅历史复原 **0** | `find 问题扫描 -type f` 实测；`工程控制/_control_packs/` 内无对应 zip |
| 文件数 | **742**（根 6 + `_cache` 43 + `_merge` 18 + `_verify` 371 + `_tools` 6 + `账本` 4 + `findings` 294；分项实测相加恰等于总数） | find 分目录计数 |
| zip sha256 | 不适用（无 zip 形态） | — |
| git 入库 | **742/742 全部被跟踪**（`git ls-files` 计数 = 742；`git status --porcelain --ignored=matching` 对本路径零行，工作树干净）→ 登记为**已入库工作区件**，非未跟踪件 | git 实测 |
| 首次出现提交 | `43c5c488`（2026-09-13 21:18:52）`docs(audit/RQS): 建立全项目只读问题扫描工作区与作业规程` | `git log --reverse --all -- 问题扫描/` |
| 最后出现提交 | `b67c32fa`（2026-09-15 02:50:35）`fix(audit/RQS): 我又把三条 P2(V19-N-09/10/11)写进 p1 目录…`（= 当前 HEAD） | `git log -1 -- 问题扫描/` |
| 触及提交总数 | 148（`--all` 与仅 main 计数相同 ⇒ 全部发生在 main，无分支携带） | `git log --oneline --all -- 问题扫描/` 计数；`git rev-list --count main -- 问题扫描` |
| 是否被删除 | 目录从未被删除；全史仅 1 个文件删除：`_verify/V15.md.part2`（临时分片），删除提交 `c8a1667a`（09-15 01:23） | `git log --all --diff-filter=D -- 问题扫描/` |
| 同名多形态一致性 | 不适用（单一形态）；但同一身份存在**两个时点口径**：INDEX.md 自述冻结于 `539626f0`（09-14 11:46），本体持续生长至 `b67c32fa`，差异见第二节实测对表 | `git log -1 -- 问题扫描/INDEX.md` |

## 二、包内文件地图（规格文件职责与数量口径）

规格文件（根 6 件；本组无 NN_XXX.md 包形命名，对应物如下）：

| 文件 | 自述职责（引其自述标题/首段要点，均单行内） |
|---|---|
| `00_README.md` | "全项目只读问题扫描工作区（READ-ONLY AUDIT SWEEP）"导航：代号、建立原因、目录结构、三级流水线（`:1` `:34-37`） |
| `10_PROTOCOL.md` | 作业规程 `RQS-PROTOCOL-001`（`:3`）：权威顺序（`:5-9`）、§0 硬性纪律、§1 九类别、§2 P0/P1/P2 判据、§3 finding 必填 schema、§4 深度覆盖、§5 档案文件头、§6 待复核、§7 现场冲突、§8 交付汇报、**再一个 §8** E 层授权边界 |
| `20_AGENT_PLAN.md` | 派发计划 `RQS-PLAN-001`（`:1`）：第一波 L01–L18/M1–M6 归属表（`:8-38`）、第二波横向轴 L19–L28 与 M7/M8/M9 映射（`:46-76`） |
| `40_OWNER_DECISIONS.md` | "需负责人裁决与授权清单（前台汇总）"（`:1`）：A-01..A-44、B-01..B-14、C-01..C-21、S-1..S-5、D-01..D-06 |
| `INDEX.md` | "定稿条目机械汇总（v4 · 全轴收工 + E 层 + FD）"（`:1`）：类别×优先级表、按产出方表、跨域主题唯一总述表（15 主题）、复跑命令 |
| `SUMMARY.md` | "系统性根因簇与残余风险（前台跨域综合）"（`:1`）：簇 1–10、残余风险三态表、未覆盖面、机制⑪–⑭ |

子目录数量与命名口径：
- `findings/` 294 件 = 9 协议类别目录 + **第 10 个类别目录 `C_ALG_IMPL`（协议外，无 README）**；pN 层 285 件（非 README 定稿 258 + pN/README.md 27 + 类别层 README 9）；命名第一波 `<合并码>_<L切片>.md`（如 `M1a_L01_L02.md`）、M7 系 `M7_<类别>_p<n>[_a|_b].md`（15 件，`_merge/M7.md:5` 自述"共 15 个文件"）、V 系 `V<N>[-b|-c].md`（59 件，含 6 件 C_ALG_IMPL）。
- `_cache/` 43 件 = L 档案 32（L01–L27、L28、L28b–L28e）+ E 层档案 4（E1–E4，B-14 授权实测）+ `F00_FRONT_SPOTCHECKS.md` + `README.md` + 数据件 5（`prio_mismatch.json`、`v12_fam.json`、`v12_mad.json`、`v12_scan2.json`、`v12_tracked.txt`）。
- `_merge/` 18 件 = 合并档案 14（M1a、M2a、M2b、M3、M3b、M4、M5a、M5b、M6a、M6b、M7、M8、M8a、M9）+ `00_COORDINATION.md` + `00_R_TIER_PLAN.md` + `ANCHOR_VERIFY_REPORT.md` + `CHANGED_FILES_WATCH.md`。
- `_verify/` 371 件 = `.md` 26（V1–V15、V17–V19 档案 + `V19_matrix_rows.md`、`V2_N01_STATUS.md`、`V9_gates_table.md`、FRONT_*.md 3、`DESIGN_check_numeric_constants.md`）+ `.py` 310（两族命名：`_v<NN>_*.py` 与 `scripts_v<NN>_*.py`）+ `.json` 26 + `.txt` 9。**V16 无独立档案**（其条目 V16-N-01 落在 `findings/G_GOV_GATE/p2/V5.md:32`）。
- `_tools/` 6 件：`verify_anchors.js`（"锚点机械核验器 v3"头注）、`gen_changed_watch.js`（BASE=`b32246c4`）、`gen_fix_ledger.py`（账本生成器）、`anchor_repair.js` v1 / `anchor_repair2.js` v2（"v1 因二次污染被回滚"头注）、`anchor_repair_manual.md`（557 对待人工复核）。
- `账本/` 4 件：`FIX_LEDGER.csv`（682 数据行×24 列）、`FIX_LEDGER.jsonl`（682 行，与 CSV 同数）、`FIX_LEDGER.md`（人读 P0 视图，自述 682/P0 93/P1 401/P2 185/其它 3，与 CSV 实测一致）、`README.md`（使用说明）。
- 本组**无** `tasks/`、`templates/`、`schemas/` 目录；对应功能由 `20_AGENT_PLAN.md`（派工）、`10_PROTOCOL.md` §3（finding schema）、`账本/README.md` §1–§2（账本列 schema）承载。

## 三、任务结构（RQS 无任务台账，等价物为修复账本与代理码体系）

- 账本：`账本/FIX_LEDGER.csv` **682 行 × 24 列**。派工令"约 574 行、22 列"与 `SUMMARY.md:203` 自述"655 行×24 列"均为更早时点快照；本报告实测 682×24（`账本/FIX_LEDGER.md:3` 自述同数，互证一致）。
- 24 列名（CSV 表头行实测）：`id, priority, category, producer, release_blocker, owner_decision, title, position, evidence, impact, clause, related, suggested_disposition, evidence_file, source_line_state, fix_state, fix_commit, fix_date, regression_test, fixed_by, fix_note, verified_state, verified_by, verified_date`；前 15 列为前台判定列、后 9 列执行侧处置列（`账本/README.md` §一）。
- 任务号族（producer 列实测）：M1a..M9 共 494、L28b–e 共 19、FD 共 8、V1..V19 共 161，合计 682；id 全局唯一（682/682，零重复）。
- 状态字面量分布：
  - `fix_state`：`OPEN` 660 / `FIXED` 20 / `PARTIAL` 2；README §二声明的枚举为 7 值（OPEN/FIXING/FIXED/PARTIAL/CANNOT_REPRODUCE/NEEDS_DECISION/DISPUTED），实测仅用 3 值。
  - `verified_state`：空 661 / `VERIFIED` 15 / `PARTIAL` 6（22 条已处置行中 `V5-N-03` 的 verified_state 为空）。
  - `priority`：P0 93 / P1 401 / P2 185 / `P?` 3（`V7-N-08/09/10`，对应正文 P3/P2 与"判否负清单"）。
  - `release_blocker`：Y 494 / N 188（README §三自述 344 为旧时点）。
  - `source_line_state`：空 627；非空 55 行中 **54 行为越列污染**（值如 `状态** OPEN`、置信度/related 残段，属生成器解析混入，非设计取值）。
- 依赖/顺序声明（包内）：三级流水线 L→M→前台（`00_README.md:32-37`）；第二波去重规则"横向轴只登记新实例、不得抢先定稿"（`20_AGENT_PLAN.md:64-65`）；账本处置顺序建议"先筛 P0∧release_blocker=Y"（`账本/README.md` §三）；R 层触发三条件与 R1–R8 互斥分片（`_merge/00_R_TIER_PLAN.md` §1–§2）。
- findings 条目标题格式：`### <代理码>-<三位序号> <标题>`（`10_PROTOCOL.md` §3）；实测 M 系用 `##`、V 系用 `###`，INDEX 口径"两者都计"（`INDEX.md:6`）。

## 四、门禁与验收要求（全部为工作区自设规程，非仓库 CI 门）

机器门（工作区内可复跑件）：
1. 锚点核验：`_tools/verify_anchors.js`（v3，五档处置强度；`INDEX.md:60` 声明"①③档禁止撤条"；最近一轮结果自述 `ANCHOR_VERIFY_REPORT.md:4`：297 份 md、12254 次引用、精确在位 8720；`00_R_TIER_PLAN.md` 采样行 refs=10913/mismatch=340/absent=409 为另一时点）。
2. 并发改动面：`_tools/gen_changed_watch.js` → `_merge/CHANGED_FILES_WATCH.md`（表 A 必复验/表 B 未点名/表 C 工作量；基线 `b32246c4`、当前 main `b8d1eb04`；`00_R_TIER_PLAN.md` §1 声明 R 层开工前必须重跑刷新）。
3. 账本生成器：`_tools/gen_fix_ledger.py`，重跑规则"按 id 保留处置列、只刷新判定列"（`账本/README.md` 生成器条）。
4. 结构化文件判读令："机器文件一律用解释器解析、禁 read 行文本"（`00_R_TIER_PLAN.md` 硬规则②，根因 read >2000 字符单行截断，另见 `SUMMARY.md:152`）。

条目纪律（验收判据）：
- 入档门：无 `文件:行号` 证据 + 逐字摘录不得成条（`10_PROTOCOL.md` §0.5）；违反 §0 硬性纪律"即该代理档案作废"（§0 标题行）。
- 覆盖门：每 slice ≥10 条 finding，不足须 §5 覆盖声明逐文件列"未见问题"依据（`10_PROTOCOL.md` §4）。
- P0 判据（R 层沿用）："断言可为假 + 条文在位 + 后果达交付面；三缺一即降"（`00_R_TIER_PLAN.md` §4）。
- 改判门：合并代理须逐条重读原文核对摘录，不吻合者剔除或降级并写明理由（`10_PROTOCOL.md` §8 合并代理条）；PATH_MISMATCH/SYMBOL_ELSEWHERE 两档不是撤条理由（`00_R_TIER_PLAN.md` 硬规则①）。
- 回归锁硬要求：`regression_test` 列必填，`ADDED:<名>` / `NOT_NEEDED:<理由>`，"没有回归锁的 FIXED 会被 R 层降级为 PARTIAL"（`账本/README.md` §二）；实测 20 条 FIXED 行 20/20 已填。
- 完成顺序：控制包式完成链在本组**不存在**；等价物为 R 层触发三条件（HEAD 连续两轮不变 + 真源脏=0 + 全部代理交档）（`00_R_TIER_PLAN.md` §1），且 `SUMMARY.md:174` 自述"R 层复验——触发条件尚未满足/未开工"。
- 豁免机制：本组自身无 waiver；E 层执行属对"禁执行令"的一次性特批 **B-14（负责人授权）**，白名单命令与新增禁令见 `10_PROTOCOL.md` §8(第二个)、`00_R_TIER_PLAN.md` §5（禁非自建构建树一切 ctest 形态、禁 `ci/run.py` 非 `--plan-only`）。
- 提交纪律：`10_PROTOCOL.md` §8(E 层) 自述"提交只由前台在本目录路径上做"；实测 148 个触及提交全部在 main、消息统一 `docs(audit/RQS)/fix(audit/RQS)` 前缀。
- 账本红线："不要删行、不要改 id"（作废走 `fix_state=DISPUTED` + 反证锚）；"不要动 问题扫描/** 下除本账本以外的任何文件"（`账本/README.md` §四）。

## 五、设计意图（转述，不评价）

- 目标：全项目"搜寻所有问题按类别与 P0/P1/P2 归档"，模式"只读研究，零执行"（`00_README.md:3-4`）；权威顺序宪章最高、历史约束件 ARCHIVED_NON_NORMATIVE 让位（`10_PROTOCOL.md:5-9`）。
- 禁止项：任何执行（含 git 只读子命令，理由"可能触碰 index.lock 干扰并发 agent"）、只写自己档案、不改科学定义、并发现场免报路径清单、禁止臆测（`10_PROTOCOL.md` §0.1–§0.5）。
- 演化自述：第二波"由已发现问题反推的 10 类可穷举失败模式"追派横向轴（`20_AGENT_PLAN.md:48-51`）；E/V 层由负责人 B-14 授权把"无法判定"落为四态（`10_PROTOCOL.md` §8E、`00_R_TIER_PLAN.md` §4）。
- 负责人裁决/指令痕迹：①建区本身=负责人直接指令（`00_README.md:4`）；②**A-34 已裁定"影子树 AGENTS.md 不改名、仅登记为问题"**（`40_OWNER_DECISIONS.md:48`，落地提交 `cb4405d8` 09-14 10:23）；③**B-14 一次性授权**（`10_PROTOCOL.md:116`、`40_OWNER_DECISIONS.md:75`，同提交 `cb4405d8`）；④"登记为问题，不处置（负责人 2026 裁定）"残见于账本 source_line_state 列 1 行；⑤P5-SNR 改冻结文档获负责人授权并已在 `CHANGELOG.md:9-11` 登记（`40_OWNER_DECISIONS.md:190` 称之为合规形态样板）。
- 待裁痕迹：**A-44**——两族"负责人裁决 2026-09-14"（PSF-FAST-001 12 处援引、P9 帧头 WCS 6 处援引含合同内自指导引文）在 `memory.md`/`CHANGELOG.md`/本档登记面**全部零命中**，真伪待裁（`40_OWNER_DECISIONS.md:186-190`、`SUMMARY.md:190-193`）。
- 对前代继承/废止：本组**无任何对控制包谱系的继承或废止声明**；与 `工程控制/ACTIVITY_STATE.md`、`工程控制/包规范.md` 零互引（两文件对"问题扫描/RQS" grep 命中 0，后者 mtime 09-13 13:14 早于 RQS 首提交 21:18）。
- 自我可信度条款：前台公开自纠记录（F00-03 计票过期、F00-07a 凭空测试名撤回、A-05 前台自造条目撤回、D-06 归因改判"原因未定"、V18 四次自纠）并升为纪律"归因与事实两回事"（`SUMMARY.md:147-154`、`40_OWNER_DECISIONS.md` D-01..D-06、提交 `14102f65`/`fc645a5a`/`fa8db3f7` 消息）。

## 六、代际差异表（RQS 内部波次演进；无可对比的前代包）

| 维度 | 第一波（09-13 21:18 建区） | 第二波（09-13 21:40 起） | E 层（09-14 10:23 B-14 落地） | V 层（09-14 起至 09-15） |
|---|---|---|---|---|
| 规格文件 | `00_README/10_PROTOCOL/20_AGENT_PLAN`（`43c5c488`） | 同 + 第二波轴表追加（`cd328f81` L19-L25、`68fc6493` L26-L28） | `10_PROTOCOL.md` 追加第二个 §8 E 层边界（`cb4405d8`） | `40_OWNER_DECISIONS.md` A/B/C/S 编号持续追加 |
| 叶子代理 | L01–L18（18） | 至 28 码、L 档案 32 件（L28 拆 b–e） | E1–E4（4 件，白名单实测） | V1–V19 轴（`_verify` 档案 26 md，**V16 无独立档案**） |
| 合并域 | 计划 M1–M6；实存 M1a/M2a/M2b/M3/M4/M5/M6 拆分件 | +M7/M8/M8a/M9（`20_AGENT_PLAN.md:63,75-76` 两次改映射：M8 先=L22..25 后=L22+L23、L24..25 改归 M9） | 不经 M 层的 FD 前台自证条 | V 系条目直接落 findings，不经 M 层 |
| 类别 | 协议 9 类 | 同 | 同 | **新增第 10 类 `C_ALG_IMPL`**（6 件，协议无定义、无 README，见 `findings/C_ALG_IMPL/p1/V10.md:1-5`） |
| 优先级档 | P0/P1/P2 三档 | 同 | 四态判据（仍成立/已修复/部分修复/无法判定，`00_R_TIER_PLAN.md` §4） | **出现 P3**（`C_ALG_IMPL/p3/` 1 件；账本 `P?` 3 行）；抽取器规则"目录是权威，按目录定级"（`b67c32fa` 消息） |
| 状态字面量 | finding `建议优先级` 字段（§3 schema） | 同 | 四态 | 账本声明 7 值、实际 OPEN/FIXED/PARTIAL + VERIFIED/PARTIAL |
| 执行纪律 | 零 shell 零 git（§0.1） | 同 | B-14 白名单只读命令 | 纯静态 python3 + `git --no-optional-locks`（各 V 档案头自述，如 `_verify/V1.md` 纪律行） |
| 规模 | v4 时点 197 定稿件（非 README）/520 条（M/L28/FD 口径） | 同 | +FD-F-003 等 | 现树 258 定稿件（非 README）/682 条含 V 层 |

## 七、执行留痕三方核对

口径（本组无 `pack_commit_link.csv` 行，四表 0 命中，全部自算）：提交集合 = `git log --all -- 问题扫描/`（148 个）；一切含 "RQS" 字样的提交消息 = 157 个（`git log --all --grep=RQS`，多出者为真源提交引用 RQS 编号，如 `fbfcfac0`/`0e061182`/`c1959436`）。注：LEDGER_RE 只认 `NN_TASK_LEDGER*.csv` 的采集盲区对本组天然不适用——本组账本名 `FIX_LEDGER.csv`，按该正则的采集器必漏采，故本报告未依赖采集面、全部实测。

1. **账本 ↔ findings（一致）**：账本 682 id 与 findings 条目标题 id（按 `M*/L28*/V*/FD*` 前缀归一）为**全等集**：两侧差集各 0（脚本实测）。每行 `evidence_file` 指回 findings 路径且 682/682 存在于磁盘。
2. **账本 ↔ git（处置面）**：非 OPEN 22 行，`fix_commit` 唯一 6 SHA：`07eb229b`(B1)、`adaeb531`(B2)、`dce8abd4`(B3)、`c3452d48`(B4)、`fbfcfac0`(P18, V2-N-01)、`0e061182`(P19, V5-N-03)——全部存在且全部是 main 祖先（`git merge-base --is-ancestor` 逐一实测 yes）；提交消息含对应编号（B1 消息直写 M9-H-1/H-2；P18/P19 直写 RQS 编号）；回填提交 `e91b1bf8`（09-15 01:24）仅改 2 行 CSV。**"有账无据"= 0**（无缺 fix_commit 的非 OPEN 行）。瑕疵 1 条：`V5-N-03` FIXED 而 `verified_state` 空。
3. **git ↔ 账本（有据无账）**：`fix(RQS/B7)` = `c1959436`（09-14 15:10，F-14 内存回收门修正）不在账本任何 fix_commit 中；`40_OWNER_DECISIONS.md:122`（A-42）自证同一事实"账本亦无 B7/`c1959436` 行"。另 `C-13`（`40_OWNER_DECISIONS.md:129`，引 V6-N-10）自述"本轮 9 个改码提交账本 ref=0"（此 9 为前台自述数，本报告未逐一复核，标**未证实**）。
4. **evidence/ 留痕**：`evidence/` 现仅 `ciqa/ refactor/ v6_1_rework/ v8_1_ci_control/` 四域，**无 RQS 专属目录**（RQS 的留痕即其 git 历史与 `run/审计执行层/E*/` 工作目录，E2/E3/E4 档案头自述）；`grep -rl` 在 `evidence/` 的唯一命中为一个 zip 内二进制伪匹配，非留痕。
5. **抽查互证例**：`9777193d`（V5 落账 M9-H-1/H-2 VERIFIED ↔ 账本两行 verified_state=VERIFIED）、`fa8db3f7`（FD-G-003 撤销升档归位 p2 ↔ 现树 `G_GOV_GATE/p2/FD_shadow_agents_md.md`）、`fc645a5a`（FD-F-003 新立 P0 ↔ 现树 `F_TEST_GAP/p0/FD_windows_zero_test_green.md`）、`f59f9968`（V10 纠正 M9-H-2 口径 ↔ 账本 M9-H-2 FIXED/verified_state=VERIFIED）。
6. **分类计数**：一致＝账本↔findings 682/682 全等、22 条处置记录中 21 条证据链完整；有账无据＝0；有据无账＝1 例确证（`c1959436`）+ 9 例自述未复核。

## 八、缺口与不确定

1. **INDEX v4 数字已过时且口径复合**：`INDEX.md:4` 自述 224 份/520 条/P0 81。在 v4 提交 `539626f0`（09-14 11:46）实测：非 README 定稿件 **197**，加 27 份 pN README 恰 =224——"224 份定稿件"须把 README 计为文件方成立（INDEX 产出方表又写"README 0 条"，两种口径并存）。现树实测（`b67c32fa`）：pN 文件 285、M/L28/FD 口径条目 **521**（FD 变 8；差值可精确归因：`fc645a5a` 增 FD-F-003，F_TEST_GAP P0 14→15）；G_GOV_GATE P1 57→56、P2 8→9（FD-G-003 归位改档，`fa8db3f7`）；其余产出方逐项（M1a44/M2a46/M2b29/M3 43/M3b22/M4 31/M5a22/M5b43/M6a31/M6b22/M7 78/M8 26/M8a31/M9 26/L28b5/L28c4/L28d3/L28e7）现值与自述**全等复现**。
2. **V 层/E 层完全不在 INDEX 口径内**：findings 中 V 前缀条目 161 条 + 非条目说明性子标题（全 `##/###` 实测 710，M/L28/FD 口径 521）不计入 520；且 `INDEX.md:6` 称"E 层条目按 `【E<n>-实测】` 前缀识别"，实测 findings 标题中该前缀 **0 命中**（E 层成果以 V 编号存在）——该识别规则与树现实不符。
3. **协议外类别与档位**：`C_ALG_IMPL`（九类之外第 10 目录，6 件、无 README）与 `p3/`（三档之外第四目录，1 件；账本以 `P?` 3 行承接）；未找到立类/立档的授权文本——来源**未证实**。
4. **V16 缺档案 + 三处定级不一致**：V16-N-01 正文自述"优先级 P1（从 P2 升）"，但其位于 `p2/` 目录且账本记 P2（`findings/G_GOV_GATE/p2/V5.md:32-33` ↔ 账本行）；`_verify/` 无 V16.md。按 `b67c32fa` 规则"目录是权威"，正文升档声明未生效——升档声明与落档矛盾的原因**未证实**。
5. **账本 source_line_state 污染**：54 行含越列文本（如 `状态** OPEN`、置信度/related 残段），属生成器（`_tools/gen_fix_ledger.py`）抽取瑕疵；未见对应改判记录，根因**未证实**。
6. **账本/README 自述滞后**：`账本/README.md`（mtime 09-14 12:45）自述"521 条（P0 82/P1 262/P2 118）"、"release_blocker=Y 共 344"，现 CSV 实测 682（93/401/185）与 Y=494——同一目录内 README 与数据面两个时点并存。
7. **派工令自述数字与实测不符**："约 574 行、22 列"实测 682×24；"9 个类别目录"实测 10 个；"L01..L28 等叶子档案"实测 L 档案 32 件（L28 拆 b–e）另加 E1–E4/F00。
8. **读不到/未跑**：`10_PROTOCOL.md` 出现两个 `## 8`（L107 与 L116，编号重复）；`_verify/` 310 个 py 仅登记存在与头部自述用途（"纯静态解析"，如 `V19.md:3` 纪律行），按派工令**未复跑**，可否只读复跑**未验证**；外网受阻未核标准原文 4 处（`SUMMARY.md:169`）；B 组 14 项中 ~~B-06~~/~~B-09~~ 两行"已答且被证伪/已答"与同编号待答行并存（`40_OWNER_DECISIONS.md:59-69`）。
9. **R 层未开工**：`SUMMARY.md:172-174` 自述已收工 27 轴 + 14 域，但"R 层复验未开工、触发条件未满足"→ 全部 682 条的四态重判未完成，账本 verified_state 覆盖仅 21/682 属预期形态而非缺陷确证。
10. **digest 对账不可能**：本组无 zip、无 digest 文件，sha256 对账项**不适用**（非缺失）。

---

## 片尾一 · 与上一组（G15 元控制层）的衔接

- G15 最后对象 `工程控制/ACTIVITY_STATE.md`：对"问题扫描/RQS" grep **0 命中**；其 mtime `09-13 13:14` 早于 RQS 首提交 `43c5c488`（09-13 21:18）约 8 小时——**唯一活动包登记源不收录本身份**，与派工令"本组身份不在包体系内"互相印证（`ACTIVITY_STATE.md` §2 状态表仅 9 行包/模板条目，无 RQS 行）。`工程控制/包规范.md` 亦 0 命中。
- 同期交叠（事实面）：`ACTIVITY_STATE.md` §2 行 1 记载负责人 09-13 指令改以 `AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912` 为权威执行包；RQS 与 RESCUE-V3 **并行运行**——RQS 修复批 B1–B4（`07eb229b`/`adaeb531`/`dce8abd4`/`c3452d48`，09-14 13:29–16:19）与 RESCUE 任务链提交（`f849860a` P1-003、`b0353303` P5 09-14 19:09、P8–P19 各批）交错于同一 main 线性历史；RQS 的 V 层正以 RESCUE 期提交为复核对象（`_verify/V1.md:4-5` 复核对象=b0353303+35c85f53；`40_OWNER_DECISIONS.md:110-115` A-40 直接针对 P5/P8 落点）。CONSTITUTION-ALIGNMENT-V1（09-13 被 RESCUE-V3 取代，`ACTIVITY_STATE.md` §2 行 2）与 RQS 无互引（四张包表 + ACTIVITY_STATE + 包规范 0 命中）。
- 控制形态差异（客观描述）：G15 诸件为"zip 派发 + 台账 + 机器门"的包形控制；RQS 为**负责人直接指令 + 前台自建 git 跟踪工作区 + 三级代理流水线（L→M→前台）+ B-14 一次性执行授权 + 可复跑核验脚本 + CSV 修复账本**（`00_README.md:3-6`、`10_PROTOCOL.md` §0/§8E、`20_AGENT_PLAN.md:3`、`_tools/` 6 件、`账本/README.md` §1–§2），即派工令所称"控制机制最新演化形态"的形态与文本证据；其登记缺口（根目录条目未入 AGENTS.md 目录规范）由工作区自行立案为 **A-09b/C-09**（`40_OWNER_DECISIONS.md:44,92`），待负责人终裁。

## 片尾二 · 重叠身份

本组仅 1 个身份（`问题扫描/` = RQS-2026-01）。与 49 身份体系及各组 zip/解包/归档形态**无重叠**（`pack_inventory.csv`/`pack_lineage_table.md`/`pack_commit_link.csv`/`pack_events.md` 四面对"RQS/问题扫描"命中实测 0）；本组无 digest，故"与本组 digest 是否一致"项为**不适用**。唯一近邻关系：RQS 的审计对象面读入前各组留存件（如 `evidence/v8_1_ci_control/`、`reports/REAUDIT_V3/`，见 `_merge/M7.md` §1-2 与 `SUMMARY.md:18-19` 的引用），属"读入线索"而非形态重叠。

—— 报告完（全部实测基于 HEAD `b67c32fa`，工作树仍在并行推进，后续提交可能再次移动第三、七节数字）
