# P-G11 · legacy 归档容器取证报告

- 组：G11　报告对象：`archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1`（工程控制归档容器）
- 容器绝对路径：`engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/`（下文相对路径均以此为根，除非写明仓库根）
- 本组任务定位：**收纳关系取证**（容器 ↔ 子包 ↔ 各组成员实例 ↔ 移档链），不评优劣、不提建议
- 全部断言只写实测可核对结果；证据不足处标注"未证实"

---

## 一、组内包清单与身份归并

G11 仅 1 个身份（`设计大纲/_evidence/packs/dispatch_groups.json` G11 项 identities=["archive__2026-09-02_legacy_工程控制_v1.3-to-v6.1"]，digests 同名 1 条）。

### 1.1 容器身份本身

| 项 | 实测值 | 证据 |
|---|---|---|
| 身份（归一后） | archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | _evidence/packs/pack_inventory.csv 行"身份"列 |
| 存形态实例数 | 解包件/归档 **1**（worktree）；zip **0**；仅历史复原 **0**；影子树（gitignore 的 run/ 副本）**72** | digest 文首"共 1 个实例"、块头"【worktree】"；影子树：`find run -type d -name 2026-09-02_legacy_工程控制_v1.3-to-v6.1` = 72（`.gitignore:19 run/*`） |
| 文件数 | 855（磁盘 855 = `git ls-files <容器>` 855；`git status --porcelain` 0 条脏项） | digest"文件 855 | 40408.5 KB"，与实测字节 41,378,301 一致 |
| zip sha256 | 无 zip 形态（容器未打包；`工程控制/_control_packs/` 11 个 zip 中无本容器名） | _evidence/packs/pack_inventory.csv"zip或自带sha256"列本行为空 |
| 自带完整性清单 | 包根 `SHA256SUMS.txt` 132 条：ok 122 / mismatch 9 / missing 1 | 本组以 python hashlib 逐条复算，mismatch/missing 清单见第八节第 3 条 |
| 首次出现提交 | b7b2dea7（2026-09-02T22:15:41+08:00） | digest git 事件 first；`git log --format=%H%n%ad --date=iso -- <容器>` 仅 1 条 |
| 末次出现提交 | 同上 b7b2dea7（首=末） | 同上 |
| 是否被删除 | 否（dels=[]；HEAD 仍跟踪全部 855 文件） | digest git 事件 dels 空；实测 ls-files |
| 同名多形态差异 | 无第二形态可对照；容器内容按其前身路径分次入库（见第六节"本体前身"） | pack_inventory 仅 1 行该身份 |

### 1.2 容器收纳清单（nested_packs，9 个子包目录）

`_evidence/packs/pack_instances.json` 本实例 `nested_packs` 字段实测 9 条路径；`parent_pack` 字段在各成员实例行反向登记同一容器。

| # | 容器内子包目录 | 身份 | 文件/字节（本组实测） | 首现提交 | 移入本容器 | 现存形态 |
|---|---|---|---|---|---|---|
| 1 | CONTROL_V4/AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828/ | AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 | 26 / 47,997 | b12305ed（2026-08-28） | b7b2dea7（R100） | digest 共 3 实例（worktree/zip/recovered） |
| 2 | CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/ | AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 | 46 / 200,775 | 4b1b948e（2026-08-30，BAS-001） | b7b2dea7 | 3 实例 |
| 3 | REAUDIT_V3/v3_reaudit/ | REAUDIT_V3/v3_reaudit | 23 / 56,879 | 3703650d（2026-08-27） | b7b2dea7 | 2 实例 |
| 4 | RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/ | AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 | 37 / 99,444 | f99e80d8（2026-08-28，BASE-001） | b7b2dea7 | 2 实例 |
| 5 | archive/ACR_FOCUSED_CONTROL_PACKAGE/ | ACR_FOCUSED_CONTROL_PACKAGE | 20 / 44,827 | 0030c3a5（2026-08-05，原 工程控制/docs/） | e1604f14 内档 → b7b2dea7 | 3 实例 |
| 6 | archive/ACR_FOCUSED_CONTROL_PACKAGE_V2/ | ACR_FOCUSED_CONTROL_PACKAGE_V2 | 21 / 53,653 | 40c9d216（2026-08-06） | 同上 | 3 实例 |
| 7 | archive/ACR_FOCUSED_CONTROL_PACKAGE_V3/ | ACR_FOCUSED_CONTROL_PACKAGE_V3 | 21 / 55,083 | a36c4825（2026-08-06） | 同上 | 3 实例 |
| 8 | archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/ | ACR_FOCUSED_CONTROL_PACKAGE_V4 | 22 / 47,183 | 38b85702（2026-08-06） | 同上 | 3 实例 |
| 9 | tasks/acr/ | acr | 50 / 148,117 | 实测各提交该路径 tracked 文件数：f8d749e4=3（2026-08-02）→ 49ea5c1e=32（2026-08-02）→ 12fb99f3=50（2026-08-04）→ e1604f14 与 b7b2dea7^ 均 50 | b7b2dea7 | 2 实例 |

**第 10 个成员目录存在但未计入 9**：`AstroCS_V6_1_REWORK_CONTROL_20260831/` 只有 1 个文件（`02_FINDINGS.csv`，11,218 B），首现 8c71f7ab（2026-09-02，REL-002），随 b7b2dea7 移入；`nested_packs` 未列它。其所属身份 AstroCS_V6_1_REWORK_CONTROL_20260831（G09）在本组的 1 文件形态未出现在该身份 digest 中（该 digest 只有 2 个实例：46 文件解包件 + 47 文件 zip），详见第八节第 8 条。

容器本体（9 子包 + V6.1 残留之外）= **588 文件**，其中 `evidence/` 占 433 文件 / 40,200,329 字节 = 容器总字节的 97.15%。

### 1.3 移档链（实测 git 层）

1. `engineering_v1.3/` --**036a3bb5**（2026-07-29，"chore: 根目录整理 + 工程控制包重命名"）--`工程控制/`：该提交 455 条改名进 `工程控制/`：实测来源分布 306 条来自 `engineering_v1.2/`、147 条来自 `engineering_v1.3/`、2 条来自仓库根（`流程概述.txt`、`马赛克叠加梯度建模计划.md`，现位于本容器 `docs/`）；同提交另有 405 条 `engineering_v1.3/` 的 D。提交消息自述 "engineering_v1.3/ → 工程控制/ (用户要求中文命名)"、"工程控制从 2.9GB 降至 26MB"。
2. `_v2_pack/` + `engineering_authoritative/` --**a78f5430**（2026-07-31）--`工程控制/migration/`：2 条 R100（实测 `migration/` 现存 2 文件：CURRENT_STATE_AND_SCOPE_MIGRATION.md、INSTALL_REPORT_20260730T024407Z.json）。
3. `工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE{,_V2,_V3,_V4}` --**e1604f14**（2026-08-27，"组织工程控制/报告：历史控制包归档"）--`工程控制/archive/…`：84 条 R100（20+21+21+22），另 1 条 `docs/ACR_25_*.md` → `archive/docs/`。
4. `工程控制/` --**b7b2dea7**（2026-09-02 22:15:41，"docs(governance): GOV-002 归档非当前工程文档"）--`engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/`：**854 条 R100 + 1 条 A（`README_ARCHIVED.md`）= 855**；同提交另有 27 条 R065–R096 → `docs/archive/`、A `docs/DOCUMENT_INDEX.yaml`、A `tools/doccheck/check_doc_index.py`、M `.gitignore`（总计 881 R / 3 A / 1 M）。
5. 提交消息自述归档范围："paths: 881 rename (854 控制文档→engineering/control/archive/2026-09-02_legacy, 27 技术文档→docs/archive)"；"requirements: GOV-002"；"tests: check_doc_index.py DOC_INDEX_PASS 10/10"；"scientific_change: NO"。消息中**不含 "V7" 字样**（`git log -1 --format=%B b7b2dea7 | grep -ic v7` = 0）。
6. 关于任务书所称"134 条 R100"：实测 b7b2dea7 的 R100 总数为 854；134 恰为 ACR 子集条数（ACR 四包 84 + tasks/acr 50 = 134），该口径见 `设计大纲/reports/packs/P-G04-ACR聚焦四代.md:13`。全容器 `设计大纲/_evidence/commits/structural_events.csv` seq1480 记 RENAME50 count=881（854+27）。
7. 后续治理修正：`docs/DOCUMENT_INDEX.yaml:598-601` 以目录级聚合登记本容器（status: ARCHIVED_NON_NORMATIVE + replacement + notes），notes 原文（单行）："GOV-002（2026-09-09）修正 replacement 中已断链的 工作根 control/active/…V7… 指向"；8e03d7da（2026-09-10）另立 `2026-09-09_superseded_V8.1_CI_CONTROL_20260905` 归档条目（DOCUMENT_INDEX.yaml:602-606）。
8. 容器引入后无任何后续提交改动（`git log -- <容器>` 仅 1 条）；`设计大纲/reports/packs/pack_events.md:65,107` 对该容器的引入记为"CSV 未捕获（归档容器：由 seq1480 b7b2dea7 2026-09-02 GOV-002 批量 mv 成组，非标记引入）"。

