# REBASE · 旧 bug 清单按最新权威重定版的口径与字段（ROOT-004）

> ⚠️ **本任务已移交独立执行**：ROOT-004 由负责人决定改派给另一个 agent 独立完成。
> **接续入口 = `reports/PROJECT-GOVERNANCE-01/root-scan/CONTINUATION.md`**（含资产清单、分片状态、复跑命令、未完事项与陷阱、三份根文档处置结论）。
> 交接时点：26 个分片已派出、5 片已落盘（130/785 条）；`REBASE_TABLE.md` / `SUMMARY.md` / `P0_RECHECK.md` **尚未生成**，由接手完成。本文件以下口径**继续有效、不得改写**。

> **抬头（对全目录生效）**：`问题扫描/` 是**上一轮（旧基线）**的全项目 bug 清单，其判据锚在旧宪章、旧工程约束、旧控制包路径与旧文档体系上。
> 自本文件起，**现行结论一律以 `问题扫描/REBASE_TABLE.md` 为准**；`findings/**`、`INDEX.md`、`SUMMARY.md`、`_merge/**`、`账本/**` 的原始记录**保留为历史证据**，不再单独作为整改依据。
> 本目录**零删除**：原始 finding 文件一个未删，仅追加了抬头说明（`00_README.md`、`INDEX.md`）。

- 基线：口径制定时 `HEAD = main = ecf6ad6f`（origin/main = `f96dff61`，同一线，未推送；三 SHA 差异见 GAP_AUDIT §5.1 U-08）。**接手轮注**：合并轮开工三 SHA 收敛为 `2c328348304d033aecfa81faf79d1c6cd802b30a`，收口时 HEAD 由并行执行线推进至 `d414c3e0`+；逐条证据一律按各自取证时刻的工作树留痕（详见 SUMMARY.md 与 REBASE_TABLE.md 抬头）。
- 口径制定时点：本轮（ROOT-004）开工时按当前树实测；所有数字口径写在 §2 与 §8，可复跑。
- 本文件只定义**怎么判、写什么字段、怎么证明覆盖**；逐条结论见 `REBASE_TABLE.md`，汇总结论见 `reports/PROJECT-GOVERNANCE-01/root-scan/SUMMARY.md`。

---

## 1. 最新权威是什么（订正的唯一标尺）

`ASTROCS_DESIGN.md §0` 声明权威链：**ASTROCS_DESIGN > AGENTS.md > ENGINEERING_SPEC > CONTROL_PACK_SPEC > docs/ci > docs/plugins（23 篇）**；
并另立 `docs/science`（公式权威）、`docs/algorithms`（推导权威）、`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置分离）。

因此以下**不在权威链上**，只能作为"原判据"出现在第 4 列，不能作为第 5 列：
`ASTROCS_PROJECT_CONSTITUTION.md`（旧宪章，仍 tracked 但已被 ASTROCS_DESIGN 取代）、`AstroCS_ENGINEERING_CONSTRAINTS.md`（自述 ARCHIVED_NON_NORMATIVE）、
`docs/standards/STANDARDS_REGISTRY.md`、`docs/contracts/*`、`docs/traceability/*`、`docs/quality/*`、`docs/modules/registry/*`、已删的 `工程控制/AstroCS_*` 路径。

用于逐条订正的权威条款（本轮实读）：ASTROCS_DESIGN §0/§1/§2/§3.3/§3.4/§4.2/§4.3/§5.3/§6.1-§6.3/§7.1-§7.3/§8/§9/§10.1/§10.2/§11.1-§11.3/§12/附录 A/附录 B；
ENGINEERING_SPEC §1-§12；CONTROL_PACK_SPEC §3/§4/§6/§7；docs/ci/01_CHECKS.md §1-§5；docs/plugins/00_INDEX.md §1-§5（23 模块归属与 8 节模板）；
docs/design/UNIFIED_MODEL.md §1-§3；docs/science/**、docs/algorithms/**（冻结公式与推导）。

**旧→新条款映射速查表**见 `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/SHARD_BRIEF.md §2`（分片与本表共用同一张表，保证判定同源）。

---

## 2. 原始条目分母：定义、计数与覆盖度证明

**定义（唯一口径）**：一条 finding = `问题扫描/账本/FIX_LEDGER.csv` 的**一行**（列 `id` 唯一）。该账本是上一轮流水线自己生成的机械台账（生成器 `问题扫描/_tools/gen_fix_ledger.py`），
含 `id / priority / category / producer / title / position / evidence / clause / related / fix_state / verified_state` 等 25 列。

**当前实测（本轮复跑，命令见 §8）**：

| 口径 | 数值 |
|---|---|
| `findings/**/*.md` 文件数（含 9 份类别 README） | **318**（原始定性稿件） |
| 其中真正的 finding 文档（排除 README） | **309** |
| `##`/`###`/`####` 标题行总数 | **821** |
| `FIX_LEDGER.csv` 条目（= 原始条目数 **N**） | **785** |
| 账本 ID 在 findings 树中有锚标题的比例 | **785 / 785 = 100%**（0 条无锚） |
| 无对应账本 ID 的说明性标题（负结果清单/轴级摘要/施工顺序等，**不计入条目**） | **34** |
| 优先级分布（账本） | P0 **93** / P1 **449** / P2 **240** / P? **3** |
| 类别分布（账本） | G_GOV_GATE 181、A_SCI_DEF 127、C_DOC_CODE_GAP 106、F_TEST_GAP 78、C_ALG_IMPL 69、D_COMMENT 67、E_TRACE_BREAK 51、I_DOC_HYGIENE 44、H_NUMERIC 29、B_STD_MISMATCH 25、J_FS_PUBLISH 8 |

