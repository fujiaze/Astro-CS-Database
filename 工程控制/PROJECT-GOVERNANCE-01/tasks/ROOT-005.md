# 任务：ROOT-005 根目录残留空目录与未登记产物目录处置

状态：NOT_STARTED
层：L0　依赖：ROOT-001　文件域互斥组：S4-S

## 目标

处置 ROOT-001 清运后剩下的**空目录**与**未登记产物目录**，并给出「保留 / 删除 / 登记为正式根条目」的逐条结论与依据。

## 基线状态（编制时实测 2026-09-16）

| 条目 | 现状 | 备注 |
|---|---|---|
| `AstroCS.wiki/` | 空目录，被 `.gitignore:122` 忽略 | 曾是本地手工 wiki clone；现 Wiki 同步改由 WIKI-001 生成，本目录**不再是必需** |
| `CS/` | 空目录 | 无来源记录，疑为误建 |
| `worktrees/` | 空目录 | AGENTS.md §5 禁止在 main 外开 worktree，长期应为空；空目录本身无害但属散落 |
| `engineering/` | 空目录 | **在 ENGINEERING_SPEC §7 白名单内**，但从未使用 |
| `logs/` | 5 个日志文件（6K） | 在 §7 内（标注 gitignore），但内含旧构建日志 `10_configure_new.log` 等运行产物 |
| `evidence/` | 2799 文件 / 19.7M（624 tracked） | **不在 §7 白名单**，却是大量历史验收证据的落点 |
| `run/` | 97G / 63 万文件 | 归 ROOT-003，本任务不处置 |

## 权威依据

- ENGINEERING_SPEC.md §7（仓库根固定条目清单；「任何新产物必须落位到对应目录，禁止散落根目录」；「确需新增根目录条目，先登记并获得负责人确认」）
- AGENTS.md §5（不在 main 外开分支/worktree/额外 clone）、§6（目录落位速查：run/ 临时产物、reports/ 正式报告）
- ASTROCS_DESIGN.md §7.1（目标架构下哪些目录是交付的一部分）

## 改动范围（文件域）

### 允许改
- 删除空目录：`CS/`、`worktrees/`、`AstroCS.wiki/`（**WIKI-001 落地后**才删 Wiki 空目录，避免抢文件域）
- `engineering/` 与 `logs/`：先给处置建议（§7 内条目需谨慎），空目录 `engineering/` 可删；`logs/` 的旧日志移入 `run/` 或删除
- 更新 `工程控制/PROJECT-GOVERNANCE-01/ROOT_LEDGER.md`：把上述条目从「待裁决」改为「已处置 + 依据」

### 禁止改
- `evidence/` 的任何内容（本任务只**给出登记建议**：要么纳入 §7 白名单，要么把证据归档到 `reports/`；决定权在负责人）
- `run/**`（ROOT-003）、`问题扫描/**`（已独立移交）、`工程控制/**`（控制包本体）
- `VERSION`、`CHANGELOG.md`、`FATDUCK_ACCESS.md`、`VISUAL_CHECK_README.md`、`REVIEW.md`、`HANDOVER.md` 等头部文档（归 GOV-001/DOC-001）

## 步骤

1. 复核每个条目的 tracked 状态与引用情况（`git ls-files` + 全仓引用扫描）；
2. 空目录：删除（`git` 不跟踪空目录，故删除不影响 tracked 集合；用前后 `git ls-tree -r HEAD | wc -l` 对照证明）；
3. `logs/`：区分「§7 认可的日志落点」与「旧运行产物」；后者删除并记账；
4. `evidence/`：产出**登记建议书**（纳入 §7 / 归档 / 保留现状三选一，各给理由与影响），不做删除；
5. 更新 ROOT_LEDGER，并把结论回写 `GAP_AUDIT.md` 的 GAP-021/022（条目级闭合）。

## 验收门（前台独立复跑）

- [ ] 每个空目录的处置都有「有无 tracked 内容 + 有无引用」两项证据
- [ ] `git ls-tree -r HEAD | wc -l` 前后一致（不误删 tracked）
- [ ] `evidence/` 的登记建议书含 tracked 文件数、被引用处清单、三种方案的取舍理由
- [ ] ROOT_LEDGER 中不再有「空目录未处置」类待裁决项
- [ ] 仓库根条目数如实记录（前 → 后）

## 禁止

- 不得删除任何 tracked 文件；
- 不得删除 `evidence/`（只给建议）；
- 不得在 WIKI-001 落地前删除 `AstroCS.wiki/`。

## 交付物

1. 删除记录（每条：路径、类型、大小、依据、指纹）；
2. `evidence/` 登记建议书；
3. ROOT_LEDGER 与 GAP_AUDIT 更新。