---

## 二、包内文件地图

### 2.1 容器根（8 个文件）

README.md（v1.3 修复与续开发指导包，含 P11-004 错误 Gate 结论）；MANIFEST.md（Package AstroCS_Recovery_Development_Pack_v1.3 / Version 1.3.0 / Files 133 / Tasks 50 / Start prompt chars 66）；AUDIT_PACK.md（生成 2026-07-29；当前 Gate G12；阶段任务合计 50 / DONE 22 / IN_PROGRESS 1 / TODO 27）；AUTONOMOUS_ENTRY.md（§1 安装迁移、§2 P11-004 强制裁决、§3 续开发范围、§4 执行与停止）；START_PROMPT.txt（139 字节，66 字符）；PACKAGE_VERSION.txt（1.3.0）；SHA256SUMS.txt（132 条）；README_ARCHIVED.md（归档说明，2,034 字节，b7b2dea7 新增）。

### 2.2 顶层目录分布（实测文件数 / 字节）

| 目录 | 文件 | 字节 | 说明 |
|---|---|---|---|
| agent/ | 7 | 5,838 | MASTER_AGENT_INSTRUCTIONS、TASK_EXECUTION_PROTOCOL、EVIDENCE_AND_REPORTING_RULES、REVIEW_AGENT_INSTRUCTIONS、PARALLELIZATION_AND_DEPENDENCY_RULES、BLOCKED_PROTOCOL、SESSION_RESUME |
| archive/ | 85 | 211,767 | ACR 四包 84 + docs/ACR_25_*.md 1（e1604f14 内档产物） |
| checklists/ | 14 | 32,670 | G9_BASELINE…G16_RELEASE_AND_HANDOVER（8）+ P11_WCS_GATE_V2、SCIENTIFIC_EVIDENCE、TASK_ENTRY、BROWSER_PERFORMANCE、QA_V19R7_QUALITY、QA_V19R8_QUALITY |
| configs/ | 1 | 1,722 | stage2.template.json |
| contracts/ | 8 | 9,715 | 7 个 `.schema.json` + FILTER_ALIAS_MAP.template.json |
| control/ | 13 | 73,810 | MASTER_TASK_REGISTER.csv、DECISION_REGISTER.md、CURRENT_TASK.md、PROJECT_STATE.yaml、BASELINE_FACTS.md、REQUIREMENTS_TRACEABILITY.csv、RISK_REGISTER.csv、P11_004_REVIEW_DECISION.json、migration_snapshots(5) |
| docs/ | 37 | 113,339 | 31 个编号规格 + 流程概述.txt + 马赛克叠加梯度建模计划.md + PHASE2_IMPLEMENTATION(3) + PHASE2_INTERFACE_FREEZE(1) |
| evidence/ | 433 | 40,200,329 | 25 个任务号目录 + README.md（P09-001…P13-003、R10-001） |
| migration/ | 2 | 1,291 | a78f5430 移入 |
| schemas/ | 1 | 13,507 | stage2.schema.json |
| tasks/ | 102 | 244,893 | 根 52 个 .md（50 个 P09–P17 + 2 个 QA-V19R7/R8-QUALITY-OPTIMIZATION.md）+ acr 子包 50 |
| templates/ | 8 | 1,930 | ADR、BLOCKED_REPORT、EVIDENCE_INDEX、P11_004_DECISION、REVIEW_REPORT、SESSION_CHECKPOINT、TASK_REPORT、TEST_REPORT（均 .md） |
| tools/ | 3 | 6,387 | validate_pack.py、phase_task_controller.py、migrate_from_v12.py（容器根无 scripts/ 目录） |
| CONTROL_V4/ | 26 | 47,997 | 子包（= zip 原件逐字节） |
| CONTROL_V6/ | 46 | 200,775 | 子包（= zip 原件逐字节） |
| REAUDIT_V3/ | 23 | 56,879 | 子包 v3_reaudit |
| RELEASE_V5/ | 37 | 99,444 | 子包 |
| AstroCS_V6_1_REWORK_CONTROL_20260831/ | 1 | 11,218 | 未登记进 nested_packs 的成员残留 |
| 根文件 8 个 | 8 | 44,790 | README/MANIFEST/AUDIT_PACK/AUTONOMOUS_ENTRY/START_PROMPT/PACKAGE_VERSION/SHA256SUMS/README_ARCHIVED |

注：上表 archive/ 的 85 文件含 4 个 ACR 子包 84 文件（200,746 B）+ `archive/docs/` 1 文件；tasks/ 的 102 文件含 tasks/acr 子包 50 文件（148,117 B）与本体任务 52 文件（96,776 B）。全表相加 = 855 文件 / 41,378,301 字节（实测）。

### 2.3 docs/ 编号规格（NN_XXX.md）自述标题，31 个（00–30 连续）

00 v1.2 阶段目标与边界 · 01 v1.1 审计结论修正 · 02 PlateSolve 单次检测主线状态 · 03 TestData T1–T4 设备与数据清单规范 · 04 主校准帧解析与匹配规范 · 05 WCS 坐标约定与闭环规范（v2）· 06 测光修正规范 · 07 SNR 与 HISS provenance 规范 · 08 Stage1 真实数据全量验证 · 09 银心三片 Red 马赛克正式数据集 · 10 球面梯度科学验证 · 11 稳健叠加与 HCSD 质量规范 · 12 球面浏览器性能基线 · 13 浏览器异步 I/O 与 Tile Cache 规范 · 14 GPU Tile Renderer 规范 · 15 HCSD LOD 与格式演进（条件任务）· 16 浏览器视觉与交互验收 · 17 测试架构与统一全量入口 · 18 代码修改地图 · 19 路线、Gate 与并行计划 · 20 用户结果交付规范 · 21 测光修正与银心三片马赛克验证方案 v1 · 22 球面坐标浏览器性能优化方案 v1 · 23 P11-004 审核裁决 · 24 WCS 验证架构 v2 · 25 PlateSolve 权威星对导出契约 · 26 P11 恢复执行手册 · 27 v1.2 → v1.3 进度迁移规范 · 28 v1.2 未完成范围继承矩阵 · 29 质量优化 V19R7 Spec — 四层统一与代码质量闭环 · 30 Wiki→代码 质量优化 V19R8 Spec — Wiki 核心约束的六阶段闭环。

首段要点（单行摘录）：docs/00 首段为"在 CLI 模式下，用 TestData 的真实数据稳定完成：校准 → 单次星点检测共享 → PlateSolve → 标准 WCS/SIP → PSF → Gaia 光谱积分 → 测光 → SNR → Drizzle → HISS"（docs/00_PHASE_GOAL_AND_BOUNDARIES.md:5）；docs/23 裁决段"现有证据**不足以支持修改 WCS 生产端**"（docs/23_P11_004_REVIEW_DECISION.md:5）。