> **与任务卡/INDEX 自述数字的差异（必须记录，不得掩盖）**：ROOT-004 任务卡与 `问题扫描/INDEX.md v4` 自述「224 份定稿件 / 520 条标题；P0 81 / P1 306 / P2 133」。
> 该数字是 **INDEX v4 时点**的快照，之后流水线又追加了 L28b/L28c/L28d/L28e、E1–E4、V1–V21、W1–W8、SA、FD、RC 等批次（`问题扫描/SUMMARY.md §六-D` 自述「274 份档案 / 630 条目级判定 / 账本 655 行」，也已过期）。
> **本轮分母一律取 785**（= 账本行数 = 现行机械可复跑的唯一事实源），并在 `REBASE_TABLE.md` 逐条覆盖到 785 行、差集为空。

**覆盖度证明（三条同时成立才算"覆盖完整"）**：
1. `REBASE_TABLE.md` 数据行数 == 785；
2. `REBASE_TABLE` 的 ID 集合 == `FIX_LEDGER.csv` 的 id 集合（双向差集为空）；
3. 每条 finding 文件都被至少一行引用（`file` 列来自 ID→文件映射 `_gen/idmap.csv`；785 条全部有锚文件，0 条落空）。

---

## 3. 结论四态（唯一词表）

| 结论 | 语义 | 必要条件 |
|---|---|---|
| `OPEN` | 对照**最新权威条款**仍成立：当前树仍违反/仍缺失该条款要求 | 第 5 列给出最新权威条款 + 第 6 列给出本轮真跑的命令与输出 |
| `RESOLVED` | 不再成立：有可复跑证据证明已修复、或缺陷对象已不存在 | 第 6 列必须证明「不再成立」；第 10 列写「什么证据使其不再成立」 |
| `VOID` | 原判据基于旧设计/旧路径，最新权威下**不构成偏差** | 第 4 列写旧文档+节号或旧路径；第 10 列写**最新替代**；禁止只写「过时」 |
| `UNVERIFIABLE` | 证据不足以判定 | 第 10 列写**缺什么**（缺构建/缺 Windows 节点/缺真实数据/缺外网原文/缺负责人裁决/缺 MOD-001 映射表…） |

**三条禁令**：① 禁止把「问题仍在」判成 `RESOLVED`；② 禁止把「设计已变、原判据失效」判成 `OPEN`；③ `VOID` 必须写明旧依据与最新替代。
**优先级不改判**：第 3 列一律沿用账本原优先级（`P?` 照写）；本轮只重定"是否仍是问题"，不重排优先级。

---

## 4. 字段定义（`REBASE_TABLE.md` 一行一条，10 列）

```
ID | 原类别 | 原优先级 | 旧判据(文档+节号/路径) | 最新权威条款 | 当前证据(命令+输出) | 结论 | 归属 | GAP关系 | 备注
```

