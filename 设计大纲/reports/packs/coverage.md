# coverage.md — 任务二 49 包身份 × 负责组 覆盖核对（含非包形控制件）

> 生成层：任务二前台归纳层。机械底稿：设计大纲/_evidence/packs/pack_inventory.csv（49 身份 / 74 存形态实例）与设计大纲/_evidence/packs/dispatch_groups.json。核对方法：对每个身份与每个实例路径，在 设计大纲/reports/packs/P-G*.md 全文做字符串检索（身份名含 / 时同时检索 __ 变体；路径未整串命中时降级到"末段目录名/文件名片段 + sha256"检索并人工判定）。核对时间：2026-09-14。

## A. 49 个包身份逐条核对表

| 身份 | 负责组 | 实例数 | 存形态 | 是否覆盖 | 备注 |
|---|---|---|---|---|---|
| 2026-09-09_superseded_V8.1_CI_CONTROL_20260905 | G12 | 1 | worktree | 是 | P-G12 §一/§六 |
| ACR_FOCUSED_CONTROL_PACKAGE | G04 | 3 | recovered×2, worktree | 是 | P-G04（三形态逐字节一致） |
| ACR_FOCUSED_CONTROL_PACKAGE_V2 | G04 | 3 | 同上 | 是 | P-G04 |
| ACR_FOCUSED_CONTROL_PACKAGE_V3 | G04 | 3 | 同上 | 是 | P-G04 |
| ACR_FOCUSED_CONTROL_PACKAGE_V4 | G04 | 3 | 同上 | 是 | P-G04 |
| AUDIT_PACKAGE_587fe0e341a7 | G07 | 1 | zip | 是 | P-G07（1699 条目，SHA256SUMS 全过） |
| AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905 | G12 | 3 | worktree, zip, recovered | 是 | P-G12；zip 实例以 sha256 77a4b03a… + _control_packs 路径引用（整串路径 0 命中，判"引用-缩写"） |
| AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3 | G10 | 4 | zip, zip_recovered | 是 | P-G10（含 run/ 影子树 72 份实测补登） |
| AstroCS_AUDIT_REVIEWPACK_20260909T133841Z | **G99→G13 并入** | 1 | zip | 是（随 G13） | 归并理由见 B 节 |
| AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0 | G02 | 1 | zip_recovered | 是 | P-G02 |
| AstroCS_Authoritative_Development_Pack_v2.0 | G02 | 1 | recovered | 是 | P-G02 |
| AstroCS_CLI_Core_Development_Pack | G01 | 1 | recovered | 是 | P-G01 |
| AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1 | G01 | 1 | zip_recovered | 是 | P-G01 |
| AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909 | G13 | 1 | worktree | 是 | P-G13 §一（首入库 e9547c02 09-10 15:14、末 f5f944a5 09-12 18:52；inventory 首末方向倒置已在 P-G13 缺口登记） |
| AstroCS_CP0 | G05 | 2 | zip, zip_recovered | 是 | P-G05（双副本同 sha256 c79f50d7…） |
| AstroCS_Delivery_20260729 | G01 | 1 | zip_recovered | 是 | P-G01 |
| AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826 | G05 | 1 | zip | 是 | P-G05（zip 未入库，无提交锚） |
| AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828 | G05,G06 重叠 | 3 | worktree, zip, recovered | 是 | P-G05 与 P-G06 双报告，引用同一 digest，实测一致 |
| AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 | G05,G06 重叠 | 1 | zip | 是 | 同上 |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828 | G07 | 2 | worktree, recovered | 是 | P-G07 |
| AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828 | G07 | 1 | zip | 是 | P-G07（ALPHA 仅存在于 zip 文件名） |
| AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z | G05 | 1 | zip | 是 | P-G05 |
| AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912 | G14 | 2 | worktree, zip | 是 | P-G14（两形态均未入库） |
| AstroCS_Stage1_HISS_Agent_Package_2026-07-31 | G03 | 1 | recovered | 是 | P-G03 |
| AstroCS_Stage1_HISS_Delivery | G03 | 1 | recovered | 是 | P-G03 |
| AstroCS_Stage1_HISS_Delivery_2026-07-31 | G03 | 1 | zip_recovered | 是 | P-G03 |
| AstroCS_Stage1_Wiki_Freeze_2026-07-30 | G02 | 1 | recovered | 是 | P-G02 |
| AstroCS_V6_1_REWORK_CONTROL_20260831 | G09 | 2 | worktree, zip | 是 | P-G09（容器内 1 文件形态由 G09/G11 双登记口径差，见各自片尾段） |
| AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830 | G08 | 3 | worktree, zip, recovered | 是 | P-G08 |
| AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3 | G10 | 1 | worktree | 是 | P-G10 |
| CONTROL_V4 | G06 | 1 | recovered | 是 | P-G06（判定为 V4 包外层容器身份） |
| CONTROL_V6 | G08 | 1 | recovered | 是 | P-G08（判定为 V6 包外层容器身份） |
| REAUDIT_V3/v3_audit | G05 | 2 | worktree, recovered | 是 | P-G05（v3_cp0 的 recovered 路径 history/3703650d/… 以身份级+目录名引用，判"引用-缩写"） |
| REAUDIT_V3/v3_cp0 | G05 | 2 | worktree, recovered | 是 | 同上（P-G05 内 "v3_cp0" 15 处） |
| REAUDIT_V3/v3_reaudit | G05 | 2 | worktree, recovered | 是 | P-G05 |
| REAUDIT_V4/v4_reaudit | G07 | 1 | recovered | 是 | P-G07（自造包，b12305ed 删除） |
| _agent_package | G03 | 1 | recovered | 是 | P-G03（与 AstroCS_Stage1_HISS_Agent_Package_2026-07-31 父子重叠） |
| _new_pack_v1.1 | G01 | 1 | recovered | 是 | P-G01（与 CLI_Core 两身份同包） |
| acr | G04 | 2 | worktree, recovered | 是 | P-G04/P-G11（容器内 tasks/acr 路径两报告均引用） |
| archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1 | G11 | 1 | worktree | 是 | P-G11（容器专项） |
| cprun-v4-pack-template | G13 | 1 | worktree | 是 | P-G13（另有 2 份 zip 内嵌副本 + 70 份 run/ 影子副本实测补登） |
| engineering_archive_v1.0 | G01 | 1 | recovered | 是 | P-G01 |
| engineering_authoritative | G02 | 1 | recovered | 是 | P-G02 |
| engineering_v1.2 | G01 | 1 | recovered | 是 | P-G01 |
| engineering_v1.3 | G01 | 1 | recovered | 是 | P-G01（复原件截断 552→400，缺失 124 文件在 G11 容器找回，两报告互证） |
| prerelease_v5/AUDIT_REVIEW | G07 | 1 | worktree | 是 | P-G07 |
| prerelease_v5/audit_src | G07 | 1 | worktree | 是 | P-G07 |
| 工程控制/REAUDIT_V3 | G05 | 1 | recovered | 是 | P-G05 |
| 工程控制/REAUDIT_V4 | G07 | 1 | recovered | 是 | P-G07（与 REAUDIT_V4/v4_reaudit 同包容器/内包） |