### 2.4 各子包命名与结构口径（实测）

| 子包 | 顶层文件 | 编号规格 | 台账文件名 | templates | schemas | scripts | 其他 |
|---|---|---|---|---|---|---|---|
| REAUDIT_V3/v3_reaudit | 11（编号 11） | 00–10 | 02_TASK_LEDGER.csv | 9（全 .csv + SUMMARY.schema.json） | 0 | 3（validate_control / validate_audit_package / package_audit） | — |
| CONTROL_V4/… | 13（编号 13） | 00–12 | 02_TASK_LEDGER.csv | 10（全 .csv） | 0 | 3（validate_control / package_final / validate_final_package） | 无 SHA256SUMS |
| RELEASE_V5/… | 19（编号 16 + PACKAGE_MANIFEST.md / SHA256SUMS / START_PROMPT.txt） | 00–15 | 02_TASK_LEDGER.csv | 13 | 2（cli_event、cpu_profile） | 3（同 V4 三件） | SHA256SUMS 36 条：35 ok / 1 mismatch |
| CONTROL_V6/… | 23（编号 19 + AGENTS_REPLACEMENT.md / MANIFEST.json / SHA256SUMS / START_PROMPT.txt） | 00–18 | **05_TASK_LEDGER.csv** | 9（.md/.csv 混合，含 OWNER_REVIEW.md） | 6 | **5**（validate_control / validate_task_graph / validate_audit / package_audit / selftest） | fixtures/3；SHA256SUMS 44 条全 ok |
| archive/ACR_FOCUSED_CONTROL_PACKAGE | 14（编号 11 + CHECKLIST.md / package_manifest.json / SHA256SUMS.txt / history/） | 00–09 | 无台账 | 1（evidence_checklist.md） | 4 | 0 | SHA256SUMS.txt 19 条全 ok |
| ACR_V2 / ACR_V3 | 14（编号 11 + audits/ + history/） | 00–10 | 无台账 | 1 | 4 | 0 | SHA256SUMS.txt 为 JSON 格式（total_files 20 / total_bytes 50287 与 51714），20 条全 ok |
| ACR_V4 | 13（编号 10 + examples/） | 00–10 | 无台账 | 1 | 2 | 0 | SHA256SUMS.txt 21 条全 ok |
| tasks/acr | 28（编号 24 个 .md + AUDIT_REPORT_2026-08-02.md / CHECKLIST 项 / package_manifest.json / SHA256SUMS.txt） | 00–22 | 无台账 | 7（`.template.*` 与 `.md` 混合） | 8 | 0 | examples/4、audits/2、include/astro/compute/acr.hpp；SHA256SUMS.txt 49 条（48 ok / 1 名称大小写不符，见第八节） |

各子包 `00_READ_FIRST.md` 自述标题（首行）：V3 "# AstroCS MAIN 预发布重审控制 V3"；V4 "# AstroCS V4：CPU 自适应预发布控制"；V5 "# AstroCS V5 预发布控制包：单一 CLI / amd64 CPU 自适应"；V6 "# AstroCS V6 系统重构控制包：执行入口"；ACR 四包同为 "# AstroCS ACR 工程控制包"；tasks/acr "# AstroCS ACR 底层支线开发控制包"。

---

## 三、任务结构

### 3.1 台账数量与行数

| 台账 | 数据行 | 表头字段 |
|---|---|---|
| control/MASTER_TASK_REGISTER.csv（本体） | 146 | task_id, phase, title, dependencies, gate, status, evidence_dir |
| REAUDIT_V3/v3_reaudit/02_TASK_LEDGER.csv | 62 | task_id, gate, depends_on, scope, required_commit, required_push, checkpoint, status |
| CONTROL_V4/…/02_TASK_LEDGER.csv | 54 | task_id, phase, depends_on, scope, required_commit, required_push, preferred_host, status |
| RELEASE_V5/…/02_TASK_LEDGER.csv | 98 | 同 V4 八字段 |
| CONTROL_V6/…/05_TASK_LEDGER.csv | 88 | task_id, gate, title, depends_on, platform, change_class, heavy_compute, commit_required, scope, acceptance, status |

digest 只登记 4 张子包台账（合计 302 行；"台账行 98" 即其中 RELEASE_V5 那张）；本体 MASTER_TASK_REGISTER.csv 未被登记（第八节第 5 条）。

### 3.2 任务号族

- 本体：P09×3、P10×6、P11×6、P12×6、P13×4、P14×8、P15×8、P16×6、P17×3、QA-V19R7-*/QA-V19R8-* ×96；gate 分布 G9 3 / G10 6 / G11 6 / G12 10 / G13 8 / G14 8 / G15 6 / G16 3 / G-QA 96。
- V3（62 行）：ID、CON×10、BLD、TST、CHK、RUN、ACR、PKG、ORA、SEAM、HIPS、REL×2、WIN×2、SCI×5、ALG×4、ARCH×2、API×5、DOC、BUILD 等单行族。
- V4（54 行）：BASE×2、GOV、SCI×6、ALG×5、CPU×4、SYN×6、DOC×2、PAR×7、MON×2、REV×4、LNX×4、WIN×6、QA×3、REL×2、ARCH、API。
- V5（98 行）：新增 ABI×3、BENCH×5、CLI×8、ISA×5、ISO、P3×4、SYN×9、DOCCHK×2、LNX×5、WIN×9、REL×4、REV×3、TRACE、VER、GOV。
- V6（88 行）：改以 BAS×4、CORE×8、CPU×8、IO×4、P1×9、P2×8、P3×6、LEG×4、QA×5、TEST、MOD 族（无 GOV/REV 族）。
- 跨台账共号：199 个唯一号中 128 号只在 1 张台账、47 号在 2 张、16 号在 3 张、8 号在全部 4 张（DOC-001、REL-001、REL-002、SCI-001、SCI-002、SCI-003、WIN-001、WIN-002）；各包独占数 V3 40 / V4 2 / V5 36 / V6 50。

### 3.3 状态字面量与分布（实测）

- 本体主台账：DONE 117 / TODO 28 / IN_PROGRESS 1（IN_PROGRESS 行为 P13-002，见 control/MASTER_TASK_REGISTER.csv）。
- V3：PASS 47 / NOT_STARTED 7 / BLOCKED 4 / IN_PROGRESS 3 / FAIL 1。
- V4：NOT_STARTED 54（全表未推进）。
- V5：PASS 88 / NOT_STARTED 7 / BLOCKED 1 / REVIEW_PENDING 1 / IN_PROGRESS 1。
- V6：NOT_STARTED 88（全表未推进）。
- 状态词表代际：V3 五值（REAUDIT_V3/v3_reaudit/00_READ_FIRST.md:19）→ V4 七值加 DEFERRED/REVIEW_PENDING（CONTROL_V4/…/00_READ_FIRST.md:40-42）→ V5 六值并禁 DEFERRED（RELEASE_V5/…/00_READ_FIRST.md:35-43）→ V6 加 WAITING_WINDOWS（CONTROL_V6/…/00_READ_FIRST.md:49 与 04_MIGRATION_AND_GATES.md:27-41）。

### 3.4 依赖与顺序声明

- 本体 dependencies 为 `;` 分隔 task_id 列表；`tools/validate_pack.py:16-18` 机器校验"每个 task_id 必须有 tasks/<id>.md"与"依赖必须闭合"。实测 146 行中 **96 行（全部 QA-* 行）无同名 tasks/QA-*.md**（tasks/ 根仅 52 个 .md）→ 该校验在包内不可能通过。
- 子包 depends_on：V4/V5/V3 单列依赖号；V6 增 gate/platform/change_class/heavy_compute/commit_required/acceptance 六个执行属性（05_TASK_LEDGER.csv 表头）。
- 顺序与 Gate 计算：CONTROL_V6/…/04_MIGRATION_AND_GATES.md:25 "Gate 状态只能由 `05_TASK_LEDGER.csv` 的任务状态计算。不得手写总数，不得通过删除 dependency 让 Gate 变绿"；同文件 :99 为"任务图变更规则"。V3 串并行统一规则见 01_WORKFLOW_AND_GATES.md:31；V6 并行规则另见本体 agent/PARALLELIZATION_AND_DEPENDENCY_RULES.md；本体顺序见 docs/19_ROADMAP_GATES_AND_PARALLEL_PLAN.md:3-10（G9–G16 序列）与 :12（P15 可在 G10 后与 P11–P13 并行、P16 真实视觉验收依赖 G13）。

