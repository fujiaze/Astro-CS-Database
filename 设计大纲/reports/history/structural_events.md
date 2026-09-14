# 结构事件表（阶段划分的客观信号）

由 index.jsonl 机械生成（脚本 _tools/09_structural_events.py）。序号 seq 为拓扑序（上游谱系分组聚合），不是严格日期序。

| 事件类型 | 数量 |
|---|---|
| NEWDIR | 375 |
| LEDGER | 289 |
| NEWDIR_BATCH | 185 |
| CI | 63 |
| PACKNEW | 52 |
| PACKDEL | 12 |
| DELETE | 10 |
| SILENT | 6 |
| RENAME50 | 6 |
| PRESET | 3 |

## 控制包引入与废止（PACKNEW / PACKDEL）
| seq | sha8 | 日期 | 事件 | 详情 |
|---|---|---|---|---|
| 128 | ba4f0d34 | 2026-07-25 | PACKNEW | 新增包文件 _new_pack_v1.1/AstroCS_CLI_Core_Development_Pack/AUTONOMOUS_ENTRY.md |
| 128 | ba4f0d34 | 2026-07-25 | PACKNEW | 新增包文件 _new_pack_v1.1/AstroCS_CLI_Core_Development_Pack/START_PROMPT.txt |
| 128 | ba4f0d34 | 2026-07-25 | PACKNEW | 新增包文件 engineering/AUTONOMOUS_ENTRY.md |
| 128 | ba4f0d34 | 2026-07-25 | PACKNEW | 新增包文件 engineering/START_PROMPT.txt |
| 162 | 0d1857de | 2026-07-27 | PACKNEW | 新增包文件 engineering_v1.2/AUTONOMOUS_ENTRY.md |
| 162 | 0d1857de | 2026-07-27 | PACKNEW | 新增包文件 engineering_v1.2/START_PROMPT.txt |
| 175 | f4ec8b24 | 2026-07-28 | PACKNEW | 新增包文件 engineering_v1.3/AUTONOMOUS_ENTRY.md |
| 175 | f4ec8b24 | 2026-07-28 | PACKNEW | 新增包文件 engineering_v1.3/START_PROMPT.txt |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 _new_pack_v1.1/AstroCS_CLI_Core_Development_Pack/AUTONOMOUS_ENTRY.md |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 _new_pack_v1.1/AstroCS_CLI_Core_Development_Pack/START_PROMPT.txt |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 engineering/AUTONOMOUS_ENTRY.md |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 engineering/START_PROMPT.txt |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 engineering_v1.2/AUTONOMOUS_ENTRY.md |
| 203 | 036a3bb5 | 2026-07-29 | PACKDEL | 删除包文件 engineering_v1.2/START_PROMPT.txt |
| 204 | 75b05f72 | 2026-07-30 | PACKNEW | 新增包文件 _v2_pack/AstroCS_Authoritative_Development_Pack_v2.0/AUTONOMOUS_ENTRY.md |
| 204 | 75b05f72 | 2026-07-30 | PACKNEW | 新增包文件 _v2_pack/AstroCS_Authoritative_Development_Pack_v2.0/START_PROMPT.txt |
| 226 | 78a9121d | 2026-07-31 | PACKNEW | 新增包文件 AstroCS_Stage1_HISS_Delivery/MANIFEST.json |
| 226 | 78a9121d | 2026-07-31 | PACKNEW | 新增包文件 _agent_package/AstroCS_Stage1_HISS_Agent_Package_2026-07-31/MANIFEST.json |
| 226 | 78a9121d | 2026-07-31 | PACKNEW | 新增包文件 _wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/MANIFEST.json |
| 237 | a78f5430 | 2026-07-31 | PACKDEL | 删除包文件 AstroCS_Stage1_HISS_Delivery/MANIFEST.json |
| 237 | a78f5430 | 2026-07-31 | PACKDEL | 删除包文件 _agent_package/AstroCS_Stage1_HISS_Agent_Package_2026-07-31/MANIFEST.json |
| 237 | a78f5430 | 2026-07-31 | PACKDEL | 删除包文件 _v2_pack/AstroCS_Authoritative_Development_Pack_v2.0/AUTONOMOUS_ENTRY.md |
| 237 | a78f5430 | 2026-07-31 | PACKDEL | 删除包文件 _v2_pack/AstroCS_Authoritative_Development_Pack_v2.0/START_PROMPT.txt |
| 237 | a78f5430 | 2026-07-31 | PACKDEL | 删除包文件 _wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/MANIFEST.json |
| 293 | 49ea5c1e | 2026-08-02 | PACKNEW | 新增包文件 工程控制/tasks/acr/00_AGENT_START_PROMPT.txt |
| 293 | 49ea5c1e | 2026-08-02 | PACKNEW | 新增包文件 工程控制/tasks/acr/00_READ_FIRST.md |
| 406 | 0030c3a5 | 2026-08-05 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE/00_AGENT_START_PROMPT.txt |
| 406 | 0030c3a5 | 2026-08-05 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE/00_READ_FIRST.md |
| 429 | 40c9d216 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V2/00_AGENT_START_PROMPT.txt |
| 429 | 40c9d216 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V2/00_READ_FIRST.md |
| 438 | a36c4825 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V3/00_AGENT_START_PROMPT.txt |
| 438 | a36c4825 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V3/00_READ_FIRST.md |
| 460 | 38b85702 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V4/00_AGENT_START_PROMPT.txt |
| 460 | 38b85702 | 2026-08-06 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V4/00_READ_FIRST.md |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE/00_AGENT_START_PROMPT.txt |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE/00_READ_FIRST.md |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V2/00_AGENT_START_PROMPT.txt |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V2/00_READ_FIRST.md |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V3/00_AGENT_START_PROMPT.txt |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V3/00_READ_FIRST.md |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V4/00_AGENT_START_PROMPT.txt |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/docs/ACR_FOCUSED_CONTROL_PACKAGE_V4/00_READ_FIRST.md |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/tasks/acr/00_AGENT_START_PROMPT.txt |
| 550 | 198d69e0 | 2026-08-10 | PACKNEW | 新增包文件 工程控制/tasks/acr/00_READ_FIRST.md |
| 978 | 3703650d | 2026-08-27 | PACKNEW | 新增包文件 工程控制/REAUDIT_V3/v3_audit/00_READ_FIRST.md |
| 978 | 3703650d | 2026-08-27 | PACKNEW | 新增包文件 工程控制/REAUDIT_V3/v3_audit/MANIFEST.json |
| 978 | 3703650d | 2026-08-27 | PACKNEW | 新增包文件 工程控制/REAUDIT_V3/v3_cp0/00_READ_FIRST.md |
| 978 | 3703650d | 2026-08-27 | PACKNEW | 新增包文件 工程控制/REAUDIT_V3/v3_cp0/MANIFEST.json |
| 978 | 3703650d | 2026-08-27 | PACKNEW | 新增包文件 工程控制/REAUDIT_V3/v3_reaudit/00_READ_FIRST.md |
| 1037 | 020cdc99 | 2026-08-28 | PACKNEW | 新增包文件 工程控制/REAUDIT_V4/v4_reaudit/00_READ_FIRST.md |
| 1039 | b12305ed | 2026-08-28 | PACKNEW | 新增包文件 工程控制/CONTROL_V4/AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828/00_READ_FIR |
| 1039 | b12305ed | 2026-08-28 | PACKDEL | 删除包文件 工程控制/REAUDIT_V4/v4_reaudit/00_READ_FIRST.md |
| 1042 | f99e80d8 | 2026-08-28 | PACKNEW | 新增包文件 工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/00_READ_FI |
| 1042 | f99e80d8 | 2026-08-28 | PACKNEW | 新增包文件 工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/START_PROM |
| 1232 | ef0858c5 | 2026-08-30 | PACKNEW | 新增包文件 artifacts/prerelease_v5/AUDIT_REVIEW/00_READ_FIRST.md |
| 1232 | ef0858c5 | 2026-08-30 | PACKNEW | 新增包文件 artifacts/prerelease_v5/AUDIT_REVIEW/MANIFEST.json |
| 1261 | 4b1b948e | 2026-08-30 | PACKNEW | 新增包文件 工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/00_READ_FIRST.md |
| 1261 | 4b1b948e | 2026-08-30 | PACKNEW | 新增包文件 工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/MANIFEST.json |
| 1261 | 4b1b948e | 2026-08-30 | PACKNEW | 新增包文件 工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/START_PROMPT.txt |
| 1521 | a4fdee3f | 2026-09-05 | PACKNEW | 新增包文件 engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20 |
| 1521 | a4fdee3f | 2026-09-05 | PACKNEW | 新增包文件 engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20 |
| 1784 | 4d3feaf4 | 2026-09-09 | PACKNEW | 新增包文件 工程控制/cprun-v4-pack-template/control-pack.json |
| 1796 | e9547c02 | 2026-09-10 | PACKNEW | 新增包文件 工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/00_READ_FIRST.md |
| 1796 | e9547c02 | 2026-09-10 | PACKNEW | 新增包文件 工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/control-pack.json |

