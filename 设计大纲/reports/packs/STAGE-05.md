# STAGE-05 V6–V7 重构期（V6 系统重构 / V6.1 返工 / V7 模块化重筑基 / legacy 归档容器）

对应组：G08、G09、G10、G11。对应任务一历史阶段：STAGE-06（seq 1261–1520，08-30→09-05，reports/history/00_OVERVIEW.md §1 表行 6）。

## 1. 时间窗
- V6 PACKNEW seq 1261（4b1b948e，08-30 23:20，基线 587fe0e3 后 20 分钟）→ 执行窗 08-30→08-31（evidence/refactor LEDGER 80 条 seq 1261–1355）→ V6.1 引入（zip 08-31；worktree 8c71f7ab 09-02 02:12；LEDGER 60 条 seq 1361–1458）→ V7 审核收口（02 号审计稿"当前基线不可作发布候选"）→ b7b2dea7（seq~1480，09-02 22:15）legacy 容器归档（854 R100+1A）与 CONTROL_V6/V4/RELEASE_V5/REAUDIT_V3 入容器 → a4fdee3f（09-05）V7.1 zip 以 V8.1 baseline 内嵌形态首现（pack_events.md / P-G10 §一）。
- V6→V6.1 空窗 575 分钟（history STAGE-06 判据，seq 1360→1361）。

## 2. 包清单（6 身份 + 容器 1）
V6（3 实例，zip fdad8e40 与归档件 46 文件逐哈希全等、复原件仅多 _PROVENANCE.json、run/ 影子树 72 份；CONTROL_V6=外层容器身份）；V6.1（zip 47/解包件 46，差异=缺 02_FINDINGS.csv+validate_return_audit.py 改版 4 hunk）；V7 两身份（zip 系 4 实例+worktree；99 路径名集合一致、98/99 同字节、唯一差 TASK_LEDGER.csv CRLF↔LF=189 B）；容器（855 文件/9 子包/未删除，无 zip）。

## 3. 共同设计意图（转述）
- V6（00、02）：以 587fe0e3 为硬绑基线、ARCHITECTURE_REFACTOR_REQUIRED 判定；把执行态移出包体（00 §3 要求台账复制为 evidence/refactor/TASK_LEDGER.csv，令包内 SHA256SUMS 可长期复算——实测 44/44 全对，P-G08 事实 3）。
- V6.1（00_READ_FIRST:12）："上轮结果正式打回，不得继承 81 PASS、Linux 侧完成、G11 PASS"——被点名状态实物定位完成（V6 终态执行台账 8be6eb4e：88 行=PASS 81/WAITING_WINDOWS 6/REVIEW_PENDING 1；出处 b16d422a docs/review/RELEASE_STATUS.md:6-9，P-G09 衔接段）；MANIFEST v2 新增四绑定字段（prior_control_sha256/reviewed_audit_sha256/reviewed_audit_reported_commit/task_ledger_sha256）。
- V7（02 §19、00 抬头）：进度包可作重构输入基线但不可作发布候选基线；废止 V4/V5/V6/V18/V19 历史控制文件覆盖；禁 REWORK_* 状态；链代方式从上包 SHA 改为"源码提交 SHA c1696156+包内审计稿"（无 prior_control_sha256 字段）。
- 负责人指令痕迹：ACTIVITY_STATE §2 记 V6.1 ARCHIVED_SUPERSEDED、V7 ARCHIVED_DISARMED（8e03d7da）；memory item9 记 owner"重新修订工程包"（32a252f8）（P-G10 尾段一）。

## 4. 任务规模与状态字面量口径（前台实测已锁定）
- 规模链 98(V5)→88(V6 内台账全 NOT_STARTED)→67(V6.1，45 号与 V6 同名)→191(V7，51 号与 V5/V6/V6.1 同号)→容器并集 199。
- 状态口径三代三式：V6 包内零状态+包外执行态（PASS/WAITING_WINDOWS/REVIEW_PENDING）；V6.1 回列内 status（全 NOT_STARTED）；V7 删 status 列、状态外置 TASK_STATE.json（11 值枚举三处一致）——"台账=静态定义单源"。
- Gate 链：V6 12 门/105 复选项 → V6.1 11 门/96 项（取消 G11）→ V7 GATE_REQUIREMENTS.csv 独立 9 门（G3 91 任务，禁 task 依赖 G0–G8）。