---

## 四、门禁与验收要求（逐条出处）

### 4.1 机器门清单

1. 本体包校验器 `tools/validate_pack.py`：必含 11 个文件（含 `review_inputs/P11-004_review_bundle.zip`，:7）、START_PROMPT ≤100 字符（:11）、tasks/<id>.md 与依赖闭包（:16-18）、decision_id 必须等于 `ADR-P11-004-GATE-V2`（:21）。本组实跑（只读）返回 `MISSING: review_inputs/P11-004_review_bundle.zip`，退出码 2。
2. 本体包完整性清单 `SHA256SUMS.txt`（132 条）。
3. 子包脚本门：V3 `scripts/validate_control.py` + `validate_audit_package.py` + `package_audit.py`；V4/V5 `validate_control.py` + `package_final.py` + `validate_final_package.py`；V6 另加 `validate_task_graph.py`、`selftest.py`、`validate_audit.py`（实测各 scripts/ 目录清单）。执行入口条款：CONTROL_V6/…/00_READ_FIRST.md:31 "验证控制包：`python3 scripts/validate_control.py .`"。
4. 归档治理门（容器外、同提交）：`tools/doccheck/check_doc_index.py`（b7b2dea7 新增，提交消息自述 `DOC_INDEX_PASS 10/10`）；`docs/DOCUMENT_INDEX.yaml:598-601` 目录级 ARCHIVED 登记。
5. 各子包自带清单核对结果（本组实测）：CONTROL_V6 SHA256SUMS 44/44 ok；RELEASE_V5 35 ok / 1 mismatch（02_TASK_LEDGER.csv）；ACR_V1 19/19 ok；ACR_V4 21/21 ok；ACR_V2/V3（JSON 清单）20/20 ok；tasks/acr 48 ok / 1 名称不符；CONTROL_V4 与 AstroCS_V6_1_REWORK 残留内无清单文件。

### 4.2 完成顺序

- 本体：docs/19:3-10 定义 G9→G16；AUTONOMOUS_ENTRY.md §4 规定仅"全部完成"或"硬阻塞 + BLOCKED_REPORT.md"两种汇报（另 templates/BLOCKED_REPORT.md、agent/BLOCKED_PROTOCOL.md）；agent/TASK_EXECUTION_PROTOCOL.md:3-10 八步（第 8 步"PASS 后更新控制文件并选择下一任务"）。
- V3：00_READ_FIRST.md:32 规则 5"到达 Checkpoint 时按 `04_CHECKPOINT_CHECKLISTS.md` 打包；等待外部审核，不得自行跨关"；09_CONTINUOUS_EXECUTION_ORDER.md:11 覆盖为"从 **CON-001** 开始连续推进至 **G7**"，:17"**仅在 G8 提交一次预发布审核包**"。
- V4：00_READ_FIRST.md:32 完成最后任务后输出 `AWAITING_EXTERNAL_RELEASE_REVIEW`。
- V5：15_CONTINUOUS_CHECKPOINTS.md:3-13 C0–C9 检查点表（每行给"所需 Task / 逐项机器检查 / 失败动作"），C9 行要求"P0/P1=0；core 100%+other 20%；双平台 hash；最终包 validator"并"输出 `AWAITING_EXTERNAL_RELEASE_REVIEW` 并停止"。
- V6：04_MIGRATION_AND_GATES.md:11-23 G0–G10 Gate 表（含"是否等人"列）；:43-55 每任务 11 步固定模板（第 8 步生成 `evidence/refactor/tasks/<TASK_ID>/TASK_RESULT.json`、第 10 步 commit 前缀 `<TASK_ID>:`）；00_READ_FIRST.md:75 完成定义（13_RELEASE_ACCEPTANCE.md 全门通过 + validate_audit.py 通过 → READY_FOR_OWNER_REVIEW；负责人 HiPS 视觉审核批准 → ALPHA_RELEASE_APPROVED）。

### 4.3 状态机口径（见 3.3 末行四代对照）；补充：CONTROL_V6/…/04_MIGRATION_AND_GATES.md:41 "`BLOCKED` 只用于真实阻塞。每次状态变更记录 UTC、commit、证据路径和原因。禁止从 FAIL 直接改 PASS 而没有新证据"。

### 4.4 waiver / 豁免

容器本体（v1.3 五张表与 docs/）无 waiver 条款；实测容器内 13 处 waiver/豁免命中全部位于子包：
- V3：00_READ_FIRST.md:18 规则 6"Agent 无权改变任务、阈值、状态词、豁免条件和执行顺序"；03_TASK_SPECIFICATIONS.md:195"明确第三方豁免需外部批准"；09_CONTINUOUS_EXECUTION_ORDER.md:20-21（审核人指令属"对执行顺序/豁免的外部授权"，仅放开中间 Checkpoint 的等待）。
- V4：00_READ_FIRST.md:31"不得用豁免把 FAIL 改 PASS"；03_TASK_DETAILS.md:20 同义；11_V3_INHERITANCE_AND_INVALIDATION.md:19"CON-010 豁免：利用率门禁失败不能豁免"。
- V5：03_TASK_DETAILS.md:12"任何 PARTIAL、waiver、忽略错误均不是 PASS"；12_V3_V4_MIGRATION.md:13 把"CON-010 一类失败后 waiver"列入明确失效，:20 记为已知风险"此前性能门禁被豁免"；15_CONTINUOUS_CHECKPOINTS.md:11 C4 行"不得 waiver"。
- V6：01_ASTROCS_ENGINEERING_CONSTRAINTS.md:79"不能由 Agent 自行豁免"；03_TARGET_ARCHITECTURE.md:204"`MEMORY_BOUND_EVIDENCED`…这不是自动豁免"。

### 4.5 审核包与提交要求