## 目录重构与批量删除（RENAME50 / DELETE / NEWDIR_BATCH）
| seq | sha8 | 日期 | 事件 | 数量 | 详情 | 消息首行 |
|---|---|---|---|---|---|---|
| 39 | 55d69dd0 | 2026-07-12 | DELETE | 28 | 删除 28 文件（批量移除/归档） | refactor: split ahpx_io/stack/drizzle into independent repos |
| 95 | 7604fdca | 2026-07-16 | DELETE | 40 | 删除 40 文件（批量移除/归档） | 归档: 遗留浏览器+healpix_lod+healpix_io 源码归档, 依赖迁移至 astro_image_io, |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 118 | 1ac74251 | 2026-07-24 | NEWDIR_BATCH | 25 | 首次出现 25 个目录，例: engineering/00_START_HERE.md, engineering/01_PROJECT_CH | P00-001: 冻结并复核主仓库基线 |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | NEWDIR_BATCH | 39 | 首次出现 39 个目录，例: AstroCS_CLI_Core_Development_Pack_2026-07-24_v1.1.zip,  | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 128 | ba4f0d34 | 2026-07-25 | RENAME50 | 59 | 改名或移动 59 文件（目录重构） | P02-002/P02-005: v1.1包部署+P00-P02证据+候选路径API |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 161 | ed145a70 | 2026-07-27 | NEWDIR_BATCH | 23 | 首次出现 23 个目录，例: README.md, audit/AstroCS-v1.1-audit-pack, engineering/e | docs: 补充项目 README + 生成 v1.1 审计包 |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 162 | 0d1857de | 2026-07-27 | NEWDIR_BATCH | 24 | 首次出现 24 个目录，例: _toolkit_configs/install_v12.json, _toolkit_configs/ins | P09-001 冻结 v1.1 事实基线并建立 v1.2 证据 (VERDICT: PASS) |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 175 | f4ec8b24 | 2026-07-28 | NEWDIR_BATCH | 15 | 首次出现 15 个目录，例: engineering_v1.3/AUTONOMOUS_ENTRY.md, engineering_v1.3/ | P11-004: 完成 WCS Gate v2 双层闭环验证与 SIP 序列化修复 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | NEWDIR_BATCH | 16 | 首次出现 16 个目录，例: 工程控制/AUDIT_PACK.md, 工程控制/AUTONOMOUS_ENTRY.md, 工程控制/MANI | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | RENAME50 | 455 | 改名或移动 455 文件（目录重构） | chore: 根目录整理 + 工程控制包重命名 |
| 203 | 036a3bb5 | 2026-07-29 | DELETE | 3786 | 删除 3786 文件（批量移除/归档） | chore: 根目录整理 + 工程控制包重命名 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 204 | 75b05f72 | 2026-07-30 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Authoritative_Development_Pack_2026-07-30_v2.0. | feat(A-001): 安装权威开发包 v2.0，完成 README 迁移 |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 226 | 78a9121d | 2026-07-31 | NEWDIR_BATCH | 13 | 首次出现 13 个目录，例: AstroCS_Stage1_HISS_Delivery/00_README.md, AstroCS_Stag | feat(Stage1): 完成Stage1/HISS规范化交付 (Phase 0-6) |
| 237 | a78f5430 | 2026-07-31 | DELETE | 338 | 删除 338 文件（批量移除/归档） | chore: 整理根目录 + 创建 AGENT.md |
| 243 | 428746c6 | 2026-07-31 | DELETE | 21 | 删除 21 文件（批量移除/归档） | feat(stage1): Phase A Wiki重写 + B2 query_pixel测试 + E checksum |
| 310 | 23d07ed4 | 2026-08-03 | DELETE | 32 | 删除 32 文件（批量移除/归档） | feat(r10): 删除生产Python封装, 实现FP32/FP64双精度模式 |
| 322 | 8653bd54 | 2026-08-03 | DELETE | 69 | 删除 69 文件（批量移除/归档） | refactor(r10): Python 生产层彻底清理 |
| 332 | d557aea9 | 2026-08-04 | DELETE | 64 | 删除 64 文件（批量移除/归档） | test(acr): produce single-head real mixed and resource-contr |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 691 | 1d3a34c2 | 2026-08-15 | NEWDIR_BATCH | 17 | 首次出现 17 个目录，例: CHANGELOG.md, docs/BUILD_RELEASE.md, docs/CONFIG_REFERE | docs(v19): diagnostics tool + full pre-release developer/sci |
| 979 | ac2ced53 | 2026-08-27 | RENAME50 | 333 | 改名或移动 333 文件（目录重构） | REAUDIT_V3: 控制包仅保留在 工程控制，已完成报告移入 reports/REAUDIT_V3 |
| 980 | e1604f14 | 2026-08-27 | RENAME50 | 126 | 改名或移动 126 文件（目录重构） | 组织工程控制/报告：历史控制包归档，reports 按版本分目录存放 |
| 1039 | b12305ed | 2026-08-28 | DELETE | 21 | 删除 21 文件（批量移除/归档） | chore(v5): 按用户指示移除自造V4重审控制包并归位用户上传的V4_CPU_ADAPTIVE原包 — 自造包由前 |
| 1480 | b7b2dea7 | 2026-09-02 | RENAME50 | 881 | 改名或移动 881 文件（目录重构） | docs(governance): GOV-002 归档非当前工程文档 |
| 1785 | 789c5b6c | 2026-09-09 | DELETE | 368 | 删除 368 文件（批量移除/归档） | chore(repo): BASS DR3 用户数据区全目录 gitignore 收紧（原跟踪索引 coords.csv |
| 1787 | 8e03d7da | 2026-09-10 | RENAME50 | 56 | 改名或移动 56 文件（目录重构） | chore(GOV-002): 旧控制包归档统一活动状态——V8.1 CI 控制包 tracked 镜像 56 文件原样 |
