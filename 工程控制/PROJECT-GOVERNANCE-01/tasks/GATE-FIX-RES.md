# 任务：GATE-FIX-RES 资源门权威落点补齐与实现纠错（R-4 结论落地）

状态：NOT_STARTED　层：L1　依赖：R-4（已完成）
互斥组：S8-A（`docs/plugins/infrastructure/21_observability.md` + `contracts/resource_gate_v1.json` + 资源件实现）；`ci/**` 部分转 CI-003

## 依据（负责人指令：文档必须正确，不要遵循旧制）

证据：`reports/PROJECT-GOVERNANCE-01/research/R-4_资源门与观测权威落点.md` + `run/PROJECT-GOVERNANCE-01/R-4/**`（E0–E9 真跑）。

**核心事实**：85%/60%/10s/32MB/s 四个数在整条权威链里**零命中**——`ENGINEERING_SPEC.md:137` 写 「见最高设计 §8」，而 `ASTROCS_DESIGN.md §8` 是纯定性条款、无一个数字 ⇒ **委派链断了一环**。这是**文档缺陷，补上**。

## 要做（逐条）

| # | 做什么 | 落点 |
|---|---|---|
| 1 | 数值唯一源：阈值（含判定域与分母取义）写入 `contracts/resource_gate_v1.json`，三处实现共读 | `contracts/**` |
| 2 | 判据权威：新增 `§8 重计算负载资源门（G-RES-01）`（判据/判定域/分母语义/豁免） | `docs/plugins/infrastructure/21_observability.md` |
| 3 | 补上级指针：`ASTROCS_DESIGN.md §8` 加一句指向第 2 项与数值唯一源；`ENGINEERING_SPEC.md:137` 改指同处 | 两份文档 |
| 4 | **实现纠错（真缺陷）**：`--gate-required` 把**机器有效核**当已分配容量 ⇒ 任何 worker 数 < 机器核数的并行任务必然判红（实测 2 线程满核 14 s → U=6.3% → `rc=10`）。改为分母 = `granted_workers`（哨兵 0 回落 `min(selected, available)`，事件回显三分量） | 资源件实现 |
| 5 | 五处判据对齐：C++ 独有 `p50≥90%` 与逐样本 `85%/70%`、5 s 豁免 vs 10 s 严格界、`32 MiB/s` vs `32 MB/s`（差 4.86%）、方向 `>` vs `>=` | 三处实现 + 第 1 项契约 |
| 6 | 删死代码：`--resource-detail`（真 CLI `unknown flag` rc=2）与恒空 `curve_points`、`build_curve()`（生产零调用）⇒ 删除；曲线唯一载体 `resource_timeseries.csv` | `cli/**` |
| 7 | 命名统一下划线（改文档不改语义）：`resource_timeseries.csv` / `resource_summary.json`（实现侧 `resource_samples.csv` 一并改名） | `docs/**` + `cli/**` |
| 8 | 名录如实：`ASTROCS_DESIGN.md §9` 七件套中 `run-plan.json / run-graph.json / run-trace.jsonl / artifact-manifest.json / run-summary.json` 实测**一个都没产出** ⇒ 标 `NOT_IMPLEMENTED` 或移出名录登记退役 | `ASTROCS_DESIGN.md §9` |
| 9 | 登记（不改）：`tests/backend/test_p2007_joint_gate.py:112-116` 走旧命令树 `phase2 run`（rc=2）⇒ 转 TEST-CLI-SYNC-2 | 报告 |

## 门禁自审落地（R-4 §4）

- **G1 保留门、收窄硬失败**为三条可客观判定判据（① 单活跃线程；② 连续 >10 s 窗 <60% 且队列有工作；③ 分配曲线 ≥32 MiB·s⁻¹ 且回落不可解释）；**85% 均值门降级为「必须记录 + 超标须登记」**（依据 DESIGN §8:412 + 项目自测 16-worker 真负载仅 65.09%）；
- **转 CI-003**：`ci/tests/test_ci001_failclosed.py:137/:152` 的反向断言必须反转（与 `docs/owner/RELEASE_STATUS.md:108-110` 直接矛盾）；CI 侧新增 `RESOURCE-GATE-REAL` 步骤（现 11 step 全是静态面）。

## 硬纪律

1. 零 git 写；不得改 `ci/**`、`.github/**`、`docs/science/**`、`docs/algorithms/**`；
2. 每条给「改前 → 改后 → 依据」；实现纠错必须给红→绿两次真实输出（复跑 E7a/E7b：改前 rc=10 → 改后 rc=0）；
3. `ninja -C build -k 0` 0 FAILED；相关 ctest 全绿；改被锚文档后复跑 `check_doc_line_anchors.py --root .` rc=0；
4. 在 `SCIENCE_CORRECTNESS.md` 追加 claim SC-003（改了什么/证据/影响面）；
5. 命令带 timeout，日志落 `run/PROJECT-GOVERNANCE-01/GATE-FIX-RES/logs/`。

## 交付（中文，直白：先结论后证据）

1. 逐条执行表；2. 新增契约与文档片段；3. 实现纠错 rc 对照；4. 未做项与原因；5. 自证摘要。