- V3 07_AUDIT_PACKAGE_SPEC.md：体积"目标且自动硬门禁：压缩后 `<=25 MiB`"、"单文件 `<=5 MiB`"、"超限即 FAIL，不允许先打大包再人工裁剪"；"必须包含"清单 12 项（00_READ_FIRST、SUMMARY.json 由脚本从 CSV 生成禁手填、TASK_LEDGER.csv、CHECKPOINT_RESULTS.csv、COMMITS.csv、FINDINGS/BUILD/TEST/PERF_RESULTS.csv、TRACEABILITY、LARGE_ARTIFACT_MANIFEST.csv、MANIFEST.json、SHA256SUMS）；"禁止包含"清单（.git、bundle、build、对象库、FITS/XISF/HISS、原始像素 CSV、完整 HiPS、模型缓存）。停止条件同步写在 00_READ_FIRST.md:55"审核包超过 25 MiB 或含禁入文件"。
- V4 00_READ_FIRST.md:24 硬规则 12：25 MiB / 5 MiB + 禁原始数据、完整 HiPS、长日志、像素 CSV；templates/REVIEW_CAPSULE_INDEX.csv 与 12_REVIEW_CAPSULE_AND_EXTERNAL_AUDIT.md 规定胶囊/外审。
- V5 templates 13 件（新增 CHECKPOINTS.csv、RELEASE_ARTIFACTS.csv、SCIENCE_CLAIMS.csv、SUMMARY.json）；00_READ_FIRST.md:56"Agent 无权宣布发布"。
- V6 12_AUDIT_PACKAGE_SPEC.md:7"审核包只能由 `scripts/package_audit.py` 生成。禁止手工把仓库根目录整体压缩"；:44-45 未列入 manifest 的单文件 >5 MiB、总包 >25 MiB（须生成 size report，不由 Agent 自行放宽）；:94 扫描项（敏感内容、禁止扩展名、symlink、绝对路径、文件大小）；15_GATE_CHECKLISTS.md:3 逐 Gate 复制对应段到 `evidence/refactor/gates/Gx/CHECKLIST.md` 并"空框不能通过"；00_READ_FIRST.md:77 汇报必列字段（目标版本、最终 commit、原子提交、科学变更状态、三类门禁结果、双平台结果、未验证项、审核包路径与 SHA-256）；00_READ_FIRST.md:35"将任务台账复制为仓库内 `evidence/refactor/TASK_LEDGER.csv`，状态只能按脚本规则迁移"。
- 本体提交纪律：agent/TASK_EXECUTION_PROTOCOL.md:3-10（含"外部命令必须有超时"）；agent/EVIDENCE_AND_REPORTING_RULES.md 八条；checklists/TASK_ENTRY.md 入场清单；control/CURRENT_TASK.md 记录 ACR 25 号计划 SHA 755278bf… 与聚焦包 SHA 56f74f2e…，并记 feature/astrocompute-runtime 分支不合并 main。
- 提交留痕模板：V3/V4/V5 各有 `templates/COMMITS.csv`（表头 task_id,commit_sha,parent_sha,branch,subject,changed_files,test_ids,push_status）；实测 V3 该文件 8 行（含真实 SHA，例 CON-006 → 6b41c06f…）、V4 与 V5 均只有表头 1 行；V6 无 COMMITS.csv（改用 00_READ_FIRST.md:53 与 04:54 的 commit 前缀规则）。

---

## 五、设计意图（转述，不评价）

- **包自述目标**：README.md 自述为 v1.3"修复与续开发指导包"，并说明 P11-004 的 Gate 结论错误来源；START_PROMPT 全文（66 字符）："解压最新AstroCS修复开发包，读取AUTONOMOUS_ENTRY.md，迁移v1.2进度并持续执行，仅硬阻塞或全部完成时汇报"；AUTONOMOUS_ENTRY.md §2 对 P11-004 采"双层闭环"强制裁决并给出 NO_CODE_CHANGE_REQUIRED 分支；MANIFEST.md "Key recovery additions" 五条（P11-004 裁决与 Gate v2、权威星对契约、v1.2 进度迁移工具、原始审核包内嵌、v1.2 未完成任务全部保留）。
- **禁止项**：docs/00"本阶段不做"5 条（不开发完整业务 GUI、不让 JS 直读 PipelineFrame/DLL、不重写 PlateSolve 核心匹配、不先改 HCSD 格式等）；CONTROL_V6/…/00_READ_FIRST.md:60-71 十条绝对禁止项（生产 workers=1、硬编码线程数与 ISA、把 ACR 链进 Alpha 生产 CLI、共享 fitsfile*、用关键词匹配宣称科学正确、反复跑旧新 32R 全量对比、把大文件塞进审核包、改依赖隐藏 blocker）；V5 00_READ_FIRST §1 12 条；V4 硬规则 12 条；V3 12 条不可变规则；V3 07 审核包禁含清单；tasks/acr/15_DECISIONS.md 6 条冻结决策（ACR 为独立底层运行时、唯一分支 feature/astrocompute-runtime、控制包不用发布版本号、公共 API 与配置禁止 share 参数等）。
- **继承 / 废止声明**：V4 00_READ_FIRST.md:9"本包完全替代 V3"（并逐项取消 V3 的 A/B/C、ACR GPU/Mixed、逐 Checkpoint 外审、历史 32R）；V5 00_READ_FIRST.md:7"本包取代 V3/V4 的未完成门禁"；V4 11_V3_INHERITANCE_AND_INVALIDATION.md（直接失效清单含 RUN-001..006）；V5 12_V3_V4_MIGRATION.md；本体 docs/27（v1.2→v1.3 进度迁移规范）与 docs/28（v1.2 未完成范围继承矩阵）；容器层 README_ARCHIVED.md:3-11 声明本目录整体 ARCHIVED（GOV-002 wave W1）、原路径 `工程控制/`（2026-07-29 由 `engineering_v1.3/` 重命名，commit 036a3bb）、归档动作为一条 git mv，:26-28 给出保留理由（854 个跟踪文件供审计追溯，SHA256SUMS 保持原样以维持包完整性校验）。
- **负责人裁决 / 用户指令痕迹（容器内实际存在者）**：control/DECISION_REGISTER.md 9 个 ADR 小节（ADR-P11-004-GATE-V2、ADR-P11-004-GATE-V2-OUTCOME、ADR-P11-005-OUTCOME、ADR-P11-006 均 2026-07-28；ADR-P12-001…004 均 2026-07-28；末条 ADR-QA-V19R8 2026-08-22；见 control/DECISION_REGISTER.md:8,11,21,29,40,53,66,80,106）；docs/23_P11_004_REVIEW_DECISION.md；control/P11_004_REVIEW_DECISION.json（decision_id 即 validate_pack.py 的机器断言对象）；templates/P11_004_DECISION.md；evidence/P11-004/P11_004_DECISION.md；checklists/P11_WCS_GATE_V2.md 11 个待勾项；control/PROJECT_STATE.yaml:5-6,14-16（current_gate G-QA、current_task QA-V19R7-B5-01、p11_004_gate_version 2、p11_004_outcome WCS_PRODUCTION_FIX_REQUIRED、p11_004_repair_nature HEADER_REGENERATION_NO_CODE_CHANGE）；REAUDIT_V3 09_CONTINUOUS_EXECUTION_ORDER.md:3（"来源: 审核人直接指令 (2026-08-27)"）与 :8（"指令内容 (审核人逐字)"）、10_REVIEWER_DEVIATIONS.md:7,16-17（DEV-ACR-001，引"GPU 不可用记 BLOCKED，不能 PASS"；该文件仅存在于容器，历史复原件与 zip 均无）；CONTROL_V6/templates/OWNER_REVIEW.md 与 00_READ_FIRST.md:75（负责人 HiPS 视觉审核）；V5 00_READ_FIRST.md:56（最终发布权在负责人）。
- **未证实项**：容器内**不存在** OWNER_BINDINGS、04_OWNER_DECISIONS、07_FRONT_DESK_RULINGS 任何同名文件（`find <容器> -iname '*OWNER*'` 只命中 CONTROL_V6/templates/OWNER_REVIEW.md）；这三类文件存在于 `工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/`（属后续代际，非本组）。容器内亦无 control-pack.json（实测 0）。

---

## 六、代际差异表（只写可核对差异）

### 6.1 容器本体与其前身路径的文件集合差异

| 对比 | 共有 | 容器独有 | 对方独有 | 内容不同 |
|---|---|---|---|---|
| 容器本体（去子包，588 文件） vs `_evidence/packs/history/036a3bb5/engineering_v1.3/`（400 文件，复原件另含 _PROVENANCE.json） | 326 | 262 | 74（全部在 `evidence/P11-004/`） | 10 |
| 同上 vs `history/036a3bb5/engineering_v1.2/`（342 文件） | 319 | 269 | 23（`control/_commit_msg_*.txt`、`control/_commit_p10_006.*` 等临时件） | 18 |