## 5. 门禁演进
- 校验器 3(V5)→5(V6，新增 validate_task_graph+selftest)→6(V6.1，新增 known_failure_scan、validate_return_audit 替代 validate_audit)→validators/ 18(V7，清单 13 vs 实存 18 不齐)。
- V6 堵法：四类 commit 必须相等（12 §5）、--baseline-sha256 台账冻结、包内负例自检（fixtures 1 负 2 正）；冲突实测：validate_task_graph.py 判执行台账 REL-004 'REVIEW_PENDING' 非法（V5 遗留字面量不在 V6 schema 枚举）、11 份留痕带 schema 禁止的 reverify 字段（P-G08 事实 6）。
- V6.1 终局门版本差：zip 密封版 validate_return_audit.py 判已交付审核包（24c7eec7 入库 98216e5f…，1309 条目）AUDIT_PACKAGE_FAIL: banned path evidence/build/summary/lnx001.md；解包件改版（第 3 hunk 豁免 build/history，注释自指 12_AUDIT_PACKAGE_SPEC.md:22-40）判 PASS…NOT_READY——包内条文与密封脚本互相矛盾（P-G09 事实 3）。

## 6. 与上一代差异（表见各报告 §六）
98→88（族 23→21，去 P3/ISA/BENCH 增 BAS/CORE/IO/LEG/DATA/TEST）；capsule 概念在 V6 消失、改单一 audit zip；V6→V6.1 规格文件 13 删/10 新增、同名 9 件仅 01 约束文件逐字节相同；V6.1→V7 规格 15→24、台账 67→191、schemas 7→9、templates 14→9+新增 checklists/science/package_policy 三类专目录；V7 起包自检=解包件形态 CONTROL_FAIL（TASK_LEDGER 行尾）而 zip 形态 PASS（P-G10 事实 2；digest_verify 已把该 mismatch 如实冻结）。

## 7. 执行留痕概况
- 命中率：V6/CONTROL_V6 100%（88/88、337 提交）但 29 号与 V5 重号、45 号与 V6.1 重号，WIN-002/003/005 全部提及早于 V6 引入（P-G08 事实 4）；V6.1 无 CSV 行——机制查明为 _tools/02b LEDGER_RE 只认 NN_TASK_LEDGER*.csv 不匹配 03_REWORK_TASK_LEDGER.csv（登记口径缺口非执行缺口，P-G09 事实 4；自算 67 号命中率按日期窗 92.5%）；V7 87.4% 不可复现（严格全号 62.3%，index.jsonl 抽取器拆解多段号致 CAT-GAIA-* 误判未命中，7c3d66e2 为例，P-G10 事实 5）；容器 95.5% 恰等于四成员并集（包含关系非独立证据，P-G11 事实 4）。
- 三方：V6 一致 82/有账无据 6（WIN-001..006 无留痕目录）/有据无账 1（REL-CONSISTENCY 内书 G11-CONSISTENCY）；V6.1 一致 60/有账无据 2/有据无账 3（CPU-006/007/008 系 09-09 回填）+REL-003 状态冲突；V7 evidence/ 无 v7 目录、191 号全部出现在 V8.1 线留痕（CLOSED 105/NOT_STARTED 84/FATDUCK_PENDING 2——后者不属 V7 枚举，P-G10 事实 4）；V7 完成度三口径并存 38/140、42/140、105/191（P-G10 事实 6）。

## 8. 与下一代衔接
V7→V8.1 双通道：ALPHA3 zip 作为 V8.1 baseline 内嵌文件（4 落点同 sha 005448fb，validate_control.py:264 常量）+ V7 台账以 baseline/V7_1_STATIC_TASK_LEDGER.csv 再现（G10/G12 互证一致）；容器与 V7 仅有"归档说明指向"（b7b2dea7 消息 0 次 V7 字样、替代表断链由 GOV-002 于 09-09 修正，P-G11 衔接段）。

## 9. 缺口（P-G08 15 + P-G09 15 + P-G10 16 + P-G11 16 条）
最关键：V6 期审核包实物不在库；V6.1 进度包 zip 不在库（65b2c214 无法核对）；容器 README_ARCHIVED 自述 854 vs 实测 855；P11-004 review bundle zip 全史 0 blob 致容器内 v1.3 机器门 exit=2；ACTIVITY_STATE §3"根 CMakeLists 追加块未提交"与现工作树零 diff 矛盾（G15 后续闭合）。
