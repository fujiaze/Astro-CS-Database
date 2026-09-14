# STAGE-06 V8 与 CI 化期（V8.1 CI 控制包与 superseded 副本；V8.0 未实体化）

对应组：G12。对应任务一历史阶段：STAGE-07（seq 1521–1785，09-05→09-09，reports/history/00_OVERVIEW.md §1 表行 7）。

## 1. 时间窗
- PACKNEW seq 1521（a4fdee3f，09-05，engineering/control/active/ 56 文件唯一批量入库；zip 与 工程控制/ 解包件不入库）；ledger 落账窗至 09-09 前后（history STAGE-07 的 k/140 冻结波与"记账线终结 1780"同期）；8e03d7da（seq~1800s 前，09-10 02:22，R100）把 active 件移入 archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905（身份 B 唯一 git 事件）。
- V8.0（AstroCS_ALPHA0.11.0_CI_VM_MIGRATION_CONTROL_V8_20260905）全仓 0 文件/0 git 路径/0 digest，只在 15_SUPERSESSION_NOTICE.md:3 有名字（P-G12 §八）——V8 代实体只有 V8.1。

## 2. 包清单（2 身份，4 实例）
身份 A：worktree（工程控制/ 56 文件）+ zip（_control_packs，77a4b03a…，296,398 B/56 条目，未跟踪）+ recovered@8e03d7da^ 类历史复原 + 内嵌 V7.1 zip（baseline/，G10 重叠）；身份 B：superseded 归档副本 57 文件（与主形态唯一差异=README_ARCHIVED.md，56 共有文件哈希全等，台账 blob 自 a4fdee3f 至 HEAD 未变，P-G12 事实 2）。

## 3. 设计意图（转述）
- 包名即主张 EXISTING_WORKSPACE：原地接管现存工作区（00 L5、01_FROZEN_CONSTRAINTS.md:13-17"沿用仓库当前规范、只补最小必要目录"、00 L41"控制包不得解压覆盖项目源码"、tasks/01:38-39 V81-ADOPT-005）。
- 对归档治理零内容引用：与 G11 容器同父目录兄弟、grep 容器名/工程控制/engineering-control 全 0 命中（P-G12 衔接段）。
- baseline/ 全部指向 V7.1 与审核快照 257ae1f4——本代不指向 v1.3–v6.1 任何子包。

## 4. 任务规模与状态字面量（前台实测同）
- CONTROL_TASK_LEDGER.csv 36 行/12 列/状态只有 NOT_STARTED（36/36）；12 号族统一 V8-/V81- 前缀（validate_control.py:61-64 显式禁与 V7.1 号冲突——对跨代同号问题的直接堵法）；50 条依赖边、最长链 24、号序≠拓扑序（V8-CI-011 第 17 行、V8-CI-010 第 18 行）。
- 依赖分隔符 V7 的";"改回"|"；status 列回归（V7 无）；owner/mode/allowed_paths/acceptance_command/timeout_s/heavy/gate 七列新增。

## 5. 门禁口径
- 85%/60% 利用率数值门与 cprun 字样在包内全形态（含 evidence 2,153 文件）0 命中——包内只有定性条款（00 L28/49、01 L32-33/45、07 L48-50、09 L26、12 L7）；数值门实体在宪章 :375(§10.5)/:656(§18.2) 与 tools/monitoring/run_monitored.py:453/455、cli/resource_gate.h:108/111，引入提交 1a590f63(09-09)/91440c16(09-11)/d8c821db(09-10) 均不早于本包（P-G12 事实 6）。
- 包自检：4 形态 2 通过/2 失败（归档副本 CONTROL_FAIL: README_ARCHIVED 不在 manifest；复原件 CONTROL_FAIL: baseline/REVIEW_CLAIMED_TASK_STATE.csv）（P-G12 §八）。

## 6. 与上一代差异
文件 99→56、规格 24→16（15 个同号改名清单见 P-G12 §六表）、validators 18→4、tasks 13→4、新增 baseline(8)/ci(8)、取消 checklists/package_policy/science/schemas；任务 191→36（wave 体系 W0–W8→ADOPT/CI/CIQA/FIX/RESUME/AUD/FAT/RELEASE）；资源口径 resource_class 三值→heavy/mode 两值。

## 7. 执行留痕概况
- pack_commit_link.csv 无 V8 行——非"真无台账"，系 LEDGER_RE 不匹配 CONTROL_TASK_LEDGER.csv 的采集盲区（G12 用正则实测 match=None 定案，连带 inventory 台账列空、状态分布{}）；自算三方：台账 36 号 vs evidence/v8_1_ci_control（2,153 文件，按任务号分目录）vs 2,045 提交——齐备 17、错位 2（V81-ADOPT-001 在 adoption/、V8-CIQA-001 在包外 evidence/ciqa/CIQA_REPORT.json verdict=PARTIAL）、有账无据 17、有据无账 0；命中率子串 55.6%/方括号 50.0%。
- V7 的 191 号在本代留痕域收口（TASK_STATE.json/STATE_RECONCILIATION.csv/COMMIT_LEDGER.jsonl 191/191），"上一代的账在下一代里核销"是本期结构特征（P-G10 事实 4 + P-G12 §七）。

## 8. 与下一代衔接
本代 active→archive 的移动提交 8e03d7da 同时是 ACTIVITY_STATE.md 的首次入库（+95）与 V7 判 ARCHIVED_DISARMED 之处；目录名日期 09-09 与提交日期 09-10 差一日历日（P-G12 衔接段）；宪章冻结入库 seq 1786（d8c821db，GOV-001）后，控制包转入 STAGE-07 宪章期。

## 9. 缺口（P-G12 共 16 条）
最关键：V8.0 包体不存在（代际差异无法核对，未证实）；1,921 个留痕文件被 gitignore（可核面受限）；G-CI/G-FIX/G-FAT/G-REL 无判定记录；ACTIVITY_STATE/DOCUMENT_INDEX 对"superseded→RESCUE/宪章包"的改指发生在 09-09..09-13（与本代尾部交叠，见 STAGE-07）。