- 容器独有 262 个的分布：evidence/ 新增 P12-004 44、P13-003 86、P13-001 14、P12-005/P12-001/P12-006/P11-005 各 11–12、R10-001 12、P11-004 9、P12-003 7 等；templates/ 8；tools/ 3；docs/29,30 + PHASE2 两子目录 4 + 2 个中文名文档；checklists/QA_* 2；migration/ 2；configs/1、schemas/1、contracts/1、README_ARCHIVED.md 1。
- 内容不同的 10 个（容器 vs v1.3 复原件，字节对比）：AUDIT_PACK.md 10282/10398；AUTONOMOUS_ENTRY.md 2939/2959；control/CURRENT_TASK.md 3188/1250；control/DECISION_REGISTER.md 14478/12821；control/MASTER_TASK_REGISTER.csv 12968/4439；control/PROJECT_STATE.yaml 8851/8846；control/RISK_REGISTER.csv 954/827；docs/18_CODE_CHANGE_MAP.md 4926/835；evidence/P11-004/EVIDENCE_INDEX.md 7884/7888；evidence/P11-004/P11_004_DECISION.md 9279/9283。
- 被删侧：036a3bb5 对 `engineering_v1.3/evidence/P11-004` 记 111 条 D（提交消息自述清理"P11-004 备份(192MB)…旧 review bundle(38MB)"）→ 容器内 P11-004 只剩 15 文件，复原件为 80 文件。

### 6.2 收纳子包的代际链（V3→V4→V5→V6→V6.1）

| 项 | V3 | V4 | V5 | V6 | V6.1（容器内） |
|---|---|---|---|---|---|
| 容器内文件/字节 | 23 / 56,879 | 26 / 47,997 | 37 / 99,444 | 46 / 200,775 | 1 / 11,218 |
| 编号规格 | 11 | 13 | 16 | 19 | 0 |
| 台账 | 02_ / 62 行 | 02_ / 54 行 | 02_ / 98 行 | **05_ / 88 行** | 无（容器内） |
| 状态字面量 | 5 | 7（含 DEFERRED） | 6（禁 DEFERRED） | +WAITING_WINDOWS | 包外台账用 PASS/WAITING_WINDOWS/NOT_STARTED |
| 容器内台账是否回写 | 是（终态） | 否（全 NOT_STARTED） | 是（终态） | 否（全 NOT_STARTED） | 无台账 |
| 自带清单 | 无 | 无 | 36 条（1 处不符） | 44 条（全符） | 无 |
| schemas / fixtures | 0 / 0 | 0 / 0 | 2 / 0 | 6 / 3 | — |
| scripts | 3 | 3 | 3 | 5 | — |
| 审核包体积条款 | 07:25 MiB/5 MiB（硬门禁） | 00:24 同 25/5 MiB | 00:24 沿用 | 12:44-45 超限须 size report | — |
| 提交留痕 | COMMITS.csv 8 行 | COMMITS.csv 表头 | COMMITS.csv 表头 | 无 COMMITS.csv（evidence/refactor/） | 包外 evidence/v6_1_rework/ |

ACR 链差异（同容器内）：V1 20 文件 / 无 audits；V2、V3 各 21（新增 audits/1：ACR_FOCUSED_REVIEW.md 与 ACR_FOCUSED_V2_REVIEW.md）；V4 22（去 audits/history，改 examples/weighted_integration + 1 份新 schema，编号规格由 11 减为 10）；tasks/acr 50（编号规格 24、schemas 8、templates 7、examples 4、audits 2、include/ 1）。四包 `00_READ_FIRST.md` 首行标题未随代次改变。

### 6.3 容器 vs 上一代归档容器（G10 末包）

| 项 | 本容器 | G10 末包 AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 |
|---|---|---|
| 文件 / 字节 | 855 / 41,378,301 | 99 / 397,833（解包件）；zip 形态 99 / 398,022 |
| zip sha256 | 无 zip 形态 | 有 zip（工程控制/_control_packs/ 及 history_zip 复原件） |
| 台账 | 4 张（302 行 / 199 号）+ 本体 1 张 146 行 | 1 张（191 行，_evidence/packs/pack_lineage_table.md:41） |
| 子包 | 9（+1 未登记残留） | 0 |
| 跟踪状态 | 855 文件全部 git 跟踪、工作树干净 | 解包件未跟踪（`git ls-files` 命中 0） |
| 控制包元数据文件 | MANIFEST.md + PACKAGE_VERSION.txt + SHA256SUMS.txt | control-pack.json 类（后续代际口径） |

---

## 七、执行留痕三方核对

### 7.1 命中率引用与复算

- 引用 `_evidence/packs/pack_commit_link.csv`：容器行 = 台账任务号 199 / 命中 190 / **95.5%** / 578 提交 / 未命中样例 "ACR-002, ACR-003, BASE-002, CHK-004, ID-002, ID-003, PKG-001, REV-004, RUN-006"；成员行：V5 98/98=100.0（379 提交）、V6 88/88=100.0（337）、V4 54/52=96.3（232）、REAUDIT_V3 62/55=88.7（194）。
- 本组复算（`git log --format=%s %b` 全 2043 条提交，宽松包含匹配）：容器 **190/199 = 95.5%**，四个成员分别 52/54、98/98、88/88、55/62，与 CSV 完全一致。
- 本组另以"带词边界"匹配复算：186/199 = 93.5%，与 CSV 相差 4 号。逐号查上下文（实测）：ID-001 只出现在 `p2_frame_id(DATA-FRAME-ID-001)`（提交 439f9f20，2026-09-10）、HIPS-001 只出现在 `DATA-TILE-001/DATA-HIPS-001 的 VERIFIED 属实`（提交 2fdd6626，2026-09-12）——两者是被更长他代号吞掉的**假命中**；CHK-005 出现在 `G4 ARCH/API/DOC/CHK-005 PASS`、REV-003 出现在 `PAR-002/REV-002/WIN-006/007/008/REV-003/REL-1..4 未达`——两者是斜杠列举中的**真命中**，被边界规则误排除。即两种口径各有偏差，聚合命中率的分子含跨代同号/子串命中。

### 7.2 "聚合"与"成员"的关系（本组核心判定）

- 容器 199 号 = 四张成员台账号的**并集**（302 行去重；71 号跨包共用）；容器命中集实测**恰等于**四成员命中集的并集（成员命中数之和 293 > 容器 190）。
- 因此 95.5% 只是"成员命中并集 ÷ 成员号并集"，与成员各自的 100/100/96.3/88.7 之间是包含关系而非独立佐证；行加权口径为 293/302 = 97.0%。
- 归因限制（实测）：V4 的 2 个独占号（BASE-002、REV-004）**全部未命中**，V4 其余 52 号全与 V5/V6/V3 共用 → V4 的 96.3% 无法独立归因于 V4 自身执行；V3 独占 40 号中 33 命中（85.0%），其 7 个未命中号（ACR-002/003、CHK-004、ID-002/003、PKG-001、RUN-006）全部为 V3 独占号。
- 结论仅就数字：容器命中率不能作为任一子包被执行的独立证据；它同时被跨包共用号与长号子串抬高或拉低。

### 7.3 分类清单（按容器 199 号；留痕口径 = 容器 `evidence/` + `reports/evidence/` + `evidence/refactor/tasks/` + `evidence/v6_1_rework/tasks/` + `artifacts/prerelease_v5/` 中同名目录/同名文件）

| 类别 | 判定 | 数量 | 清单要点 |
|---|---|---|---|
| 一致 | 台账 + 提交消息 + 留痕目录三者齐备 | **138** | 含 V6 全 88（对应 `evidence/refactor/tasks/`）、V5 大部、V3 大部 |
| 有账无据（消息有、留痕目录无） | 台账 + 消息命中，包内包外均无同号留痕目录 | **52** | 例：ACR-001、BLD-002/003/004、CHK-005/006、CON-001…（V3 的 CON/CHK 族留痕以 `reports/REAUDIT_V3/v3_exec` 等文件形式存在，不按号建目录） |
| 两者皆无 | 既无消息命中也无留痕目录 | **9** | ACR-002、ACR-003、BASE-002、CHK-004、ID-002、ID-003、PKG-001、REV-004、RUN-006 |
| 有据无账 | 目录名形似任务号、但不在容器内 5 张台账（本体 146 行 + 四子包 302 行）任何一行 | 16 个号目录 | 15 个位于 `evidence/v6_1_rework/tasks/`（DOC-006、GOV-002、R0-001…004、RT-001…009）——它们在包外 `evidence/v6_1_rework/TASK_LEDGER.csv`（67 行）中各有行，属容器无账/包外有账；另 1 个 `evidence/refactor/tasks/REL-CONSISTENCY` 两侧均无账。容器内 `evidence/R10-001`（12 文件）同样不在任何台账（R10 纠正包本体不在容器）。6 个非任务号留痕目录不计入本类：`artifacts/prerelease_v5/` 的 AUDIT_REVIEW、audit_src、capsules、lnx_pkg、review、tables |