**小结**：49 身份中 **49/49 已有负责组报告落盘覆盖**（P-G13 已于 05:2x 落盘，60,126 B，覆盖宪章对齐包/cprun 模板/G99 审核包 3 身份，20 条缺口；本报告小结由"P-G13 在跑"更新为闭合）。74 个存形态实例：71 个整路径或身份级明确引用，3 个判"引用-缩写"（V8.1 zip、v3_cp0 recovered、acr worktree，上表已注判定依据），0 个完全未引用。注：P-G13 实测修正了 pack_inventory 对宪章对齐包"首次/最后提交"方向倒置（真序：首 e9547c02 09-10、末 f5f944a5 09-12），A 表该行与 00_OVERVIEW 谱系表已按 P-G13 订正。

## B. G99 并入 G13 的理由

- dispatch_groups.json 中 G99 仅 1 身份 AstroCS_AUDIT_REVIEWPACK_20260909T133841Z（zip、14556 文件、98 台账行、无提交锚），前台指令将其并入 G13。
- 客观依据：该审核包时间戳 2026-09-09T133841Z 与 G13 的宪章对齐包（20260909）同窗口；P-G08 实测其 zip 内嵌 46 份 V6 文件（归档路径），属"审核封装件"而非独立控制包，与 G13 的"控制包规范/元层"主题同域。归并后 G13 覆盖 3 身份，P-G13 报告第一节需含 G99 行。

## C. 非包形控制件（前台追加，不在 pack_inventory.csv）

| 对象 | 负责组 | 报告 | 是否覆盖 | 备注 |
|---|---|---|---|---|
| 工程控制/包规范.md | G15 | P-G15-包规范与活动状态.md | 是 | 从未入 git（仅提交消息 19ef3722 提及），mtime 09-04 |
| 工程控制/ACTIVITY_STATE.md | G15 | 同上 | 是 | 权限 0600；git 首入库 8e03d7da（09-10）；末更新 5a250999（09-13） |
| 问题扫描/（RQS-2026-01 工作区） | G16 | P-G16-RQS现行程序.md | 是 | 742 文件全入库；首提交 43c5c488（09-13）至 HEAD b67c32fa（09-15）；非 zip 派发件 |

覆盖总数按"49 包身份 + 3 非包形对象"计，共 52 个分析对象；**52/52 覆盖**（P-G13 落盘后 49/49 闭合）；非包形 3/3 覆盖。

## D. 缺口登记

1. ~~P-G13 在跑~~ **已闭合**：P-G13-宪章对齐与包模板.md 落盘，3 身份全覆盖，20 缺口。前台实测骨架与其一致（根件 20 项、control-pack.json cprun/v4 tasks=57/lanes=11/gates=5/max_parallel=12、台账 57 行 PASSED 29/NOT_STARTED 23/IN_PROGRESS 5、无 SHA256SUMS/START_PROMPT）；P-G13 增补实测：首末提交方向订正（e9547c02 首/f5f944a5 末）、首入库版机器门 CONTROL_FAIL（25 行 vs 图 29 任务，缺 WCS-003/BASE-UTIL-001/P1-HIPS-DIGEST-001/ARCH-AUDIT-P1）、G99 被采 98 号台账经核为 V5 系件（与 prerelease_v5/AUDIT_REVIEW 台账逐字节同）。残余缺口并入 225 条总缺口（见 00_OVERVIEW §5）。
2. 实例引用"缩写"3 处（A 节注），不构成内容缺口。
3. pack_inventory.csv 中 AstroCS_ALPHA3_…FINAL3 的 4 实例存形态字段为 zip/zip_recovered 混标（P-G10 实测 4 落点含 2 个 V8.1 baseline 内嵌 + history_zip 复原 + _control_packs；run/ 影子树 72 份未入 inventory）——登记口径缺口，报告内已补登。
