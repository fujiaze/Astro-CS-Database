# 00_OVERVIEW — 任务二 历代控制包取证总览

> 任务二只读取证产物（设计大纲/ 工作域，未改仓库既有文件、未 commit）。归纳层由前台在 16 份组报告（P-G01..G16）+ pack_events.md + digest_verify.md + coverage.md 之上聚合；每条断言的细则与指针以各组报告为准，本文件不重复粘贴。状态字面量与台账行数以设计大纲/_evidence/packs/pack_inventory.csv 及前台复测为源；与组报告冲突处已在 coverage.md 与 STAGE 文件注明。全程不评价优劣、不提改进；无据处写"未证实"。

## 0. 口径与域界定
- 分析对象 = 49 个包身份（pack_inventory.csv）+ 3 个非包形控制件（工程控制/包规范.md、工程控制/ACTIVITY_STATE.md、问题扫描/=RQS-2026-01）；G99 单身份（AstroCS_AUDIT_REVIEWPACK_20260909T133841Z）按前台指令并入 G13 处理（理由见 coverage.md B 节）。
- 存形态口径：worktree（解包件）/ zip / archive（归档容器内）/ recovered_from_history（仅存 git 历史的整树复原，含注入 _PROVENANCE.json）/ zip_recovered_from_history（从 git blob 复原的历史 zip）/ run/** 影子树（只登记不分析，digest_verify 与 P-G08/P-G10 实测 72 份量级）。
- 复原口径注（digest_verify O-1）：recovered 实例的文件数按"含 _PROVENANCE.json"计，跨形态对比时必须还原。

## 1. 完整谱系表（身份 × 存形态 × 提交锚 × 台账 × 自带 SHA256SUMS）
列序：身份 | 形态(去重) | 实例数 | 最大文件数 | 首次提交+日期 | 最后提交 | 删除提交 | 台账行 | 状态分布(一行内) | zip sha256前12 / 包内SHA256SUMS核对
（"—"=该列在 pack_inventory.csv 中为空。多身份同一行的方向异常——如 recovered 形态"首次提交"实为删除提交——是采集脚本已知缺陷，P-G01/G02/G03 缺口节均有登记，引用时以 _PROVENANCE.json 与 git 实测为准。）

| 身份 | 形态 | 实例 | 最大文件 | 首次提交 日期 | 最后提交 | 删除提交 | 台账行 | 状态分布 | zipsha / SHA256SUMS |
|---|---|---|---|---|---|---|---|---|---|
| 2026-09-09_superseded_V8.1_CI_CONTROL_20260905 | work | 1 | 57 | 8e03d7da 2026-09-10 | 8e03d7da | — | 0 | — | — / file: SHA256SUMS, ok: 54, mismatch: 0, missing: 0, mismatch_list |
| ACR_FOCUSED_CONTROL_PACKAGE | reco,work | 3 | 21 | 0030c3a5 2026-08-05 | e1604f14 | — | 0 | — | — / file: SHA256SUMS.txt, ok: 19, mismatch: 0, missing: 0, mismatch_ |
| ACR_FOCUSED_CONTROL_PACKAGE_V2 | reco,work | 3 | 22 | 40c9d216 2026-08-06 | e1604f14 | — | 0 | — | — / file: SHA256SUMS.txt, ok: 0, mismatch: 0, missing: 0, mismatch_l |
| ACR_FOCUSED_CONTROL_PACKAGE_V3 | reco,work | 3 | 22 | a36c4825 2026-08-06 | e1604f14 | — | 0 | — | — / file: SHA256SUMS.txt, ok: 0, mismatch: 0, missing: 0, mismatch_l |
| ACR_FOCUSED_CONTROL_PACKAGE_V4 | reco,work | 3 | 23 | 38b85702 2026-08-06 | e1604f14 | — | 0 | — | — / file: SHA256SUMS.txt, ok: 21, mismatch: 0, missing: 0, mismatch_ |
| AUDIT_PACKAGE_587fe0e341a7 | zip | 1 | 1699 | —  | — | — | 98 | — | 81b7e39ed80c / — |
| AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905 | reco,work,zip | 3 | 57 | a4fdee3f 2026-09-05 | a4fdee3f | — | 0 | — | 77a4b03a622c / file: SHA256SUMS, ok: 51, mismatch: 3, missing: 0, mismatch_list |
| AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3 | zip,zip_ | 4 | 99 | —  | — | — | 191 | — | 005448fb3b68 / — |
| AstroCS_AUDIT_REVIEWPACK_20260909T133841Z | zip | 1 | 14556 | —  | — | — | 98 | — | 3b5d9c230473 / —（98 行经 P-G13 核为 V5 系件聚合，非本包账） |
| AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0 | zip_ | 1 | 74 | —  | — | — | 0 | — | 3e10847e11de / — |
| AstroCS_Authoritative_Development_Pack_v2.0 | reco | 1 | 75 | a78f5430 2026-07-31 | 75b05f72 | a78f5430;a78f5430;a78f5430;a78f5430;a78f5430;a78f5430 | 0 | — | — / file: SHA256SUMS.txt, ok: 71, mismatch: 1, missing: 0, mismatch_ |
| AstroCS_CLI_Core_Development_Pack | reco | 1 | 93 | 036a3bb5 2026-07-29 | ba4f0d34 | 036a3bb5;036a3bb5 | 0 | — | — / file: SHA256SUMS.txt, ok: 91, mismatch: 0, missing: 0, mismatch_ |
| AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1 | zip_ | 1 | 92 | —  | — | — | 0 | — | a87648ec9bc7 / — |
| AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 | work | 1 | 74 | e9547c02 2026-09-10（P-G13 订正方向，原表倒置） | f5f944a5 | — | 57 | PASSED: 29, IN_PROGRESS: 5, NOT_STARTED: 23 | — / — |
| AstroCS_CP0 | zip,zip_ | 2 | 18 | —  | — | — | 62 | — | c79f50d7cdf8 / — |
| AstroCS_Delivery_20260729 | zip_ | 1 | 10 | —  | — | — | 0 | — | 5fd7add31d82 / — |
| AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826 | zip | 1 | 2 | —  | — | — | 0 | — | 493870e7b7cd / — |
| AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 | reco,work,zip | 3 | 27 | b12305ed 2026-08-28 | b7b2dea7 | — | 54 | NOT_STARTED: 54 | f41aacec3d85 / — |
| AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 | zip | 1 | 21 | —  | — | — | 62 | — | fcd96d697a58 / — |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 | reco,work | 2 | 38 | 3ca7f974 2026-08-30 | f99e80d8 | — | 98 | PASS: 1, NOT_STARTED: 97 | — / file: SHA256SUMS, ok: 35, mismatch: 1, missing: 0, mismatch_list |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828 | zip | 1 | 37 | —  | — | — | 98 | — | 42a03c0b0916 / — |
| AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z | zip | 1 | 46 | —  | — | — | 62 | — | 76a43bddb9e7 / — |
| AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | work,zip | 2 | 24 | —  | — | — | 0 | — | 14ee6592f0e1 / — |
| AstroCS_Stage1_HISS_Agent_Package_2026-07-31 | reco | 1 | 13 | a78f5430 2026-07-31 | 78a9121d | a78f5430 | 0 | — | — / — |
| AstroCS_Stage1_HISS_Delivery | reco | 1 | 51 | a78f5430 2026-07-31 | 78a9121d | a78f5430;a78f5430;a78f5430;a78f5430;a78f5430;a78f5430 | 0 | — | — / — |
| AstroCS_Stage1_HISS_Delivery_2026-07-31 | zip_ | 1 | 50 | —  | — | — | 0 | — | 2eceeff476b5 / — |
| AstroCS_Stage1_Wiki_Freeze_2026-07-30 | reco | 1 | 24 | a78f5430 2026-07-31 | 78a9121d | a78f5430;a78f5430;a78f5430;a78f5430;a78f5430;a78f5430 | 0 | — | — / — |
| AstroCS_V6_1_REWORK_CONTROL_20260831 | work,zip | 2 | 47 | 8c71f7ab 2026-09-02 | 8c71f7ab | — | 0 | — | 903018212bfb / file: SHA256SUMS, ok: 43, mismatch: 1, missing: 1, mismatch_list |
| AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 | reco,work,zip | 3 | 47 | 4b1b948e 2026-08-30 | b7b2dea7 | — | 88 | NOT_STARTED: 88 | fdad8e40123a / file: SHA256SUMS, ok: 44, mismatch: 0, missing: 0, mismatch_list |
| AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 | work | 1 | 99 | —  | — | — | 191 | — | — / file: SHA256SUMS, ok: 97, mismatch: 1, missing: 0, mismatch_list |
| CONTROL_V4 | reco | 1 | 27 | b12305ed 2026-08-28 | b12305ed | — | 54 | NOT_STARTED: 54 | — / — |
| CONTROL_V6 | reco | 1 | 47 | 4b1b948e 2026-08-30 | 4b1b948e | — | 88 | NOT_STARTED: 88 | — / — |
| REAUDIT_V3/v3_audit | reco,work | 2 | 19 | 3703650d 2026-08-27 | ac2ced53 | — | 62 | PASS: 10, IN_PROGRESS: 1, NOT_STARTED: 51 | — / file: SHA256SUMS, ok: 1, mismatch: 16, missing: 0, mismatch_list |
| REAUDIT_V3/v3_cp0 | reco,work | 2 | 19 | 3703650d 2026-08-27 | ac2ced53 | — | 62 | PASS: 3, NOT_STARTED: 59 | — / file: SHA256SUMS, ok: 1, mismatch: 16, missing: 0, mismatch_list |
| REAUDIT_V3/v3_reaudit | reco,work | 2 | 23 | 6a947f51 2026-08-28 | b7b2dea7 | — | 62 | PASS: 10, IN_PROGRESS: 1, NOT_STARTED: 51 | — / — |
| REAUDIT_V4/v4_reaudit | reco | 1 | 24 | b12305ed 2026-08-28 | 020cdc99 | b12305ed;b12305ed | 98 | PASS: 5, NOT_STARTED: 93 | — / — |
| _agent_package | reco | 1 | 13 | a78f5430 2026-07-31 | 78a9121d | a78f5430;a78f5430;a78f5430;a78f5430;a78f5430;a78f5430 | 0 | — | — / — |
| _new_pack_v1.1 | reco | 1 | 93 | 036a3bb5 2026-07-29 | ba4f0d34 | 036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5 | 0 | — | — / — |
| acr | reco,work | 2 | 50 | 12fb99f3 2026-08-04 | b7b2dea7 | — | 0 | — | — / file: SHA256SUMS.txt, ok: 48, mismatch: 0, missing: 1, mismatch_ |
| archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | work | 1 | 855 | b7b2dea7 2026-09-02 | b7b2dea7 | — | 54 | NOT_STARTED: 54 | — / file: SHA256SUMS.txt, ok: 122, mismatch: 9, missing: 1, mismatch |
| cprun-v4-pack-template | work | 1 | 10 | 4d3feaf4 2026-09-09 | 4d3feaf4 | — | 0 | — | — / — |
| engineering_archive_v1.0 | reco | 1 | 154 | 036a3bb5 2026-07-29 | ba4f0d34 | 036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5 | 0 | — | — / — |
| engineering_authoritative | reco | 1 | 177 | a78f5430 2026-07-31 | 75b05f72 | a78f5430;a78f5430;a78f5430;a78f5430;a78f5430;a78f5430 | 0 | — | — / — |
| engineering_v1.2 | reco | 1 | 343 | 036a3bb5 2026-07-29 | 0d1857de | 036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5 | 0 | — | — / file: SHA256SUMS.txt, ok: 115, mismatch: 5, missing: 0, mismatch |
| engineering_v1.3 | reco | 1 | 401 | 036a3bb5 2026-07-29 | f4ec8b24 | 036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5;036a3bb5 | 0 | — | — / file: SHA256SUMS.txt, ok: 113, mismatch: 6, missing: 13, mismatc |
| prerelease_v5/AUDIT_REVIEW | work | 1 | 34 | fd4e11ed 2026-08-30 | ef0858c5 | — | 98 | PASS: 88, BLOCKED: 1, REVIEW_PENDING: 1, IN_PROGRESS: 1, NOT_STARTED:  | — / file: SHA256SUMS, ok: 33, mismatch: 0, missing: 0, mismatch_list |
| prerelease_v5/audit_src | work | 1 | 33 | —  | — | — | 98 | PASS: 88, BLOCKED: 1, REVIEW_PENDING: 1, IN_PROGRESS: 1, NOT_STARTED:  | — / — |
| 工程控制/REAUDIT_V3 | reco | 1 | 356 | 6a947f51 2026-08-28 | 3703650d | — | 62 | PASS: 10, IN_PROGRESS: 1, NOT_STARTED: 51 | — / — |
| 工程控制/REAUDIT_V4 | reco | 1 | 24 | b12305ed 2026-08-28 | 020cdc99 | b12305ed;b12305ed;b12305ed;b12305ed;b12305ed;b12305ed | 98 | PASS: 5, NOT_STARTED: 93 | — / — |

## 2. 控制包口径演进序列（七条主线，逐项标出处）

### 2.1 用户入口（产品面）与 Agent 入口（包面）
- 产品入口：无固定入口（前史）→ astrocs-stage2（history seq 563）→ 单一 astrocs target（V5 期 1088 cli/main.cpp）→ 三 Phase 隔离（1519）→ 宪章 §3.1/§8.1 唯一入口（history/00_OVERVIEW §2 CLI 线）。
- 包入口件演进：v1.0 00_START_HERE.md + agent/AUTONOMOUS_MASTER_AGENT_PROMPT.md → v1.1–v1.3 AUTONOMOUS_ENTRY.md + START_PROMPT.txt（61→66 字符）→ v2.0 同式 + README_INSTALLATION.md（安装器）→ Stage1 Agent 包 00_AGENT_START.txt + START_HERE.md → ACR 五包 00_AGENT_START_PROMPT.txt → V3 起 00_READ_FIRST.md + START_PROMPT.txt 双件套（V4 zip 反而无 START_PROMPT，V5 恢复，P-G07 §六）→ V6/V6.1 START_PROMPT 连续执行指令（R0-001→REL-003）→ V7 START_PROMPT + CONTROL_MANIFEST → V8.1 START_PROMPT + baseline/ → 宪章对齐/救援回归 00_READ_FIRST + START_PROMPT/TASK_LIST（救援包无台账）。

### 2.2 架构分层
- v1.x：前史模块路径坐标系（P-G01）；v2.0 九门（Gate A–I）；V3/V4/V5 任务号族即分层（SCI/ALG/ARCH/API/CLI/ABI…）；V5 (ARCH-002)"单一 CLI 架构冻结·Phase1/2/3 in-process"（structural_events seq 1070 消息）；V6 三件套 03_TARGET_ARCHITECTURE/08_CPU_PARALLEL_BACKEND；V7 模块化重筑基（21 组×4 模块号、DLL/ABI/LOADER 标准 12 号）；V8.1 03_VM_AND_CI_ARCHITECTURE（CI 化、runner、token 经济 14 号）；宪章期起架构权威移出包体入宪章（§1.1 权威分层第 5 层 ARCH-*，STAGE-07 §3）。

### 2.3 资源门禁
- V2（无 CSV 台账，§1.3 十字段证据）→ V3 CPU≥150%、1T/2T≥1.50、串行<1%（05_NUMERICAL_AND_PERFORMANCE_GATES.md:19-24）→ V4/V5 相对带宽式 0.80×min(selected_workers, available_cpus)（V4 05_RESOURCE_MONITOR_SPEC §4；V5 07 §3 + 前 10 秒快速失败）→ V6 08_CPU_PARALLEL_BACKEND + task heavy_compute 列 → V6.1 resource_summary.schema.json + 08_CPU_RESOURCE_ACCEPTANCE → V7 resource_class 三值 + 15_CPU 标准 → V8.1 包内回归定性条款（85%/60% 数值 0 命中，数值在宪章 §10.5/§18.2 与 run_monitored.py:453/455、resource_gate.h:108/111，P-G12 事实 6）。方向：包内数值 → 仓库代码/宪章数值 + 包只引用（AGENTS.md 机器门映射节）。

### 2.4 版本命名
- v1.0 无版本串 → v1.1 日期+特性长串 → v1.2/1.3 语义化（1.2.0/1.3.0）→ v2.0 起以产品目标版本入 MANIFEST（V5 期 VER-001 VERSION=0.9.0-alpha.1 单源+alpha 正则；V6 0.10.0-alpha.1 → V6.1 0.10.0-alpha.2 → V7 CONTROL_MANIFEST.product_version 0.11.0-alpha.1 → V8.1 包名嵌 ALPHA0.11.0）→ 宪章 §16.1 MAJOR.MINOR.PATCH-alpha.N 冻结（AGENTS.md alpha/发布行）。审核包命名同步演进：V3 每 Gate 包 → V5 capsules/<task_id>_<commit12>.zip → V6 AstroCS_V6_AUDIT_<UTC>_<12char>.zip → V6.1 AstroCS_V6_1_AUDIT_20260902T042239Z_faad602da555.zip（P-G09 事实 3）。

### 2.5 状态机字面量（全序列，含前台实测锁定值）
TODO/DONE/BLOCKED（v1.1）→ +DEFERRED（v1.2）→ +IN_PROGRESS（v1.3，迁移白名单 5 含 FAILED）→ READY/CONDITIONAL/PENDING_USER_DECISION（v2.0）→ NOT_STARTED/IN_PROGRESS/PASS/FAIL/BLOCKED（V3 五值）→ +DEFERRED/REVIEW_PENDING（V4 七值）→ DEFERRED 禁用、REVIEW_PENDING 存（V5 六值）→ 去 REVIEW_PENDING、+WAITING_WINDOWS（V6 validate_task_graph.py:19——但其包外执行台账仍用 REVIEW_PENDING，校验器判非法，P-G08 事实 6）→ V6.1 status 列回归 WAITING_WINDOWS → V7 删 status 列、状态外置 TASK_STATE.json 11 值 + WAITING_RESOURCE + 禁 REWORK_* → V8.1 status 列回归（全 NOT_STARTED；留痕出现枚举外 FATDUCK_PENDING，P-G10 事实 4）→ 宪章对齐期 PASSED（新字面量，前台实测：57 行=PASSED 29/NOT_STARTED 23/IN_PROGRESS 5，同包归档旧账 TASK_LEDGER_V1_ARCHIVE.csv）→ 救援期无台账无状态。现行（宪章 §14.5 / tools/quality/validate_task_ledger.py）：REVIEW_PENDING 判非法，仅映射历史（AGENTS.md 状态机行）。

### 2.6 审核包与提交协议
V3 逐 Gate 停工+外审（CP0–CP8 复选框）→ V4 连续执行+异步胶囊（01 "取消"节）→ V5 胶囊索引 REVIEW_CAPSULE_INDEX.csv（88 PASS 全有 capsule，P-G07 事实 5）→ V6 单一 audit zip + 四类 commit 相等 + --baseline-sha256（12 §5）+ 执行态外置 evidence/（00 §3）→ V6.1 validate_return_audit 白名单 + READY_FOR_OWNER_REVIEW/α批准权分离（START_PROMPT、00 §1）→ V7 PACKAGE_POLICY ≤50MiB vs 18 §6 ≤20MiB 自相矛盾（P-G10 缺口）+ 独立评审 17 号 → V8.1 STATE_RECONCILIATION/COMMIT_LEDGER.jsonl 对账机制 → 宪章 §14.5 冻结完成顺序（任务→CI→真实数据终验→图像初审→Fatduck→汇总打包）→ 救援期"FINAL-AUDIT 自产物 + ci/known_failures 只减不增"（P-G14 事实 5）→ RQS 把审核变成常驻账本（FIX_LEDGER 682 行与 findings 全等集，P-G16 事实 3）。提交协议常量：main-only 自 V3 06_GIT_MAIN_ONLY 起历代延续（V4 并入 09、V6 11、V7 08、V8.1 MAIN_ONLY_GIT_PROTOCOL）。

### 2.7 派发协议
单代理提示（v1.0 master prompt）→ START_PROMPT.txt 一句话连续执行（v1.1 起）→ 安装器复制（v2.0 install_and_migrate.py 硬编码 engineering_authoritative）→ 三件套/复审包（V3：解包→validate_control→fetch→建 evidence 目录，00 §3 启动动作定型）→ cp/cprun 队列（V7 期 140 任务、/workspace/.dsh/control-runs/ 状态文件，P-G10 事实 6）→ cprun-v4-pack-template（09-09，4d3feaf4 入库）——宪章对齐包本体即 cprun/v4 schema 实例（control-pack.json：max_parallel=12/lanes=11/gates=5/tasks=57，前台实测）+ 包规范.md（09-04 建、从未入库）→ 救援包 FRONT_DESK_RUNBOOK 前台交接式（09-12）→ RQS 三级代理流水线（20_AGENT_PLAN 归属表，09-13）。控制包完成顺序与派发权的宪章化：§14.5（前台派发/SubAgent 不 commit/原子 push）。

## 3. 包 ↔ 历史提交对接结论（pack_commit_link.csv + 各组抽查）
命中率原值：V3 系 88.7%（55/62，窗外 7 号系后世代同号；窗内 48/62）｜V4/CONTROL_V4 96.3%（全部同号歧义，见下）｜V5 系与 prerelease_v5 100%（98/98、379 提交）｜V6/CONTROL_V6 100%（88/88、337）｜REAUDIT_V4 两身份 0%｜V7 87.4%｜宪章对齐 57.9%（P-G13 复现同值，整号口径 87.7%、限包窗 86.0%）｜归档容器 95.5%（并集）。
方法局限（必须与任何引用同段出现）：
1) 跨代同号——V4 命中 52 号中 44 号与 V5 同号、24 号与 V6 同号，抽查 11 号零可归因（P-G06）；V6 的 29 号与 V5 重、45 号与 V6.1 重且 WIN-002/003/005 命中全部早于 V6 引入（P-G08）；V7 51 号与前代同号（P-G10）；容器命中集=成员并集（P-G11 事实 4）。
2) 抽取器缺陷——index.jsonl ids 拆解多段号（CAT-GAIA-DOC→AST-001/DESIGN-001/GAIA-001，V7 实测复现 62.3% vs 表值 87.4%，P-G10 事实 5）；C<d>-<ddd> 族 0 抽取（REAUDIT_V4 0% 的构成因素，P-G07 事实 3）；子串误命中（ID-001←DATA-FRAME-ID-001、C3-002←F-C3-002，P-G11/P-G07）。
3) 采集正则盲区——_tools/02b LEDGER_RE 只认 NN_TASK_LEDGER*.csv：V6.1（03_REWORK…）与 V8.1（CONTROL_TASK_LEDGER.csv）无行系登记口径缺口（P-G09 事实 4、P-G12 事实 4）；救援包无行系"真无台账"（00 L37 明文，P-G14 事实 2）。
4) zip 不入库——13 个 zip-only 身份无提交锚（pack_events.md 事实 4；_control_packs 实测未跟踪，P-G05 事实 1）。
可作强对接的结论：V5（100%+88 capsule 实锚）与 V6（100% 但需剔同号）的号族与提交流真实咬合；REAUDIT_V4 0% 与 V4 全 NOT_STARTED 共同支持"上传件≠执行件"（b12305ed 消息明文，P-G06 事实 3）；V7 的 191 号在 V8.1 留痕域收口（跨代收口，191/191，P-G10 事实 4）。

## 4. 与任务一 8 阶段对照表（reports/history/00_OVERVIEW.md §1 为 seq 权威）
| 任务一段 | seq | 本任务代 | 控制包身份数 | 关键对齐锚 |
|---|---|---|---|---|
| STAGE-01 上游起源与初代包 | 1–271 | STAGE-01 | 15 | v1.0 PACKNEW seq 118；权威包 204 75b05f72；清理 a78f5430 |
| STAGE-02 ACR 底座 | 272–550 | STAGE-02 | 5 | ACR PACKNEW 406/429/438/460；重复注册 550 |
| STAGE-03 Phase2 v6→v19 | 551–715 | （无包代） | 0 | 无 marker 包件；QA 活动非包形（见 §0 域界定） |
| STAGE-04 V19R4-R8 与 REAUDIT_V3 | 716–990 | STAGE-03 主体 | 9 | REAUDIT_V3 PACKNEW seq 978；LEDGER 起点 978 |
| STAGE-05 V4 更替与 V5 | 991–1260 | STAGE-04 | 7（+G06 重叠 3） | 1037/1038/1039（REAUDIT_V4 18 分钟）、V5 1042 |
| STAGE-06 V6/V6.1/V7 | 1261–1520 | STAGE-05 | 6+容器 | CONTROL_V6 1261；V6→V6.1 空窗 575 分；归档 1480 b7b2dea7；V7 1474/1475 |
| STAGE-07 V8.1 与 W1 冻结波 | 1521–1785 | STAGE-06 | 2 | V8.1 1521 a4fdee3f；记账线终结 1780 |
| STAGE-08 宪章·真实数据·CI 转绿·RQS | 1786–1988 | STAGE-07 | 3+G13 3+非包 3 | 宪章 1786 d8c821db；RESCUE 1850；RQS 1859；FIX_LEDGER 1945 |
注：任务一 STAGE-03/04 之间（08-10→08-26）在本包谱系中无新包引入（pack_events.md §三窗口性缺口）——该段由 v19 系列版本前缀与 QA-* 治理号驱动（history/00_OVERVIEW §3 表行 3–4），两任务互不矛盾。

## 5. 证据缺口汇总（分类；明细见各报告 §八 与 coverage.md D 节）
- 入库性：13 zip-only 无提交锚 + 救援包两形态未跟踪 + 包规范.md 从未入库（P-G15 事实 1）。
- 采集脚本层：LEDGER_RE 盲区（2 身份）、recovered 首末提交方向倒置（G01/G02/G03 多身份）、acr 日期倒序（P-G04 事实 4）、v1.3 截断 152 文件（124 个经 G11 容器找回）、dispatch_groups G07 digests 数组重复/漏列（P-G07 缺口）。
- 包内自洽层：CP0/v3_audit 双形态校验相反（行尾）；V6.1 密封脚本 vs 改版脚本互斥判据；V7 ≤20MiB vs 50MiB；V3 09 号引不存在规则 #32；v3_audit SUMMARY 越 schema；REVIEWPACK 自述 PASS 47 vs 包内 45。
- 不可复核层：v1.2 基线锁定哈希全史 0 命中；ACR 三个 Review zip、AstroCS_Stage1_Fix_Review zip、V6 期审核包、V6.1 进度包 zip 均不在库；AUDIT_PACKAGE 内 test_bench_cli.py 113 行版无 git 对象；V8.0 包体不存在。
- 归因层：命中率四项局限（§3）；"有据无账"多为回填/跨代/包外（V6.1 CPU-006/007/008 09-09 回填；RQS c1959436；rescue 49 条目）。
- 已闭合：P-G13 落盘（3 身份、20 缺口），coverage.md D-1 与 STAGE-07 §2/§9 已按其订正（首末提交方向、宪章对齐整号命中率 87.7%、G99 之 98 号台账系 V5 系件聚合、首入库版 CONTROL_FAIL 25/29、三本台账均被现行 validate_task_ledger.py 判 columns mismatch）。组报告缺口合计 225 条（G01..G16 = 17/15/15/8/13/9/20/15/15/16/16/16/20/10/10/10）。

## 6. 产出文件索引（全部为新增，位于 设计大纲/reports/packs/）
组报告 16 份：P-G01-上游前史与初代开发包.md｜P-G02-权威开发包v2.md｜P-G03-Stage1HISS交付与Agent包.md｜P-G04-ACR聚焦四代.md｜P-G05-审计复审V2V3.md｜P-G06-V4预发布控制.md｜P-G07-V5发布控制与审核件.md｜P-G08-V6系统重构.md｜P-G09-V6.1返工.md｜P-G10-V7重筑基.md｜P-G11-legacy归档容器.md｜P-G12-V8与V8.1_CI.md｜P-G13-宪章对齐与包模板.md（已落盘，含 G99 并入）｜P-G14-发布救援V3.md｜P-G15-包规范与活动状态.md｜P-G16-RQS现行程序.md
专项：pack_events.md｜digest_verify.md｜coverage.md
归纳：STAGE-01.md … STAGE-07.md｜00_OVERVIEW.md（本文件）
证据底座（只读引用）：设计大纲/_evidence/packs/（pack_inventory.csv、pack_lineage_table.md、pack_commit_link.csv/md、dispatch_groups.json、digest/、history/、history_zip/）；设计大纲/_evidence/commits/structural_events.csv；设计大纲/reports/history/（00_OVERVIEW.md、structural_events.md、STAGE-01..08.md）