### 7.4 容器本体主台账（146 行）三方

| 类别 | 数量 | 明细 |
|---|---|---|
| 一致（DONE + 包内 evidence/ + 消息） | 22 | P09-001…P13-001 的 DONE 行 |
| IN_PROGRESS + 包内 evidence/ | 1 | P13-002（control/MASTER_TASK_REGISTER.csv；PROJECT_STATE.yaml:6 亦指 QA-V19R7-B5-01） |
| TODO 但已有包内 evidence/ | 1 | P13-003（`evidence/P13-003/` 86 文件，台账仍 TODO） |
| 有账无据（DONE 无包内留痕） | 95 | 全部 QA-* 行；其中 8 号（QA-V19R7-A1-01、QA-V19R7-A2-01…05、A2-06…）在包外 `reports/evidence/QA-V19R7-A2-01..08` 有同名目录（30 条跟踪路径含 QA-V19R7），另 87 号在包内包外均无同名留痕目录 |
| 有账无据（TODO 且无留痕、无消息） | 25 | P13-004、P14-001…008、P15-002… 等 |
| 有账无据（TODO 但消息命中） | 3 | P15-001、QA-V19R8-S3-11 等 |

- 本体 146 号在提交消息中的命中率（本组严格词边界复算）= 32/146 = 21.9%，与容器聚合 95.5% 不属同一集合（本体号族 P/QA 与子包号族交集为 0，实测）。
- 包内 `evidence/` 25 目录中 24 个能对上主台账任务号，1 个（R10-001）对不上；主台账中 122 行的 `evidence_dir` 指向的目录在容器内不存在。
- V6.1 三方（对照，容器内无台账）：包外 `evidence/v6_1_rework/TASK_LEDGER.csv` 67 行（PASS 57 / WAITING_WINDOWS 7 / NOT_STARTED 3）与 `evidence/v6_1_rework/tasks/` 68 目录；容器内该子包仅剩 02_FINDINGS.csv。

### 7.5 blob 级抽查

- `RELEASE_V5/…/02_TASK_LEDGER.csv`：容器磁盘 = b7b2dea7 中 blob = 8e18fa789e… = HEAD（无脏改）；zip 内为 907137567225…（10,712 B，全 NOT_STARTED）；f99e80d8 复原件为 10,705 B（PASS 1 / NOT_STARTED 97）→ 三个形态三个 digest。
- `REAUDIT_V3/v3_reaudit/02_TASK_LEDGER.csv` blob 37717230fa…、`templates/COMMITS.csv` blob 853efdaef1…（容器与 HEAD 一致）。
- 提交侧留痕抽样：V6 包外台账最近改动为 8be6eb4e、196b2e8e（2026-08-31，"G10-WIN: WIN-001..006 WAITING_WINDOWS"）；V5 审核包侧为 fd4e11ed、52e9541d（2026-08-30，自述"88 PASS, WIN-006 IN_PROGRESS"）；V6.1 残件为 8c71f7ab（2026-09-02，"REL-002: FINDINGS P0/P1 关闭 36/40"）。

---

## 八、缺口与不确定

1. `review_inputs/P11-004_review_bundle.zip` 读不到：`SHA256SUMS.txt:71` 登记其 hash（f3562ff6…）、MANIFEST.md 与 README.md 均称内含原始审核包，但容器内无该文件；`git log --all --diff-filter=A --name-only | grep -c review_inputs` = 0 → 该文件从未进入 Git。后果实测：`tools/validate_pack.py` 退出码 2（第八节证据即实跑输出）。
2. 本体 `tools/validate_pack.py:16` 要求每行台账有 `tasks/<id>.md`，实测 146 行中 96 行（全部 QA-* 号）无同名任务文件 → 该机器门在归档态不可能通过（未证实当初是否运行过）。
3. 容器自带清单 `SHA256SUMS.txt` 与本体不一致 9 条：AUTONOMOUS_ENTRY.md、contracts/wcs_authoritative_pairs.schema.json、control/CURRENT_TASK.md、control/DECISION_REGISTER.md、control/MASTER_TASK_REGISTER.csv、control/PROJECT_STATE.yaml、control/REQUIREMENTS_TRACEABILITY.csv、control/RISK_REGISTER.csv、docs/18_CODE_CHANGE_MAP.md（另有 1 条缺失=第 1 条）。
4. `MANIFEST.md` 与本体不一致：自述 Files 133，实际反引号条目 131；容器本体 588 文件中 458 个不在 MANIFEST 清单内；MANIFEST 的 Tasks 50 与主台账 146 行、tasks/ 根 52 个 .md 三个口径互不相同。
5. 证据文件三处对同一容器的"台账行"口径不同：digest 列 4 张子包台账（54/88/62/98）、`pack_inventory.csv` 该行"台账行数"= 54、`pack_lineage_table.md:42` = 98；三者均未登记本体 `control/MASTER_TASK_REGISTER.csv`（146 行）。另 digest 未输出"规格文件"行（其口径只扫包根 NN_*.md，而本体的 31 个编号规格在 `docs/` 下、各子包编号规格在各子包根）；digest "tasks/ 共 85" 为递归 `.md` 计数（本体 tasks 根 52 + tasks/acr 33），而容器 tasks/ 实际 102 文件；digest 的 scripts 字段为空（本体三个脚本位于 `tools/`，不在 `scripts/`）；digest 的 read_first 指向 `CONTROL_V4/…/00_READ_FIRST.md`（容器根本无 00_READ_FIRST.md，其入口文件为 AUTONOMOUS_ENTRY.md 与 README.md）；digest git 事件记 adds_n=0、mod_n=2，而 b7b2dea7 在容器路径实为 854 R100 + 1 A（`README_ARCHIVED.md`）。
6. git 事件口径：digest 记本实例 adds_n=0、mod_n=2，而 b7b2dea7 实测在容器内为 854 R100 + 1 A（`README_ARCHIVED.md`）；两者口径不可直接互推（未证实计数规则）。
7. `tasks/acr/SHA256SUMS.txt:26` 登记 `CHECKLIST.md`（大写），磁盘为 `checklist.md`（小写，3119 B，sha256 与清单一致）→ 名称大小写不符导致按清单校验报"缺失 1"；实测该文件内容存在。
8. AstroCS_V6_1_REWORK_CONTROL_20260831 归属无法归一：容器内 1 文件 `02_FINDINGS.csv`（11,218 B，'0f0c6573cffe'）比该身份 zip 内同名文件（7,357 B，'b39def2b31db'）多 2 列（resolution_commit、resolution_evidence_sha256），40 个 finding_id 集合与 severity/status 结构一致；现存 46 文件解包件中不含 02_FINDINGS.csv（zip 独有 1 文件，实测差集 {'02_FINDINGS.csv'}）；`nested_packs` 未列该目录；其身份 digest 只有 2 个实例。→ 三方（容器/zip/现存解包件）互不完整，容器内该文件属"终态执行留痕"还是"另一份本体"未证实。
9. REAUDIT_V3 子包相对其 zip 原件（`AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827.zip`，21 文件 / 51,229 B）：容器多 2 个文件（`09_CONTINUOUS_EXECUTION_ORDER.md` 5,320 B、`10_REVIEWER_DEVIATIONS.md` 1,346 B），zip 无独有文件；21 个同名文件中 15 个大小不同，其中 13 个恰小 1 字节（例：00_READ_FIRST.md 2824/2825、03_TASK_SPECIFICATIONS.md 14082/14083、templates/TRACEABILITY.csv 103/104），另 2 个为实质差异（`02_TASK_LEDGER.csv` 容器 4414 vs zip 4766；`templates/COMMITS.csv` 容器 2715 vs zip 110）。仓库根 `.gitattributes` 存在且 `git check-attr text` 对该包 .md 返回 auto → 未证实 1 字节差是否为行尾归一化所致。
10. tasks/acr 无法与复原件对照：容器 50 文件 vs 该身份唯一历史复原件 `history/49ea5c1e/工程控制/tasks/acr/` 仅 3 个文件（provenance 自述 recovered_files=3、recovered_from_rev=49ea5c1e^、first_commit=12fb99f3）；两侧唯一同名文件 `checklist.md` 内容不同（容器 3,119 B / 复原件 9,616 B），复原件独有 `spec.md`（24,069 B）与 `tasks.md`（13,174 B）在容器内不存在，容器独有 47 个文件在复原件中不存在 → 该身份的归档形态与仅历史复形态不可互证（复原件为部分恢复）。
11. 影子树与实例数不可混用：72 份 `run/` 容器副本全部落在 `.gitignore:19 run/*`（`git ls-files run` = 1）；70 份与容器 855 文件/41,378,301 字节完全一致，2 份（`run/perf-fix/P10-util2/src` 与 `src-base`）只有 422 文件、缺容器全部 433 个 `evidence/` 文件、无内容冲突。
12. 命中率跨代串号：见 7.1 末段（ID-001、HIPS-001 的"命中"来自后期 `DATA-FRAME-ID-001`、`DATA-HIPS-001` 等其它包号族）；`pack_commit_link.csv` 未给出匹配实现细节 → 该差异属计数口径差异，未证实哪一种为"正确"。
13. `README_ARCHIVED.md:4,26` 两次自述"854 个 Git 跟踪文件"，实测 855（含该说明文件本身）；b7b2dea7 提交消息把目标目录写作 `engineering/control/archive/2026-09-02_legacy`（无 `_工程控制_v1.3-to-v6.1` 后缀）→ 自述与实际目录名不同。
14. 替代指向断链：`README_ARCHIVED.md:18` 指向 `工作根 control/active/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/`，该路径在 Git 中不存在（`git ls-files | grep -c V7_MODULAR` = 0；`engineering/control/active/` 下唯一曾跟踪路径为 V8_1 包）；断链由 `docs/DOCUMENT_INDEX.yaml:601` notes 记录，现 replacement 已改指 RELEASE_RESCUE_V3 / CONSTITUTION_ALIGNMENT_V1（同文件 :600）。原路径是否曾以其它形式存在：未证实。
15. `evidence/P11-004` 只余 15 文件（前身复原件 80 文件），差 74 个由 036a3bb5 的 111 条 D 删除（该提交自述清理范围含"P11-004 备份(192MB)"、"旧 review bundle(38MB)"）→ 容器内 P11-004 留痕不完整。
16. 本体 `migration/` 两文件（INSTALL_REPORT_20260730T024407Z.json 等）来自 a78f5430 的 2 条 R100，原路径为 `_v2_pack/` 与 `engineering_authoritative/`；该两目录现存形态本组未见跟踪实例 → 迁移来源无法在本容器内闭环核对。

