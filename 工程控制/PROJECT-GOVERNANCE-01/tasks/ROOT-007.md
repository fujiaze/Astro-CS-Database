# 任务：ROOT-007 作废世代治理入口清除（历史控制包产物归档）

状态：IN_PROGRESS（前台执行，负责人已授权「历史版本控制包全部作废」）
依赖：—　文件域互斥组：S4-U

## 负责人裁决（2026-09-16）

- 「**历史版本控制包全部作废**」+「你自行裁决，我觉得没啥用」⇒ 作废世代的治理入口与产物**不再保留在仓库**。
- 例外（前台自裁，理由见下）：**实测数据类**逐项判定，不随治理层一起删。

## 处置清单

### A. 删除（作废世代的治理入口，tracked）

| 文件 | 理由 |
|---|---|
| `ASTROCS_PROJECT_CONSTITUTION.md` | 旧宪章；权威已由 `ASTROCS_DESIGN.md` §0 权威链接管 |
| `AstroCS_ENGINEERING_CONSTRAINTS.md` | 自述 ARCHIVED_NON_NORMATIVE；约束已由 `ENGINEERING_SPEC.md` 接管 |
| `REVIEW.md` | 旧控制包时代的外部复核报告（旧 ID/旧状态） |
| `CHANGELOG.md` | 旧控制包时代的发布流水（CC-*/V8-* 条目）；重构后由 git 历史承担 |

### B. 归档后删除（历史控制包的执行产物，tracked，约 29 MB / 2799 文件）

- `evidence/refactor/`（298 文件）、`evidence/v6_1_rework/`（347）、`evidence/v8_1_ci_control/`（2153）、`evidence/ciqa/`（1）；
- 这些是**旧控制包（V6.1/V8.1/REFACTOR/CIQA）的验收证据**，其控制包本体已在 `a861d8f6` 删除；
- 处置：先打 tar.gz 到 `run/archive/legacy-control-packs/`（不入库、不占工作树），再删除工作树副本；
- 保留判据：新文档集未引用它们；被删后若某报告引用失效，登记为「历史引用（已归档）」而非缺口。

### C. 保留（不入本轮删除）

| 项 | 理由 |
|---|---|
| `memory.md` | 仍被 `ENGINEERING_SPEC.md` §7 白名单列为仓库根固定条目 → 归 GOV-001/DOC-001 定去留 |
| `HANDOVER.md` | 工程规范白名单提及（交接入口）；内容陈旧归 DOC-001 |
| `artifacts/**` | 含**实测性能数据**（`MEASUREMENTS.csv`：baseline_ns / avx2_variant_ns / improvement_pct = 真基准测量）、CI 运行产物（`artifacts/ci/`，1644 文件，**今日仍在更新**）、旧评审包 zip；**删除决定须负责人单独确认**（数据不可再生） |
| `evidence/` 中任何被活动面引用的文件 | 若有，改归档链接后随 DOC-001 处理 |

## 负责人第二轮裁决（2026-09-16）

- 「**没必要归档，留着你也不看，纯浪费空间**」⇒ 不保留归档副本：
  - `artifacts/**`（82 MB / 2081 文件，含旧审核包 zip 56 MB、旧 capsules、真实基准 CSV、今日仍在更新的 `artifacts/ci/`）→ **直接删除**；
  - 已生成的 `run/archive/legacy-control-packs/evidence-legacy-control-packs.tar.gz` → **一并删除**（evidence 的最终提交 `7940d70e` 已终结其 tracked 副本，归档副本无独立价值）；
- 数据安全性依据：删除仅为**工作树/索引**层面的移除，内容永久留存于 git 历史（`git show <commit>:<path>` 可取回）；
- 护栏：`.gitignore` 增加 `artifacts/ci/` 与 `artifacts/**/*.zip`（含 capsules），防止 CI 产物再次散落根下；
- 仍保留：`memory.md`、`HANDOVER.md`（§7 白名单条目，归 GOV-001/DOC-001）。

## 验收门

- [ ] 四个根文档已删除且 tracked 集合相应减少（给出前后 `git ls-tree -r HEAD | wc -l`）
- [ ] `evidence/` 归档包存在、可列出内容（`tar tzf | wc -l` == 原文件数 2799）
- [ ] 归档包不在仓库内（`run/` 已 gitignore）且未入库
- [ ] 无活动面因删除而出现悬空引用（对 4 个文件名跑引用扫描，命中项逐条判定归属）
- [ ] `artifacts/**` 与 `memory.md`、`HANDOVER.md` **未被本任务改动**（`git status` 自查）

## 交付物

1. 删除提交（前台）；2. 归档包与清单；3. 悬空引用复核结果。