- **ID**：逐字沿用账本 `id`（不重命名、不合并、不删除）；重复项保留原 ID，另在 GAP关系列标注。
- **原类别 / 原优先级**：逐字沿用账本 `category` / `priority`。
- **旧判据**：原 finding 实际引用的旧文档+节号（如「旧宪章 §17.10」）或旧路径（如「工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915」）；原判据若本身就是现行权威，写「无（原判据即现行权威）」。
- **最新权威条款**：`<文档名> §<节号>：<原文要点 ≤2 句>`。
- **当前证据**：`命令：<可复跑单行命令>；输出：<≤3 行逐字输出>`（本轮真跑）。
- **结论**：四态之一。
- **归属**：`TASK_LIST.md` 30 个任务之一的 ID，或 `NEXT-PACK:<候选ID>`（映射不上的新任务候选）。
- **GAP关系**：`无` 或 `与 GAP-0xx 重复`（与 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` GAP-001..023 + U-01..U-08 对照；**重复也保留原 ID，不合并删除**）。
- **备注**：一句话；`VOID`/`UNVERIFIABLE` 必填理由，`RESOLVED` 必填使其不成立的证据。

---

## 5. 判定纪律

1. finding 自述「仍成立」**不是证据**；必须本轮在当前树重新取证（`read`/`grep`/`git ls-files`/解释器解析）。
2. 行号按当前树重定位；finding 行号漂移本身可作为 `E_TRACE_BREAK` 类 finding 的证据，但不改变结论。
3. 集合类判据（「全仓零命中」「N 处」）必须重算并写明数字口径；不得抄 finding 的数字。
4. 结构化文件用解释器解析（`read` 对 >2000 字符单行会截断）。
5. P0 条目**逐条**由主控亲自复核（100%，不抽样），复核记录见 `reports/PROJECT-GOVERNANCE-01/root-scan/P0_RECHECK.md`。
6. 无法判定一律 `UNVERIFIABLE` 并写明缺什么；**不为凑数强判**。
7. 本轮**零修复**：不改任何代码/文档/测试/CI；不 commit/push/branch/stash/reset/clean/checkout。

---

## 6. 分片与合并（机械可复现）

- 分片：按「类别 × 优先级」切分并平衡到 ≤46 条/片，共 **26 片**，覆盖 785/785，无重复无遗漏；分配表 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/<分片名>.tsv`，索引 `_gen/shard_index.json`。
- 字段与判定统一由 `_tools/SHARD_BRIEF.md` 下发（同一套 10 列 + 同一张旧→新映射表 + 同一套四态定义）。
- 合并：每片交 `shards/<分片名>.psv`（10 列 PSV），主控用 `_tools/merge_rebase.py` 合并为 `REBASE_TABLE.md`；**任何不符合 10 列/结论不在四态内的行都会被脚本拒绝并要求重做**。
- P0（93 条）另由主控逐条复核，产出独立复核文件，不依赖分片自述。

---

## 7. 交付物

| 文件 | 内容 |
|---|---|
| `问题扫描/REBASE.md` | 本文件：口径、四态词表、字段定义、覆盖度证明、复跑命令 |
| `问题扫描/REBASE_TABLE.md` | 逐条总表（785 行 × 10 列） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/SUMMARY.md` | 结论分组统计 + OPEN 的 P0/P1/P2 分布 + 与 30 任务映射计数 + 下一轮候选任务清单 + 三份根文档处置建议 |
| `reports/PROJECT-GOVERNANCE-01/root-scan/P0_RECHECK.md` | 93 条 P0 的主控逐条复核记录 |
| `reports/PROJECT-GOVERNANCE-01/root-scan/shards/**` | 26 片原始判定与分配表 |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_gen/**` | ID 映射、分片索引、统计 JSON（机器可复跑） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/**` | 复跑脚本与共用简报 |

---

## 8. 复跑命令（覆盖度与统计，全部只读）

```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-004/logs reports/PROJECT-GOVERNANCE-01/root-scan
# 原始 finding 文件计数（前后对比，证明零删除）
find 问题扫描 -type f | wc -l
find 问题扫描/findings -type f -name '*.md' | wc -l
# 账本条目数（原始条目分母 N=785）
python3 -c "import csv;print(len(list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))))"
# ID→文件映射与覆盖度（生成 _gen/idmap.csv；打印 785/785 与 34 条说明性标题）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_idmap.py
# 分片分配（26 片，覆盖 785/785）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_shards.py
# 合并与统计（产出 REBASE_TABLE.md / stats.json；双向差集必须为空）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/merge_rebase.py
```