---

## 片尾一 · 与上一组（G10，末包 AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3）的衔接

- 时间衔接：本容器唯一 git 事件为 b7b2dea7（2026-09-02T22:15:41+08:00）；G10 末包名同为 20260902 日戳，其 zip 形态 99 文件 / 398,022 字节（`_evidence/packs/pack_inventory.csv` V7 行）。
- 归档范围自述核对：b7b2dea7 提交消息只自述 GOV-002 的分类规则与条数（"881 rename (854 控制文档→engineering/control/archive/2026-09-02_legacy, 27 技术文档→docs/archive)"）、"tests: check_doc_index.py DOC_INDEX_PASS 10/10"、"scientific_change: NO"；**消息中不出现 V7 或 FINAL3 字样**（实测 grep 计数 0）。
- 与 V7 的关系只出现在同提交新增的 `README_ARCHIVED.md:18`（替代表"当前执行控制包"行）与 `docs/DOCUMENT_INDEX.yaml:600` 的 replacement 字段；两处指向的 `control/active/…V7…FINAL3/` 路径在 Git 中不存在（实测 0 条跟踪），该断链由 DOCUMENT_INDEX:601 的 notes 于 2026-09-09 GOV-002 修正记录，并进一步在 8e03d7da（2026-09-10）与 DOCUMENT_INDEX:602-606 的 `2026-09-09_superseded_V8.1_CI_CONTROL_20260905` 条目中被改指到 RELEASE_RESCUE_V3 / CONSTITUTION_ALIGNMENT_V1。
- 收纳关系判定（只按文件集合）：容器 `nested_packs` 9 条中无任何 V7/V7.1 目录，容器内 `find -iname '*V7*'` = 0 命中 → **本容器与 G10 的 V7 包不存在文件集合上的收纳或继承关系**，仅有归档说明上的"替代指向"。V7 自身解包件未跟踪（`git ls-files` 0），其 zip 复原件另存于 `_evidence/packs/history_zip/` 与 V8.1 归档 baseline 内。

## 片尾二 · 重叠身份

本组 1 个容器身份的收纳成员，与其它组登记的成员身份为同一批目录，实测数值逐项一致（文件数与字节完全相同，判定为同形态）：

| 重叠身份 | 出现组 | 容器内数值 vs 该组数值 | 与本组 digest 是否一致 |
|---|---|---|---|
| AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 | G05、G06 | 26 / 47,997 = digest worktree"26 文件 46.9 KB" | 一致（digest 共 3 实例，容器为其中 worktree） |
| AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 | G08 | 46 / 200,775 = digest"46 文件 196.1 KB" | 一致（共 3 实例） |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 | G07 | 37 / 99,444 = digest"37 文件 97.1 KB" | 一致（共 2 实例） |
| REAUDIT_V3/v3_reaudit | G05 | 23 / 56,879 = digest"23 文件 55.5 KB" | 一致（共 2 实例） |
| acr（tasks/acr） | G04 | 50 / 148,117 = digest"50 文件 144.6 KB" | 一致（共 2 实例） |
| ACR_FOCUSED_CONTROL_PACKAGE / _V2 / _V3 / _V4 | G04 | 20/44,827、21/53,653、21/55,083、22/47,183 | 一致（各共 3 实例） |
| AstroCS_V6_1_REWORK_CONTROL_20260831 | G09 | 容器内仅 1 / 11,218；G09 侧为 46/209,075（解包件）与 47/214,426（zip） | **不一致**：该身份 digest 未登记容器内 1 文件形态（第八节第 8 条） |
| CONTROL_V4、CONTROL_V6（父目录级身份） | G06、G08 | 容器内该两级目录只是包裹层，不计为子包；其 digest 各只 1 实例且引用的是 `history/…` 复原件（0 处容器路径引用，实测 grep） | 不重叠（形态不同层级） |
| REAUDIT_V3/v3_audit、v3_cp0、工程控制/REAUDIT_V3 | G05 | 容器内不存在这些路径（3703650d 引入、ac2ced53 删除；报告改落 `reports/REAUDIT_V3/`） | 不重叠 |
| AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3 | G10 | 容器内 0 命中；其 4 个 zip 形态均在容器外 | 不重叠 |

---

### 取证方法附注（可复现）

- 只读命令：`git ls-files` / `git log` / `git show --name-status` / `git ls-tree` / `git hash-object` / `git cat-file`、python3 遍历与 hashlib 复算、`python3 zipfile` 只读列目录；未解压任何 zip 进仓库，未修改任何既有文件，未执行任何 git 写操作。
- 中文路径统一用 `git -c core.quotepath=off` 或按八进制转义字面匹配（`\345\267\245\346\216\247\345\210\266` = 工程控制）。
- 比对历史复原件时一律排除脚本注入的 `_PROVENANCE.json`。
