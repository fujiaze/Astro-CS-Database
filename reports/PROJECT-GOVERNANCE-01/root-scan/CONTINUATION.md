# ROOT-004 接续执行包（CONTINUATION）——给完全不了解上下文的下一位 Agent

> **移交说明（2026-09-16）**：ROOT-004「旧 bug 清单按最新权威订正」已由负责人决定**独立出去交给另一个 agent 执行**。
> 本文件是**唯一接续入口**：前一位执行者（下称"前任"）已完成**口径与基础设施**、**93 条 P0 的逐条复核**、**三份根文档核对**，并派出 26 个分片但**尚未全部回收**。
> 后一位执行者（下称"接手"）**不需要重读全部权威文档**，按本文件 §3/§4/§8 即可无缝接续；但**必须**读 §9 的陷阱与 §10 的硬规则。
> 交接时口径：`问题扫描/REBASE.md` 为口径权威（已加移交抬头）；`问题扫描/REBASE_TABLE.md` **尚未生成**（需 26 片齐后合并）。

---

## 1. 目标与最终判据

**目标**：把 `问题扫描/`（上一轮针对**旧基线**的全项目 bug 清单）逐条对照**最新权威设计**与**当前仓库实际状态**，给出订正后的四态结论，形成下一轮工程包的输入；并处置三份根目录存疑文档。

**原始条目分母 = 785**（= `问题扫描/账本/FIX_LEDGER.csv` 的行数）。
- ⚠️ **不要用**任务卡/INDEX 自述的「224 份定稿件 / 520 条标题 / P0 81 / P1 306 / P2 133」——那是 `INDEX.md v4` 时点快照，已被后续 L28b–e、E1–E4、V1–V21、W1–W8、SA、FD、RC 等批次追平并超过。
- 当前实测：`findings/**/*.md` = **318** 份（含 9 份类别 README，实际 finding 文档 309）；标题行 **821**；其中 **785 个账本 ID 全部有锚标题（785/785）**，另 **34 条**为负结果清单/轴级摘要/施工顺序等**说明性小节，不计入条目**。
- 优先级分布：**P0 93 / P1 449 / P2 240 / P? 3**。
- 类别分布：G_GOV_GATE 181、A_SCI_DEF 127、C_DOC_CODE_GAP 106、F_TEST_GAP 78、C_ALG_IMPL 69、D_COMMENT 67、E_TRACE_BREAK 51、I_DOC_HYGIENE 44、H_NUMERIC 29、B_STD_MISMATCH 25、J_FS_PUBLISH 8。

**覆盖度证明（三条同时成立才算完成）**：
1. `REBASE_TABLE.md` 数据行数 == **785**；
2. 其 ID 集合与 `FIX_LEDGER.csv` 的 `id` 集合**双向差集为空**；
3. 每条 finding 文件都被至少一行引用（映射表 `_gen/idmap.csv`；785 条全部有锚文件，0 条落空）。

---

## 2. 结论四态（唯一词表，不得自创第五态）

| 结论 | 语义 | 必要条件 |
|---|---|---|
| `OPEN` | 对照**最新权威条款**仍成立：当前树仍违反/仍缺失 | 第 5 列最新权威条款 + 第 6 列本轮真跑命令与输出 |
| `RESOLVED` | 不再成立：有可复跑证据证明已修复、或缺陷对象已不存在 | 第 6 列证明"不再成立"；第 10 列写"什么证据使其不再成立" |
| `VOID` | 原判据基于旧设计/旧路径，最新权威下**不构成偏差** | 第 4 列旧文档+节号或旧路径；第 10 列写**最新替代**；**禁止只写"过时"** |
| `UNVERIFIABLE` | 证据不足 | 第 10 列写**缺什么**（缺构建/缺 Windows 节点/缺真实数据/缺外网原文/缺负责人裁决/缺 MOD-001 映射表…） |

**三条禁令**：① 禁止把「问题仍在」判成 `RESOLVED`；② 禁止把「设计已变、原判据失效」判成 `OPEN`；③ `VOID` 必须写旧依据与最新替代。
**优先级不改判**：第 3 列沿用账本原优先级（`P?` 照写）；本轮只重定"是否仍是问题"。

---

## 3. 字段表（10 列 PSV，逐列含义；分片产物 `shards/<分片名>.psv`）

首行表头逐字为：
```
ID|原类别|原优先级|旧判据(文档+节号/路径)|最新权威条款|当前证据(命令+输出)|结论|归属|GAP关系|备注
```

| 列 | 含义与要求 |
|---|---|
| 1 `ID` | 逐字沿用账本 `id`（如 `M2a-A-1`、`V11-N-04`）；**不重命名、不合并、不删除** |
| 2 `原类别` | 账本 `category` 逐字 |
| 3 `原优先级` | 账本 `priority` 逐字（`P?` 照写） |
| 4 `旧判据` | 原 finding 实际引用的**旧文档+节号**（如"旧宪章 §17.10"）或**旧路径**（如已删的 `工程控制/AstroCS_*`）；原判据本身就是现行权威时写 `无（原判据即现行权威）` |
| 5 `最新权威条款` | `<文档名> §<节号>：<原文要点≤2 句>`；写前用 `read`/`grep` 核对节号与原文确实存在 |
| 6 `当前证据` | `命令：<单行命令>；输出：<≤3 行逐字输出>`，必须是执行者**本轮真跑过**的 |
| 7 `结论` | `OPEN`/`RESOLVED`/`VOID`/`UNVERIFIABLE` 之一 |
| 8 `归属` | `TASK_LIST.md` 30 任务之一的 ID，或 `NEXT-PACK:<候选ID>` |
| 9 `GAP关系` | `无` 或 `与 GAP-0xx 重复`（对照 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 的 GAP-001..023 + U-01..U-08；**重复也保留原 ID，不合并删除**） |
| 10 `备注` | 一句话；`VOID`/`UNVERIFIABLE` 必填理由，`RESOLVED` 必填使其不成立的证据 |

列内**禁 `|`、禁换行**（用 `；` 与空格代替）。合并脚本会**拒绝**列数≠10 或结论不在四态内的行。

> ⚠️ **已落盘片的两个既定变通（合并脚本必须容忍，不得"修回去"）**：
> 1. **`¦` 替代符**：至少 `B_STD_MISMATCH_ALL` 在**第 6 列**把逐字输出里的 `|` 写成 `¦`（12 行受影响，其日志含替换对照，且自证 `evidence_exactly_reproducible=True`）。合并时**按原样保留 `¦`**（不要回改成 `|`，否则该行列数会立刻爆掉）。
> 2. **命令内规避写法**：另有片用 `chr(124)`/`awk gsub` 在命令里规避 `|`，命令与输出仍逐字对应 ⇒ 合并脚本不要对第 6 列做"是否含管道/是否像命令"的格式推断。

---

## 4. 权威链与「旧→新条款映射表」

- 权威链（`ASTROCS_DESIGN.md §0`）：**ASTROCS_DESIGN > AGENTS.md > ENGINEERING_SPEC > CONTROL_PACK_SPEC > docs/ci > docs/plugins（23 篇）**；另立 `docs/science`（公式权威）、`docs/algorithms`（推导权威）、`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置分离）。
- **不在权威链上、只能出现在第 4 列**：`ASTROCS_PROJECT_CONSTITUTION.md`（旧宪章）、`AstroCS_ENGINEERING_CONSTRAINTS.md`（ARCHIVED_NON_NORMATIVE）、`docs/standards/STANDARDS_REGISTRY.md`、`docs/contracts/*`、`docs/traceability/*`、`docs/quality/*`、`docs/modules/registry/*`、已删的 `工程控制/AstroCS_*` 路径。
- **旧→新条款映射速查表**：`reports/PROJECT-GOVERNANCE-01/root-scan/_tools/SHARD_BRIEF.md §2`（旧宪章 §1.1→DESIGN §0、§3.2→DESIGN §1.2、§4.1→UNIFIED_MODEL §2、§6.3→UNIFIED_MODEL §2 + DESIGN §4.3、§8.1→DESIGN §6.1/§6.2/§10.1、§10.4→DESIGN §8 + ENG_SPEC §10、§12.2→ENG_SPEC §2、§12.3-4→DESIGN §11.1 + ENG_SPEC §5.1、§13→DESIGN §11.1 + ENG_SPEC §5.1、§14→ENG_SPEC §7、§15.3/§15.4→DESIGN §11.2/§11.3、§16→DESIGN §12 + ENG_SPEC §7、§17.1/§17.10→DESIGN §12、`AstroCS_ENGINEERING_CONSTRAINTS`→无对应即 VOID、CI 检查项→docs/ci/01_CHECKS §2–§5 等）。
- `SHARD_BRIEF.md` 还含：30 任务一览与映射速查、输出格式、硬纪律。**接手必须复用同一张表**，否则判定不同源、无法机械合并。

---

## 5. 现有资产清单（逐路径 + 用途）

### 5.1 口径与产物
| 路径 | 用途 |
|---|---|
| `问题扫描/REBASE.md` | **口径权威**：四态词表、10 列字段定义、分母与覆盖度证明、复跑命令、判定纪律（顶部已加移交抬头） |
| `问题扫描/REBASE_TABLE.md` | ❌ **尚未生成**（接手产物：26 片齐后由 `merge_rebase.py` 合并） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/SUMMARY.md` | ❌ **尚未生成**（接手产物：结论分组统计 + OPEN 的 P0/P1/P2 分布 + 与 30 任务映射计数 + 下一轮候选任务清单 + 三份根文档处置建议） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/P0_RECHECK.md` | ❌ 尚未生成（前任已有全部素材：见 §5.3；接手可直接落盘） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/CONTINUATION.md` | 本文件 |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/SHARD_BRIEF.md` | 分片共用简报（口径 + 旧→新映射 + 字段 + 30 任务 + 纪律）；**接手与任何新分片都必须先读** |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_idmap.py` | 生成 ID→文件映射 `_gen/idmap.csv` + `_gen/idmap_headings.json`（含 785/785 覆盖与 34 条说明性标题） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_shards.py` | 生成 26 个分片分配表 `shards/_assign/*.tsv` + `_gen/shard_index.json` |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/extract_p0.py` | 抽取 93 条 P0 正文到 `_gen/p0_sections/<ID>.md` + `_gen/p0_digest.json` |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/p0_tight2.py` / `p0_digest_print.py` | 打印 P0 精简判读稿（分批复核用） |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/check_p0_anchors.py` / `check_p0_anchors2.py` | P0 锚点机械核验（basename 解析 + 行漂移分档 EXACT/NEAR/DRIFT/ABSENT），产出 `_gen/p0_anchor_v2.json` |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/check_p0_quotes.py` | P0 **引文存活性**核验（原判据引文是否仍逐字存在于当前文件），产出 `_gen/p0_quote_liveness.json` |
| `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/merge_rebase.py` | ❌ **尚未编写**（接手需写：合并 26 片 PSV → `REBASE_TABLE.md` + `_gen/stats.json`；须校验列数=10、结论∈四态、ID 集合双向差集为空、行数=785） |

### 5.2 机器可复跑数据（`reports/PROJECT-GOVERNANCE-01/root-scan/_gen/`）
| 文件 | 内容 |
|---|---|
| `idmap.csv` | 785 行：`id/category/priority/producer/title/file/line/n_headings/files_all/position/evidence/clause/impact/suggested_disposition/related` |
| `idmap_headings.json` | ID→标题锚（含多锚）与 34 条非条目标题清单 |
| `shard_index.json` | 26 片索引：分片名/条目数/首末 ID/类别/优先级 |
| `p0_digest.json` + `p0_sections/*.md` | 93 条 P0 的正文切段与索引（复核证据） |
| `p0_anchor_v2.json` | 93 条 P0 的 1283 个锚点分档结果（EXACT 105 / NEAR 65 / DRIFT 211 / ABSENT 35 / NO-LINE 750 / NORESOLVE 117） |
| `p0_quote_liveness.json` | 93 条 P0 的 125 条引文存活性（**ALIVE 106 / GONE 19**；16 条含 GONE 引文者已由前任逐条复核，见 §5.3） |

### 5.3 前任已完成但**尚未汇总**的成果（接手直接可用，勿重做）
| 路径 | 内容 |
|---|---|
| `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_verify_A2.out` | 前任**亲自跑**的 P0 复核命令批 A（29 条命令：锚点/引文/注册表行/门实现） |
| `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_verify_B.out` | 前任亲自跑的 P0 复核命令批 B（51 条命令，覆盖剩余 P0） |
| `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_anchors_v2.log` / `p0_quotes.log` / `p0_extract.log` / `idmap.log` / `shards.log` | 机械核验与生成日志 |
| `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/scan-files-before.txt` | **零删除证据基线**：`find 问题扫描 -type f | wc -l` = **1095**（收尾需再跑一次并对比） |
| 三份根文档处置（见 §11） | CHANGELOG.md / VISUAL_CHECK_README.md / FATDUCK_ACCESS.md 的只读核对结论与建议 |

### 5.4 分片分配表
`reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/*.tsv`：**26 个文件、合计 811 行 = 785 条目 + 26 个表头**（⚠️ 别把 811 当条目数）。
列：`ID / 原类别 / 原优先级 / producer / 文件 / 行 / 标题 / 旧位置字段 / 旧证据字段 / 旧条款字段 / fix_state / verified_state`。

---

## 6. 分片状态表（交接快照；接手请重跑 §8 第 5 条刷新）

> 快照规则：`shards/<分片>.psv` 存在且行数 = 条目数 ⇒ DONE。

| 分片 | 条目 | 状态 | 产物 |
|---|---|---|---|
| `A_SCI_DEF_P0` | 15 | 见下（快照） | `shards/A_SCI_DEF_P0.psv` |
| `A_SCI_DEF_P1a` | 38 | 见下（快照） | `shards/A_SCI_DEF_P1a.psv` |
| `A_SCI_DEF_P1b` | 38 | 见下（快照） | `shards/A_SCI_DEF_P1b.psv` |
| `A_SCI_DEF_P2` | 36 | 见下（快照） | `shards/A_SCI_DEF_P2.psv` |
| `B_STD_MISMATCH_ALL` | 25 | 见下（快照） | `shards/B_STD_MISMATCH_ALL.psv` |
| `C_ALG_IMPL_a` | 35 | 见下（快照） | `shards/C_ALG_IMPL_a.psv` |
| `C_ALG_IMPL_b` | 34 | 见下（快照） | `shards/C_ALG_IMPL_b.psv` |
| `C_DOC_CODE_GAP_P0` | 18 | 见下（快照） | `shards/C_DOC_CODE_GAP_P0.psv` |
| `C_DOC_CODE_GAP_P1a` | 32 | 见下（快照） | `shards/C_DOC_CODE_GAP_P1a.psv` |
| `C_DOC_CODE_GAP_P1b` | 32 | 见下（快照） | `shards/C_DOC_CODE_GAP_P1b.psv` |
| `C_DOC_CODE_GAP_P2` | 24 | 见下（快照） | `shards/C_DOC_CODE_GAP_P2.psv` |
| `D_COMMENT_P1` | 32 | 见下（快照） | `shards/D_COMMENT_P1.psv` |
| `D_COMMENT_P2` | 35 | 见下（快照） | `shards/D_COMMENT_P2.psv` |
| `E_TRACE_BREAK_P0P1` | 32 | 见下（快照） | `shards/E_TRACE_BREAK_P0P1.psv` |
| `E_TRACE_P2_J_FS` | 27 | 见下（快照） | `shards/E_TRACE_P2_J_FS.psv` |
| `F_TEST_GAP_P0` | 15 | 见下（快照） | `shards/F_TEST_GAP_P0.psv` |
| `F_TEST_GAP_P1` | 46 | 见下（快照） | `shards/F_TEST_GAP_P1.psv` |
| `F_TEST_GAP_P2` | 17 | 见下（快照） | `shards/F_TEST_GAP_P2.psv` |
| `G_GOV_GATE_P0` | 28 | 见下（快照） | `shards/G_GOV_GATE_P0.psv` |
| `G_GOV_GATE_P1a` | 31 | 见下（快照） | `shards/G_GOV_GATE_P1a.psv` |
| `G_GOV_GATE_P1b` | 31 | 见下（快照） | `shards/G_GOV_GATE_P1b.psv` |
| `G_GOV_GATE_P1c` | 31 | 见下（快照） | `shards/G_GOV_GATE_P1c.psv` |
| `G_GOV_GATE_P1d` | 31 | 见下（快照） | `shards/G_GOV_GATE_P1d.psv` |
| `G_GOV_GATE_P2` | 29 | 见下（快照） | `shards/G_GOV_GATE_P2.psv` |
| `H_NUMERIC_ALL` | 29 | 见下（快照） | `shards/H_NUMERIC_ALL.psv` |
| `I_DOC_HYGIENE_ALL` | 44 | 见下（快照） | `shards/I_DOC_HYGIENE_ALL.psv` |

**快照统计（首次交接时点）**：DONE **5 / 26**，已落盘 **130 / 785** 条。
**末次刷新见 §13.1**（DONE 9 / 26，落盘 222 / 785，其中 1 片格式不合格）。
❗**已在跑的 26 个分片 subagent 仍在工作**（后台 agent，产物直接写盘，不需要也不允许重派）。接手请等待它们自然收尾，再刷新本表；**长时间无产出者登记「未完成，交下一任」，不要重派**。

---

## 7. 结论回收后的合并与汇总（接手主路径）

1. 等待/收集 26 片 `shards/*.psv`；
2. 写并跑 `_tools/merge_rebase.py`：校验 + 合并 → `问题扫描/REBASE_TABLE.md` + `_gen/stats.json`；
3. 写 `reports/PROJECT-GOVERNANCE-01/root-scan/SUMMARY.md`：四态统计、OPEN 的 P0/P1/P2 分布、与 30 任务映射计数、映射不上的「下一轮工程包候选任务」（每条含建议文件域 + 验收门）、三份根文档处置建议；
4. 用 §5.3 的 P0 素材写 `P0_RECHECK.md`（93 条逐条：权威条款 + 当前证据 + 结论 + 与分片判定是否一致）；
5. 收尾验收：§8 全部命令复跑 + `git status --porcelain=v1` 核对「`问题扫描/` 与 `reports/PROJECT-GOVERNANCE-01/root-scan/` 之外无本任务改动」。

---

## 8. 复跑命令（**按此顺序**，全部只读；均需 `cd "/workspace/Astro CS Database"`）

```bash
cd "/workspace/Astro CS Database"
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-004/logs reports/PROJECT-GOVERNANCE-01/root-scan

# 1) 零删除证据（收尾必须与 scan-files-before.txt 对比；基线 1095）
find 问题扫描 -type f | wc -l
find 问题扫描/findings -type f -name '*.md' | wc -l

# 2) 原始条目分母（必须 = 785）
python3 -c "import csv;print(len(list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))))"

# 3) ID→文件映射与覆盖度（打印 785/785 与 34 条说明性标题）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_idmap.py

# 4) 分片分配（26 片，覆盖 785/785，无重无漏）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/make_shards.py

# 5) 分片状态刷新（存在且行数=条目数 ⇒ DONE）
for f in reports/PROJECT-GOVERNANCE-01/root-scan/shards/*.psv; do echo "$(basename $f .psv) $(( $(wc -l < "$f") - 1 ))"; done
awk -F'|' 'FNR>1{print FILENAME": "$7}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/*.psv | sort | uniq -c   # 结论分布
awk -F'|' 'FNR>1 && NF!=10 {print "BADCOLS "FILENAME" line "FNR}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/*.psv    # 列数校验（应无输出）

# 6) P0 机械复核（前任已跑；接手复跑即可复现）
timeout 900 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/check_p0_anchors2.py 0 93 | tail -5
timeout 900 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/check_p0_quotes.py 0 93 | head -3
timeout 900 bash run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_verify_A2.sh | tail -40
timeout 900 bash run/PROJECT-GOVERNANCE-01/ROOT-004/logs/p0_verify_B.sh  | tail -50

# 7) 合并与统计（接手编写 merge_rebase.py 后运行；双向差集必须为空、行数必须 785）
timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/merge_rebase.py

# 8) 越界核对（本任务只允许改 问题扫描/**、reports/PROJECT-GOVERNANCE-01/root-scan/**、run/PROJECT-GOVERNANCE-01/ROOT-004/**）
git status --porcelain=v1 | grep -v -E "问题扫描/|reports/PROJECT-GOVERNANCE-01/root-scan/|run/PROJECT-GOVERNANCE-01/ROOT-004/" | head -40
```

---

## 9. 未完事项与陷阱（**接手必读**）

### 9.1 未完事项
1. **26 片中 21 片未回收**（截至快照）；`REBASE_TABLE.md`、`SUMMARY.md`、`P0_RECHECK.md`、`merge_rebase.py` 四项尚未产出。
2. **`问题扫描/00_README.md` 与 `INDEX.md` 的 REBASE 抬头**尚未加（ROOT-004 步骤 6 要求：说明「原清单基于旧基线、现行结论以 REBASE_TABLE 为准、原始 findings 保留为历史证据」）。
3. **三份根文档的最终登记**：结论见 §11，但需要落到 `SUMMARY.md` 并回给 ROOT-001/GOV-001。
4. 前任**未**完成的自我约束：本轮**不做任何修复**、**不 commit/push**（保持原样交接）。

### 9.2 陷阱（前任踩过或识别到的）
1. **分母陷阱**：任务卡与 `INDEX.md` 的 224/520/P0 81 已过期；**必须用账本 785**（§1）。
2. **811 ≠ 785**：`_assign/*.tsv` 合计 811 行含 26 个表头。
3. **行号普遍漂移**：1283 个 P0 锚点里 **DRIFT 211 / ABSENT 35**，仅 EXACT 105。**行号不能当事实**，一律按 `path::symbol` 或重定位后取证；行号漂移本身可作为 `E_TRACE_BREAK` 的证据。
4. **引文存活性**：125 条 P0 引文中 **19 条 GONE**（16 条 finding）——多数是解析噪声（跨行/改写），**少数是真实变化**（如 `M4-F-01` 的 `add_subdirectory` 断言、`M6b-G-002` 的 `REVIEW_PENDING`、`M3b-F-02` 的 `curve_fit`、`M8-F-002` 的 io_ownership 主程序）。**GONE 不等于 RESOLVED**：必须区分「已修」与「对象已消失/被替换」。
5. **账本已有 fix_state**：`FIX_LEDGER.csv` 的 `fix_state/verified_state/fix_note` 记录了并发修复批（B1–B4）成果：**P0 中 19 条标 FIXED/PARTIAL**（M1a-C-002、M2a-C-1、M2a-H-1、M3-C-001、M3-E-001、M3-C-002(PARTIAL)、M3b-F-01(PARTIAL)、M3b-F-02、M3b-H-01、M4-C-01、M4-C-02、M4-C-03、M5a-G-001、M5a-G-002、M7-C-001(PARTIAL)、M8-F-001、M8-F-002、M8-F-003、M9-H-1、M9-H-2、V2-N-01）。**这些修言必须逐条用当前树复跑验证后才可判 RESOLVED**（前任已跑批 A/B 的一部分）。
6. **注册表节名会被误当锚**：`docs/standards/STANDARDS_REGISTRY.md` 的 `D.hips`/`D.catalog`/`D.cal` 等节名会被路径正则切成 `D.h`/`D.c`（`check_p0_anchors2.py` 的 NORESOLVE 117 大多来自此）；不是缺文件。
7. **同名文件多份**：`README.md`/`module.yaml`/`CMakeLists.txt` 在仓内多处；basename 解析会取最短路径，**引用必须带目录前缀**。
8. **`read` 工具对 >2000 字符单行会截断**：结构化文件（CSV/JSON/YAML）一律用 python 解析。
9. **巨目录**：`run/` 102G/63 万文件、`build/` 6G、`设计大纲/` 101M、`GaiaDR3*`；任何 `grep -r`/`find` 必须排除 `{run,build,out,artifacts,evidence,reports,graph,worktrees,Testing,logs,third_party,设计大纲,GaiaDR3,GaiaDR3SP,BASS DR3}`。
10. **并发线**：另有执行线在做「根目录清洁」（写 `工程控制/PROJECT-GOVERNANCE-01/ROOT_LEDGER.md`、`RETENTION.md`，会移动/删除根条目）。**不要动仓库根条目**，也不要与它抢同一文件域。
11. **`FATDUCK_ACCESS.md` 含凭据**：不得 `read`/`cat`/打印/复制其内容；只登记结论（见 §11.3）。
12. ⚠️ **`NEXT-PACK` 候选 ID 跨片不同源**：分片自拟的候选 ID 已出现 `NP-01..NP-04`、`NP-DC-01`、`NP-DC-02`、`NP-DC-03`、`NP-DC1`、`NP-GAIA-01` 等多种写法（同名不同义/同义不同名并存）。**合并时必须先归并候选清单**（建议：按"文件域 + 验收门"去重后统一编号为 `NP-01..NP-nn`，并在 SUMMARY 里保留原片自拟 ID 的对照列），否则"映射不上的候选任务"会重复计数。
13. ⚠️ **仓库根 `grep -r PATTERN .` 在本工作区不可用**：多名分片 subagent 实测该写法在 60s 内**零输出**（超时/管道缓冲丢结果），据此得出的"全仓 0 命中"结论是假证据（`G_GOV_GATE_P1a` 已自查修正 RADESYS / sampling_semantics / PHASE_OVERVIEW 三处）。**必须**改用显式目录列表（`grep -rn PATTERN docs lib cli tests ci tools contracts`）或 `git grep -n PATTERN -- <paths>`，并加 `timeout`。
14. ⚠️ **判定时点必须写成三元组**：工作树在本轮**同时被多条线修改**（`ci/checks.json` 145→147 且为 M、`ci/root_manifest.json` 新增未跟踪、`docs/contracts/INDEX.yaml`/`.gitignore`/`GAP_AUDIT.md`/`ISA-00*/MEASUREMENTS.csv` 处 M、`设计大纲/**` 大批 D）⇒ 结论行应写「时点 = `<SHA>` + 当时的未提交改动」，**不要**引用"门数/不可达门数/CHK-* 数量"一类静态数字（必须按当时树重算）。
15. ⚠️ **账本 `clause` 列常为空**：相当比例条目（如 `G_GOV_GATE_P1c` 的 27/31）在原账本第 `clause` 列为空 ⇒ 第 4 列按规则写「无（原判据即现行权威，账本 clause 列为空）」，不得编造旧条款。
16. ⚠️ **路径漂移的高频形态（合并时用于重定位，不要当"文件消失"）**：`docs/science/X.md` 实为 `docs/algorithms/X.md`（PHASE2_SAMPLER / PHOTOMETRIC_FIT）；`lib/star_detection/sdet_api.cpp` → `lib/star_detector/src/`；`lib/star_detector/src/star_detector.cpp` → `lib/phase1/stars/star_detector.cpp`；`lib/phase1/photometry/image_corrector.cpp` → `lib/photometric_calib/cpp/src/`；`lib/astro_image_io/src/healpix/aio_healpix_io.h` → `lib/astro_image_io/include/`；`lib/hips/types.h` → `lib/hips/include/astrocs/hips/types.h`；`lib/plate_solve/src/ipv_triangle.cpp` → `lib/plate_solve/cpp/ipv/src/`；`include/` 公共头 96→23（其余在 `lib/*/include`）。
17. ✅ **分配表/ID 映射已被交叉验证为正确 —— 分片对它的"笔误"指控经复核不成立，不要"修"它**：`C_ALG_IMPL_a` 报「`V10-N-07/N-08` 的『文件』列写 `p1/V10-c.md`」。前任实测：`_assign/C_ALG_IMPL_a.tsv` 两行的第 5 列**都是** `问题扫描/findings/C_ALG_IMPL/p2/V10-c.md`（行 3/10），且 `p1/V10-c.md` 在本树**不存在**、`p2/V10-c.md` 存在（6005 B）；`_gen/idmap.csv` 同指 `p2`。
   ⇒ **规则**：若某片声称分配表路径错，先跑 `awk -F'\t' '$1=="<ID>"{print $5,$6}' shards/_assign/<片>.tsv` 与 `ls` 复核，**以实测为准**；分片第 4 列可能带一条不准确的转述（本例 `V10-N-07` 第 4 列写了"分配表写 p1"，属分片侧误读，不影响其结论与第 6 列证据）。
18. ✅ **新增偏差候选（前任已复核实证，接手须收进"下一轮候选任务"）**：`docs/ci/01_CHECKS.md:7` 明文「豁免显式登记 `ci/exemptions.json`，只减不增，需负责人批准」，而 **`ci/exemptions.json` 在树内不存在**（实测 `ls` 0 命中）；实际豁免分散在 `ci/checks.json` 的 `waivable`（7 项）与 `ci/run.py:103` 的硬编码 `EMPTY_OUTPUT_SILENCE_EXEMPT`（2 项）。⇒ 由 `G_GOV_GATE_P2` 提出、前任复核确认；登记为 **`NEXT-PACK:NP-09`**（不属 `GAP-001..031`，勿误标重复）；建议文件域 `ci/**` + `docs/ci/01_CHECKS.md`，验收门 = `test -f ci/exemptions.json` 且每项豁免在注册表可追溯、`ci/run.py` 无硬编码豁免。
19. ⚠️ **`pN` 目录不是优先级权威**：实测 **11 条**条目的所在目录 `pN` 与账本 `priority` 不一致（V/W/V2/V13/V14 轴文件是"按轴成文、混优先级"），例如 `V11-N-02`(P1) 在 `G_GOV_GATE/p0/V11.md`、`V13-N-05/07/08`(P2) 在 `p1/V13-b.md`、`V14-N-07`(P2) 在 `p1/V14-c.md`。**优先级一律取账本第 3 列**（这也是 `REBASE_TABLE` 与分片表的口径）；**不要**用目录名推断优先级，也不要用目录名做"优先级/目录不符"的finding。
19. **`UNVERIFIABLE` 的常见成因**：需 Windows/Fatduck 真机行为、需真实数据端到端、需新建 ASan/UBSan 构建、需外网原文（Paper I §3.3.3、Paper II Table 1、HiPS hips_frame 枚举行、IVOA 响应格式）、需负责人裁决（原 `40_OWNER_DECISIONS.md` 的 A-01…A-44）。**判不动就登记，不要强判。**

### 9.3 ID 定位与条款查无
- **ID 定位：0 条落空**（785/785 都有锚文件与锚标题）。若某片 PSV 少行，先查该片 `shards/<分片>.md` 的覆盖自证，再查 `_assign/<分片>.tsv`。
- **条款查无**：旧宪章条款在 `ASTROCS_PROJECT_CONSTITUTION.md` 仍可查（但**非权威**）；`AstroCS_ENGINEERING_CONSTRAINTS.md` 同；`docs/standards/STANDARDS_REGISTRY.md` 条款在此文件内可查但**非权威链**。若某条 finding 只引这三者且新权威无对应条款 ⇒ 第 4 列写旧依据、结论判 `VOID` 并在第 10 列写最新替代。

---

## 10. 硬规则（继承 ROOT-004 任务卡，接手同样受约束）

1. **零修复**：本轮只做判定与产出清单，不改任何代码/文档/测试/CI。
2. **零删除**：原始 finding 一个都不删；前后 `find 问题扫描 -type f | wc -l` 必须一致（基线 **1095**）。
3. **零 git 写**：不 commit / push / 建分支 / stash / reset / clean / checkout；只读 git 查询允许。
4. **文件域**：只允许写 `问题扫描/**`（订正产物 + 必要的抬头追加）、`reports/PROJECT-GOVERNANCE-01/root-scan/**`、`run/PROJECT-GOVERNANCE-01/ROOT-004/**`；**不得**碰 `lib/** cli/** tests/** contracts/** ci/** tools/** docs/** AGENTS.md ASTROCS_DESIGN.md ENGINEERING_SPEC.md CONTROL_PACK_SPEC.md VERSION 设计大纲/**` 与**仓库根条目**。
5. **`docs/science/**` 与 `docs/algorithms/**` 只读**（公式与推导不得改）。
6. 所有外部命令带 `timeout`，日志落 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/`。
7. 禁止用「环境问题/工具问题」掩盖失败；无法判定登记 `UNVERIFIABLE` 并写明缺什么。
8. 分片 subagent **零 git 写权限**，只允许写自己那一片的 `shards/<分片>.psv` 与 `.md`。

---

## 11. 三份根目录存疑文档（前任已核对，结论待接手写入 SUMMARY.md）

### 11.1 `CHANGELOG.md`（253 行 / 16,478 B；**tracked、未被 ignore**）
- **内容**：AstroCS 版本历史；首节自称"当前产品版本节（`0.11.0-alpha.2`）"并绑定 `VERSION`/`docs/governance/VERSION_NAMESPACES.md`，其后为 `alpha.1` 与 V19R8…V12 历史节；含 V6 控制包（**已删路径** `工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/`）与宪章 §17.12 引用；大量"未通过/未决"如实登记（Linux CI 红、Windows AWAITING、SO-05 利用率不足等）。
- **与最新权威冲突**：① `ASTROCS_DESIGN.md §12`「Alpha 之前程序与代码不包含任何版本信息」——本文以"当前版本节+bound VERSION"形式承载版本信息；② 引用已删控制包路径与旧宪章，属 `GAP-019` 的一处实例（该 GAP 记 337 处引用）。
- **建议**：**保留（登记）**，理由：`ENGINEERING_SPEC.md §7` 正文明确承认「`VERSION/CHANGELOG.md` 仅作内部助记，不进入程序与发布产物」——它是**被规范承认的裁决/授权登记面**（机制⑫要求"裁决必须进登记面"，而 CHANGELOG 是现成合规形态）；同时**必须由 GOV-001/DOC-001 收敛**其"当前版本节"表述以符合 DESIGN §12，并登记为根条目（§7 白名单未列它 → GAP-001）。
- **不建议删除**：会毁掉唯一的裁决登记通道；也不建议归档（历史节仍有追溯价值且需与 `alpha.2` 现状对照）。

### 11.2 `VISUAL_CHECK_README.md`（72 行 / 2,271 B；**tracked、未被 ignore**）
- **内容**：HiPS 浏览器（Qt）1–2 分钟人工视觉验收 runbook：启动命令 `pwsh -File .\launch\start_browser.ps1`、预期画面、Signal/Support 切换、预设视角、鼠标操作、"请重点检查"7 条、回填 `USER_VISUAL_ACCEPTANCE=PASS`。
- **与最新权威冲突（实测硬证据）**：① 启动路径 `launch/` **在当前树不存在**（`ls launch` → 没有那个文件或目录）；② 示例产品 `run/phase2/v9/geometry_truth.hips` **不存在**（`run/` 正在被 ROOT-003 清运，该路径属一次性产物）；③ `ASTROCS_DESIGN.md §1.3` 明确 HiPS Browser 是"未来可视化组件，**不进产品 manifest**"，`§10.1` "Alpha 不含 GUI" —— 它不是产品交付面，却占一个未登记的根条目（GAP-001/021）。
- **建议**：**归档到 `docs/archive/`**（理由：内容是有价值的操作 runbook，但入口脚本与所引 run/ 产品均已不存在，留在根目录既误导又违反 §7 白名单；归档可保住历史操作知识，同时消除根条目与"路径不存在"的悬空引用）。若负责人坚持保留在活动面，则**必须**改指 `docs/browser/HIPS_BROWSER.md` 并同步真实入口脚本路径——这属下一轮工程包候选（建议文件域 `docs/browser/**` + `launch/**`，验收门：文档内每条命令路径 `test -e` 通过）。

### 11.3 `FATDUCK_ACCESS.md`（42 行 / 2,153 B；**tracked、未被 ignore**）——**含凭据，只读核对，本文不打印其内容**
- **只读核对所得（不含任何敏感值）**：文件 42 行；按模式计数：`ssh` 7 行命中、`user`/`host` 各 1 行命中，`password/passwd/secret/token/api_key/private_key/ip/port` 均 **0** 命中 ⇒ 属**远程访问指引（SSH 主机/账号/密钥路径类）**而非口令表。
- **风险（证据级）**：① 该文件**已 tracked 进 Git**（`git ls-files --error-unmatch` 成功），且**未被 `.gitignore` 覆盖**（`git check-ignore -v` → NOT-IGNORED）⇒ 凭据已进入仓库历史；② `tools/pack_audit_package.py:16-19` 的 `ROOT_FILES` 白名单**包含 `FATDUCK_ACCESS.md`** ⇒ 它会被打进**审核包 zip（交付面）**；③ `ci/impact_map.json:565` 亦登记该文件。
- **建议**：**保留在本地并移出仓库**——① 加入 `.gitignore`；② 从 `tools/pack_audit_package.py` 的 `ROOT_FILES` 白名单移除；③ 由**负责人裁决**是否 `git rm --cached`、以及是否轮换该访问凭据（前任无 git 写权限，未执行）；④ 若必须保留在受跟踪面，则改为**只留占位说明**并指向本地注入路径。
- **关系**：与 `GAP-001`（未登记根条目）、`GAP-021/022`（根污染）相关但**不重复**——GAP 未登记"凭据入仓 + 打包白名单命中"这一面 ⇒ 建议接手在 SUMMARY 中列为**独立的新发现**（归属：`GOV-001` 登记面 + `NEXT-PACK` 凭据外泄面，验收门：`git check-ignore -v FATDUCK_ACCESS.md` 有输出 且 `grep -c FATDUCK_ACCESS tools/pack_audit_package.py` = 0）。

---

## 11.5 ⚠️ 交接风险与基准变更（前任离场前实测，**接手必读**）

1. **`问题扫描/REBASE.md` 目前是 `untracked`（未入库）** —— 它是**口径权威**，若发生 clean checkout 或被根清洁线误清，口径将丢失。**接手第一件事之一：确认它已入库**（`git ls-files --error-unmatch 问题扫描/REBASE.md`），未入库则请前台补交（前任无 git 写权限）。
2. **前台已把部分交接物归档进 main**（`900916fb` 等）：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/**` 共 **75 个文件（PSV+MD）**、`_tools/SHARD_BRIEF.md`、以及**较早版本**的 `CONTINUATION.md` 已在库；而 `REBASE.md`、`_gen/landed_stats.json`、`shards/G_GOV_GATE_P1c.md`、`00_README.md`/`INDEX.md` 的 REBASE 抬头、以及本文件的**最新修订**仍未入库（`git status --porcelain=v1 -- 问题扫描 reports/PROJECT-GOVERNANCE-01/root-scan | wc -l` 实测 96 条）。
   ⇒ 接手应以**工作树**为准（工作树比 HEAD 新），并请前台在开工前把这批产物补交，避免"两份真相"。
3. **任务数已由 30 → 33**：新增 `ROOT-004`(本任务)、`ROOT-005/006`、`TEST-GREEN-001`（`1497f796` 立，33 任务）。⇒ 第 8 列"归属"的**可选任务集合与映射计数必须按当前 `TASK_LIST.md` 重算**（本文件 §3 与 SHARD_BRIEF §4 里的 30 任务表已过期，仅作历史）。
4. **凭据面已升级为独立任务**：`c44adc08` 立 **`ROOT-006（凭据入仓 SECURITY-URGENT）`** —— 即前任在 §11.3 登记的 `FATDUCK_ACCESS.md` 风险已被前台接住。接手在 `SUMMARY.md` 中仍应保留该发现的交叉引用，但**不要**与 ROOT-006 重复立任务。
   - **📌 勘误（负责人 2026-09-16 裁决，前任补记）**：凭据那条最终登记为 **`GAP-031`**，**不是 `GAP-028`**（§11.5 第 6 条与本文件此前出现的 `GAP-028=凭据` 写法均以此为准）。现行 `GAP_AUDIT.md` 编号：**`GAP-028` = ROOT 线「注册表/`tests/quality` 执行前就红」**、**`GAP-029` = `run/` 体量**、**`GAP-030` = 控制包执行期前台仍向 main 提交**、**`GAP-031` = 凭据入仓**。⇒ 第 9 列比对基准按**文件内现状**（`grep` 实测）取，**不要**引用本文件写死的编号。
5. **根清洁线已在推进**：`4511712b` 删除 `设计大纲/`（344 tracked）、`e5fba371` ROOT-002 根目录长效机器门、`4fc3e898` ROOT-001/003 账本、`900916fb` 归档治理快照。⇒ 根目录条目与 `ci/checks.json` 仍在变动，**不要**引用任何"当前根条目数/门数"的静态数字。
6. **`GAP` 基准以文件内现状为准**：前任实测为 `GAP-001..GAP-030`，**此后新增 `GAP-031`（凭据入仓，见第 4 条勘误）** ⇒ 合并前**必须重跑** `grep -oE '^(### |\*\*)GAP-[0-9]+' 工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md | grep -oE 'GAP-[0-9]+' | sort -u | wc -l` 并取实测值，**不要**沿用本文件写死的 30/31。第 9 列按「实测 GAP 全集 + U-01..U-08」重比对。
7. **交接物已入库（负责人 2026-09-16 确认）**：`问题扫描/REBASE.md` 连同 `00_README.md`/`INDEX.md` 抬头、分片产物与 `CONTINUATION.md` **已经提交**（`e7f33817` + `46a1599a`）⇒ **第 1 条的"untracked 风险已解除"**（接手如仍见 untracked，说明工作树被回退，须先查 `git log`）。
8. **根目录线又推进两步（接手勿引用静态数字）**：`4511712b` 删 `设计大纲/`（344 tracked）；**`01db973b`（ROOT-007）删除四个根文档 —— 旧宪章 `ASTROCS_PROJECT_CONSTITUTION.md`、`AstroCS_ENGINEERING_CONSTRAINTS.md`、`REVIEW.md`、`CHANGELOG.md`，并把 `evidence/**`（2799 文件）归档到 `run/archive/legacy-control-packs/`** ⇒ 根条目 **61 → 56**。
   - ⚠️ **对 §11 三份根文档处置结论的影响（接手以此为准，勿再按 §11 原建议执行）**：`CHANGELOG.md` 已被 ROOT-007 **删除**（前任原建议"保留+登记"被前台决策取代）；`FATDUCK_ACCESS.md` 归 `ROOT-006`；`VISUAL_CHECK_README.md` 的归档建议**仍待处理**。
   - **旧宪章与旧工程约束被删除后**：`REBASE_TABLE` 第 4 列的"旧判据"只剩**文本引用**（文件已不在树内）⇒ 凡引用旧宪章的条目，**不要再试图 `read` 其原文**；按 `SHARD_BRIEF §2` 的旧→新映射表处理，并在第 4 列注明"原文件已由 ROOT-007 删除"。
9. **`artifacts/**` 保留未动**（含真实基准数据与仍在内更新的 CI 产物），负责人待裁 ⇒ **不要**把 `artifacts/` 下任何文件当"运行产物散落"立 finding，也不要清理。

---

## 12. 与 ROOT-004 验收门的对应（接手完成时逐条自证）

| 验收门 | 复跑方式 |
|---|---|
| 订正覆盖度：结论条目数 == 原始条目数（差集为空） | §8 第 2/7 条；`REBASE_TABLE` 行数 785、ID 双向差集空 |
| 所有 P0 有"权威条款+仓库证据+结论"三要素，抽查 10 条可复现 | §5.3 P0 素材 + §8 第 6 条；`P0_RECHECK.md` 93 条 |
| OPEN 条目 100% 映射到现有任务或「下一轮候选」 | `_gen/stats.json` 映射计数（接手生成） |
| VOID 必须写旧依据与最新替代（无一条只写"过时"） | `awk -F'|' '$7=="VOID" && $4==""' ` 应为空 |
| 原始 finding 文件零删除 | §8 第 1 条；**口径见 §13.2**：基线 1095 → 当前 1096（唯一新增 = `问题扫描/REBASE.md`），`findings/**/*.md` 恒为 327 |
| `问题扫描/` 之外无改动 | §8 第 8 条 |

---

## 13. 交接末次刷新（前任离场时点）

### 13.1 分片状态（末次刷新；接手请用 §8 第 5 条再刷）

> **🎯 全量落盘达成（前任最后一次实测，2026-09-16T08:03Z）**：分片 **26 / 26**、条目 **785 / 785（100%）**、**列数不合格 0 行**、重复 ID 0 ⇒ **ROOT-004 验收门第 1 条（结论条目数 == 原始条目数，差集为空）在分片层已满足**。
> 分片层四态合计：**OPEN 733 / RESOLVED 41 / VOID 7 / UNVERIFIABLE 4**；其中 **P0 = 93 条全额覆盖：OPEN 73 + RESOLVED 20**（P1 444 = OPEN 428 + RESOLVED 16 + VOID 3 + UNVERIFIABLE 2；P2 239；P? 3）。
> `问题扫描` 文件数 **1096**（零删除）；`HEAD=b46a316f`（仍移动）。**接手的剩余工作只剩：写 `merge_rebase.py` 合并 26 片 → 出 `REBASE_TABLE.md`/`SUMMARY.md`/`P0_RECHECK.md` → 第 8/9 列按当时树统一重跑 → 逐门自证。**

> **较早快照（24/26）**2026-09-16T07:48Z）**：分片 **25 / 26 落盘**、条目 **756 / 785**、列数不合格 **0** 行；结论分布 **{'OPEN': 706, 'RESOLVED': 41, 'VOID': 6, 'UNVERIFIABLE': 3}**；`问题扫描` 文件数 **1096**（零删除）；`HEAD=900916fb`。
> **仍未落盘**：`G_GOV_GATE_P2`。
>
> **较早快照**：分片 **24 / 26 落盘**、条目 **735 / 785**、重复 ID = 0、列数不合格 **0** 行；结论分布 **{'OPEN': 688, 'RESOLVED': 38, 'VOID': 6, 'UNVERIFIABLE': 3}**。
> **仍未落盘**：`F_TEST_GAP_P0 G_GOV_GATE_P2`（接手按 §8 第 5 条轮询；若长时间无产出，登记「未完成，交下一任」，**不要重派**）。
> `问题扫描` 文件数 **1096**（零删除）；`HEAD=4511712b`（继续移动，接手须自记）。以下为更早快照，保留以显示增长。

> **✅ 值得沿用的作业范本（`E_TRACE_BREAK_P0P1` 提供）**：该片在**基线漂移后把全部 32 条命令在新 HEAD 上整体复跑，32/32 输出逐字一致**，并在备注区分"整条 OPEN 但个别子项已消失"。**在移动基线环境下，这就是"可复跑证据"的正确做法**（接手合并时若树又变，对引用行号/计数的行应按此法整体复跑）。

> **收口终态（前任最后一次动作）**：分片 **22 / 26 已落盘**、条目 **665 / 785（84.7%）**、重复 ID = 0、列数不合格 = 0（曾出现的 5 片问题均已由作者自修）。
> 结论分布：**OPEN 618 / RESOLVED 38 / VOID 6 / UNVERIFIABLE 3**（P0 已落盘 78：OPEN 61 + RESOLVED 17）；`问题扫描` 文件数 **1096**（零删除）；`HEAD=c44adc08`（继续移动）。
> **仍未落盘的 4 片（接手按 §8 第 5 条轮询，勿重派）**：`C_ALG_IMPL_a`(35) / `E_TRACE_P2_J_FS`(27) / `F_TEST_GAP_P0`(15) / `G_GOV_GATE_P2`(29) —— 合计 **106 条**（785 − 665 = 120 条差额中另有 14 条属 `G_GOV_GATE_P1c` 等未写满的片）。
> 已落盘片清单：A_SCI_DEF_P0/P1a/P1b/P2、B_STD_MISMATCH_ALL、C_DOC_CODE_GAP_P0/P1a/P1b/P2、C_ALG_IMPL_b、D_COMMENT_P1/P2、G_GOV_GATE_P0/P1a/P1b/P1c(部分)/P1d、F_TEST_GAP_P1/P2、H_NUMERIC_ALL、I_DOC_HYGIENE_ALL（共 22 个文件）。

> **更新（同一收口窗口内的再刷新，2026-09-16T07:31Z）**：COUNT=22；VERDICT: {'OPEN': 618, 'RESOLVED': 38, 'VOID': 6, 'UNVERIFIABLE': 3}；BADCOLS_TOTAL=0；SHA=c44adc08。
> （下面是较早一次的同类快照，保留以显示增长速度。）

> **较早快照**（快照会继续变化，接手以自测为准）：分片 **21 / 26 已落盘**、条目 **637 / 785（81.1%）**、**重复 ID = 0**、**列数不合格分片 = 0**（此前出现过的 `D_COMMENT_P1/D_COMMENT_P2/G_GOV_GATE_P1d/C_ALG_IMPL_b/A_SCI_DEF_P2` 均已被各自作者修复）；
> 结论分布：**OPEN 594 / RESOLVED 34 / VOID 6 / UNVERIFIABLE 3**（P0 已落盘 50：OPEN 37 + RESOLVED 13）；`问题扫描` 文件数 **1096**（零删除）；`HEAD` 离场时 `c44adc08`（移动目标）。

| 分片 | 条目 | 状态 | 产物 |
|---|---|---|---|
| `A_SCI_DEF_P0` | 15 | **DONE**（OPEN 14 / RESOLVED 1 = M3-A-001 / VOID 0 / UNVERIFIABLE 0）——该片属 P0，**前任已亲自复核其全部 15 条**（见 §13.6） | `shards/A_SCI_DEF_P0.psv` |
| `A_SCI_DEF_P1a` | 38 | **DONE**（OPEN 35 / RESOLVED 3 / VOID 0 / UNVERIFIABLE 0） | `shards/A_SCI_DEF_P1a.psv` |
| `B_STD_MISMATCH_ALL` | 25 | **DONE** | `shards/B_STD_MISMATCH_ALL.psv` |
| `C_DOC_CODE_GAP_P0` | 18 | **DONE**（OPEN 10 / RESOLVED 8） | `shards/C_DOC_CODE_GAP_P0.psv` |
| `C_DOC_CODE_GAP_P2` | 24 | **DONE**（OPEN 22 / RESOLVED 2） | `shards/C_DOC_CODE_GAP_P2.psv` |
| `C_DOC_CODE_GAP_P1b` | 32 | **DONE** | `shards/C_DOC_CODE_GAP_P1b.psv` |
| `D_COMMENT_P1` | 32 | **DONE**（31 行；前任首查时曾 12 行列数≠10，作者已自行修复，末查 badcols=0） | `shards/D_COMMENT_P1.psv` |
| `D_COMMENT_P2` | 35 | ⚠️ **已产出但 1 行列数≠10** | `shards/D_COMMENT_P2.psv`（合并前需修） |
| `G_GOV_GATE_P1d` | 31 | ⚠️ **已产出但 13 行列数≠10** | `shards/G_GOV_GATE_P1d.psv`（合并前需修） |
| `I_DOC_HYGIENE_ALL` | 44 | **DONE**（44 行，badcols=0） | `shards/I_DOC_HYGIENE_ALL.psv` |
| `F_TEST_GAP_P1` | 46 | **DONE**（OPEN 46 / RESOLVED 0） | `shards/F_TEST_GAP_P1.psv` |
| `F_TEST_GAP_P2` | 17 | **DONE**（OPEN 17 / RESOLVED 0） | `shards/F_TEST_GAP_P2.psv` |
| `G_GOV_GATE_P1c` | 31 | **部分落盘**（11/31 行）——该片仍在跑或已中断，**不得重派**，未齐则登记「未完成，交下一任」 | `shards/G_GOV_GATE_P1c.psv` |
| 其余 17 片 | — | 未完成（subagent 仍在跑；产物落盘即 DONE） | — |

> ⚠️ **子代理归属提醒**：这 26 个分片 subagent 是**前任会话的子代理**（父 = ROOT-004 执行会话）。它们的收尾通知**只会发给前任会话，不会发给接手**；但**产物直接写盘**（`shards/<分片>.psv` + `.md` + `run/.../logs/shards/*.log`）。
> ⇒ 接手**不要等通知**，按 §8 第 5 条**轮询磁盘**判断完成度；长时间无新 `.psv` 落盘即视为「未完成，交下一任」，**不要重派已派过的 26 个分片名**（避免重复劳动与同文件写入冲突）。

**处理 `D_COMMENT_P1` 的两条路径（接手选一）**：① 把该片 `_assign/D_COMMENT_P1.tsv` 重新派发给一个新 agent（按 `SHARD_BRIEF.md` 同一字段格式重写，禁止字段内出现 `|`）；② 由下一位人工按原文修复这 12 行的字段边界（第 6 列"当前证据"内的 `|` 改为 `；`）。**其余各片列数合格，可直接合并。**
> ⚠️ **格式合格性是移动目标**：分片 subagent 会在收到反馈后重写自己的 `.psv`（前任亲历 `D_COMMENT_P1` 由 12 行不合格 → 0 行不合格）。**合并前必须重跑 §8 第 5 条的列数校验**，以那一刻的结果为准；对 `badcols≠0` 的片按上面两条路径处理，**不要沿用本表的旧结论**。

### 13.2 零删除与越界核算（前任自证）
- `find 问题扫描 -type f | wc -l`：**基线 1095 → 现 1096**；唯一新增 = `问题扫描/REBASE.md`（新产物，非删除）；`find 问题扫描/findings -type f -name '*.md'` **恒为 327**。
- 前任只做了 3 处**追加式**编辑（不改原文）：`问题扫描/REBASE.md`（新建）、`问题扫描/00_README.md` 顶部 REBASE 抬头、`问题扫描/INDEX.md` 顶部 REBASE 抬头。
- 其余写入全在 `reports/PROJECT-GOVERNANCE-01/root-scan/**` 与 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/**`；**零 git 写**（未 commit/push/branch/stash/reset/clean/checkout）。

### 13.3 ⚠️ 基线在交接期内发生位移（接手必须重取）
- 前任开工时：`HEAD = main = ecf6ad6f`，`origin/main = f96dff61`。
- 交接末次实测：`HEAD = main = origin/main = 2c328348304d033aecfa81faf79d1c6cd802b30a`；期间的提交：
  - `5f891080` docs(governance): ROOT-004 订正旧 bug 清单；登记负责人裁决与调度裁决（30 任务）
  - `2c328348` docs(governance): 登记 GAP-024/025 与 CFG-001 裁决；消除 ROOT 卡二义
- 影响：① 分片 subagent 的取证在 `2c328348` 树（比前任的机械核验更新）⇒ **合并时以分片证据为准并复核**；② `GAP_AUDIT.md` 已从 GAP-001..023 扩到 **GAP-001..025**（另有 §5.6 新增内容）⇒ 第 9 列"GAP关系"的比对基准要按**当前** `GAP_AUDIT.md` 重做一次；③ `TASK_LIST.md` 任务数已由 29 → **30**（新增任务见该文件）。
- **前任离场前最后一次实测：`HEAD = 939d3f6c57bb603e98e11e4ee2f4f5ab4c88c876`**（前台仍在连续提交治理文档 ⇒ **基线是移动目标**）。
- **接手第一件事：`git rev-parse HEAD main origin/main` 并记录**，随后所有判定以**你开工时**的树为准，并在 `REBASE_TABLE.md` 抬头写明该 SHA；**本文出现的任何 SHA 都是历史值，不得当基线**。
- 分片 subagent 的取证横跨 `ecf6ad6f → 2c328348` 两个时点 ⇒ 合并时对"涉及治理文档/任务清单/GAP 编号"的行**必须按当前树复核一次**。

### 13.4 前任亲自复跑的 P0 事实摘录（**不是最终判定**；接手按 §2 定结论并补齐剩余）
> 说明：下表是前任用**自己跑的命令**（`run/.../p0_verify_A2.sh`、`p0_verify_B.sh`，输出见同名 `.out`）在当前树取到的**事实**与**判定方向**。`方向`列仅供接手参考，**最终结论必须以分片/接手自己的复跑为准**。

| ID | 前任实测事实 | 方向 |
|---|---|---|
| M1a-A-001 | `ASTROMETRY.md:48` 仍写 `(u,v)=CD·(xp−CRPIX)+SIP_A/B(u,v)`（px 加 deg 混量纲） | OPEN |
| M1a-A-002 | `plate_solve/memory.md:55` T3 独立 median 0.897px、`:63` VERDICT PASS；`PLATESOLVE.md:186` 门 ≤0.5″ | OPEN |
| M1a-A-003 | `p3_projection.cpp:198` `theta = -yd`；CAR/AIT 段无 crval_dec_deg 参与 | OPEN |
| M2a-A-1 | `drizzle_engine.cpp` drop_area 归一段在位；SCI §5/§7/§11 互斥文本仍在 | OPEN |
| M2b-A-01 | `aio_hips_writer.cpp:481` 只判 `nside<512`+`ilog2_u64`（:105/:493）；正解 `lib/hips/src/module_entry.cpp:342`（2 的幂+上界）仍在 | OPEN |
| M2b-A-02 | `aio_hips_writer.cpp:1069` `nside_k = 1u << (k + 9)` | OPEN |
| M3-A-001 | **前任复核确认 RESOLVED**：生产控制点走 Horne 1986 `SNR_F`（`snr_estimator.cpp` `sourceSnrFromPsfRow` 6 处 + `orchestrator.cpp:4296/:4500` `snr_extract_model_v3`）；旧 `(A-B)/residual_scale` 仅存于退休注释与负向断言（`snr_science.cpp:10`、`snr_estimator.cpp:4/:56/:187`、`p1snr_science_test.cpp:242/:272`）；SCI 侧 `CONTROL_WEIGHT_SNR.md:44/:52`、`NOISE_MODEL.md:98` 已同步重定义 | **RESOLVED**（证据：`run/.../p0_verify_C_M3A001.out`） |
| M3-A-002 | `stage2.cpp:1141`/`:1414` `weights[s]=support_v[s]*snr_v*snr_v` | OPEN |
| M3b-A-01 | `sdet_api.cpp:111` `sdet_gaussian_f`、`:375` 挂入拟合 | OPEN |
| M3b-A-02 | `sdet_api.cpp:1537` `rec.flux=(float)fit_results[i].A` | OPEN |
| M3b-A-03 | `orchestrator.cpp:2470` `[4]=flux_uncertainty (PSF mad proxy)` | OPEN |
| M4-A-01 | `rejection.cpp:1660-1662` 全拒→UNDERDETERMINED 路径在位 | OPEN |
| M4-A-02 | `integrate.cpp:44/:50` canonical reducer = max(**accepted** support) | OPEN |
| M7-A-001 | `PHASE2_UPM.md:46` 三因子积式与 `:47` raw/normalized 两式并存 | OPEN |
| M7-A-002 | `PHASE2_INTEGRATION.md:125-129` weight_mode 语义块在位 | OPEN |
| M1a-B-001 | `ipv_wcs.cpp:411` `NB_GRID=41 (整改: 7 -> 41)`；registry `:66/:76` 仍写"7×7 网格…SCI 层已显式冻结该口径" | OPEN |
| M2a-B-1 | registry `:45/:186/:193` 要求 **J2016.0**；`ASTROMETRY.md:23/:33/:103` 仍写 **J2000** | OPEN |
| M9-B-1 | `ipv_wcs.cpp:361` 仍写"单位: … = 1/像素^(i+j-1) (SIP 标准)"；`module_adapters.cpp:1933` 写出面在位 | OPEN |
| M2b-B-01 | `aio_hips_writer.cpp:137-138` `dir=ipix/10000; npix=ipix%10000`；`runtime/io/hips_core.c:558-559` 同 | **需标准原文**（无外网 ⇒ 宜 UNVERIFIABLE 或引仓库内可核条款） |
| M2b-B-02 | `aio_hips_writer.cpp:242-243` 有 `ORDERING=NESTED`/`COORDSYS=C`（**需确认是否在 write_moc_fits 内**） | 需细查 |
| M2b-B-03 | `aio_hips_writer.cpp:943` 仍 `3600.0*180.0/π*…/nside` 写 `hips_pixel_scale` | OPEN |
| M2b-B-04 | registry `D.hips` CLAUSES 锚与九必填键集需逐键核对（`creator_did` 等实现在位） | 需细查 |
| M2b-B-07 | `lib/hips/src/module_entry.cpp:342` 有 `nside ≤ 2^24` 上界；"order ≤ 29" 条款归属需核 | 需细查 |
| M2b-B-09 | `aio_hips_writer.cpp:15/:170-182` 注释称由 cfitsio 写 CHECKSUM 且置 ASCII 0 基准态 | 需细查 |
| M2b-C-01 | `module_adapters.cpp:28` 仍称"AIO-002 原子发布内建"；writer 内 `fsync` 命中 **0** | OPEN |
| M2b-F-01 | `astropy_healpix` 仅命中 `docs/archive/history/memory_V18R2-V19…:1664/1676/1691/2063`（历史日志），oracle 生成器/数据缺位 | OPEN |
| M1a-C-001 | `ASTROMETRY.md:39/:125` 仍 `NB_GRID=7`；`ipv_wcs.cpp:411` 实为 41 | OPEN |
| M1a-C-002 | `ipv_wcs.cpp:219` 有 `result.success = true;`（**需看是否在 extract_wcs_sip 的失败分支**）；ledger 标 FIXED | 需细查 |
| M1a-C-003 | `wcs_tan.h:11` `crpix1,crpix2; // 参考像素 (1-based)` 在位 | OPEN |
| M1a-C-004 | `lib/phase3_session/p3_wcs.cpp` 内 `FOV` **零命中** | OPEN |
| M3-C-001 | `p1phot_fixgates.cpp` 已存在（`|r_consistent|>=3` 门，:6/:114/:129/:142）；ledger FIXED | RESOLVED |
| M3-C-002 | ⚠️ 前任 grep 的 `lib/photometric_calib/src/star_matcher.cpp` **路径不存在**，需按 basename 重定位；ledger PARTIAL/VERIFIED | 需细查 |
| M3-C-003 | `NOISE_MODEL.md:37` 默认 `min_samples=5` 仍在；实现取参数（`noise_model.cpp:77/90/103/107`），默认值调用点需查 | 需细查 |
| M3b-C-01 | `docs/KNOWN_LIMITATIONS.md` 与 `docs/owner/RELEASE_STATUS.md` 对 `0.5px` **零命中** | OPEN |
| M3b-C-02 | `module_adapters.cpp:1616` `const astrocs::phase1::StarDetector det(5.0)` | OPEN |
| M4-C-01 | `sampler.cpp:89-102` `pf_grid[3]`+两段分段线性（ledger FIXED） | RESOLVED |
| M4-C-02 | `upm.cpp:226/:244` `zero_anchor_weight = 1e-3`；ledger FIXED（两装配显式 1e-3） | RESOLVED |
| M4-C-03 | `stage2.cpp:1120/:1141/:1389` support 回退仍在、`:1378` 注释"禁止静默换 support"；ledger FIXED（默认 fail-closed+计数） | 需细查（默认/legacy 两面） |
| M4-F-01 | `add_subdirectory` 前 20 条无 `lib/phase2`；`control_median_mc_test`/`kcorr_matrix_test` 在 CMakeLists/tests/unit/ci **零命中** | OPEN |
| M4-F-02 | `huber`/`IRLS` 仅命中 `tests/backend/test_p2003_seam_oracle.py`、`test_p2002_parallel_upm.py`（是否独立复算需看） | 需细查 |
| M6a-G-001 | `check_comments.py:9` `STALE_PATTERNS` 定义后**全仓仅此定义行**；`:63` 为 `return 0 if status=="PASS" else 1`（**非无条件 return 0**，findings 措辞需订正） | OPEN（细项需订正） |
| M6b-G-002 | 四份 `RELEASE_STATUS.md` 全在（`docs/`、`docs/owner/`、`docs/review/`、`docs/archive/review/`） | OPEN |
| M6b-E-001 | `README-DOCS.md:13`、`DEVELOPER_GUIDE.md:38`、`API_STANDARD.md:14` 仍指 `docs/TRACEABILITY.csv` | OPEN |
| M6b-G-001 | ⚠️ 前任 `sed -n '258,272p' tools/traceability/check_traceability_matrix.py` 输出与 finding 描述不同 ⇒ **C7 skip 判据需重定位** | 需细查 |
| M3-E-001 | `docs/TRACEABILITY.csv` 有 63 行 VERIFIED；ledger FIXED-PARTIAL | 部分 |
| FD-F-003 | `ci/run.py:103` `EMPTY_OUTPUT_SILENCE_EXEMPT` 在册 | OPEN |
| M1a-F-001 | `tests/unit/p3_projection_test.cpp:214` `dec_o = -yd`；`tests/backend/test_p3_projection_oracle.py:156` `dec = -y_deg`（与实现同式） | OPEN |
| M3-F-001 | `NOISE_MODEL.md:117/:140` 仍承诺 NumPy `rtol 1e-9`；测试面命中仅该文档 | OPEN |
| M3b-F-01 | `gate2_psf_oracle.py:17` `centroid_p95_le_0.01px_converged`；ledger PARTIAL/FIXED（新增生产 0.3px 门） | OPEN（残余） |
| M3b-F-02 | `curve_fit` 在 `tests/`+`lib/` **零命中**；`PSF.md:99` 仍承诺 scipy/NumPy `curve_fit` | OPEN |
| M3b-F-03 | `p1star_fixtures.hpp:60` `p<0.15 → amp=40+100p // SNR≈20 谱段`；`test_isa_avx.py:109` `assertTrue(True)` | OPEN |
| M8-F-001 | `tests/abi` 现含 4 探针 + `mod001_install_load_check.py`；ledger FIXED（18→22 用例 + R11） | RESOLVED |
| M8-F-002 | `io_ownership_test.cpp` 行数已远超 32（:25-32 为其它内容）；ledger FIXED（return failures==0?0:1） | RESOLVED |
| M8-F-003 | `test_cli_single_install.py:40` 仍有 `raise unittest.SkipTest`（ledger FIXED 称改硬失败 ⇒ 需看该行属哪个用例） | 需细查 |
| M8-F-004 | `docs/standards/checks/check_standards_registry.py:249` 仍只 `path_exists` | OPEN |
| M9-F-1 | 8 个孤儿 TU 名在 `CMakeLists.txt`/`tests/unit/CMakeLists.txt`/`ci/checks.json` **零命中** | OPEN |
| M2a-F-1 | `candidate_oracle_test`/`variance_propagation_test` 在上述三处 **零命中**；`DRIZZLE.md:115/:133` 仍以其为 CONFORMANT 证据 | OPEN |
| M5b-C-01 / M5b-C-02 / M5a-G-001..005 / V9-* / V11-N-01/04/05 / V13-N-03 / V18-N-12 / V19-N-01 / V2-N-08 / M2a-H-1/2 / M9-H-1/2 / M3b-H-01 / M2b-B-04/07/09 / M6a-C-001 / M8a-G-001 / M7-G-001 / M3b-G-01 | 前任**未**逐条亲跑（时间/上下文所限）⇒ **接手必须补**（其中 ledger 标 FIXED 者优先复验：M2a-H-1、M9-H-1、M9-H-2、M3b-H-01、V2-N-01、M5a-G-001/002） | 待复核 |

### 13.6 P0 亲自复核进度（前任）
- **已逐条亲自复核**：`A_SCI_DEF_P0` **15/15**（与分片判定一致：14 OPEN + 1 RESOLVED；RESOLVED 那条由前任独立复跑确认，证据 `p0_verify_C_M3A001.out`）。
- **已用自跑命令覆盖事实**（未定终判）：见 §13.4 共 55+ 条，其中 20 条为 ledger 标 FIXED/PARTIAL 者仍需按 §2 定结论。
- **仍未亲自复核的 P0**：`C_DOC_CODE_GAP_P0`（18 条，分片已判定：OPEN 10/RESOLVED 8）、`F_TEST_GAP_P0`（15 条）、`G_GOV_GATE_P0`（28 条）、`E_TRACE_BREAK_P0P1` 的 P0 2 条、`B_STD_MISMATCH_ALL` 的 P0 10 条、`H_NUMERIC_ALL` 的 P0 5 条 ⇒ **合计 78 条 P0 需接手按"三要素"逐条复核**（素材：`_gen/p0_sections/<ID>.md` + `_gen/p0_anchor_v2.json` + `_gen/p0_quote_liveness.json` + 各片 `.psv` 第 6 列）。

### 13.7 已落盘条目的机械统计（前任用 `_tools/summarize_landed.py` 从磁盘算出，可复跑）

**时点快照**：分片 **16 / 26**，条目 **471 / 785**（60.0%），重复 ID = 0。

| 结论 | 合计 | P0 | P1 | P2 | P? |
|---|---|---|---|---|---|
| OPEN | **440** | 34 | 268 | 136 | 2 |
| RESOLVED | **26** | 9 | 14 | 3 | 0 |
| VOID | **3** | 0 | 1 | 1 | 1 |
| UNVERIFIABLE | **2** | 0 | 2 | 0 | 0 |
| 合计 | 471 | 43 | 285 | 140 | 3 |

按类别（OPEN/RESOLVED/VOID/UNVERIFIABLE）：A_SCI_DEF 49/4/0/0；B_STD_MISMATCH 25/0/0/0；C_ALG_IMPL 32/0/2/0；C_DOC_CODE_GAP 91/14/1/0；D_COMMENT 67/0/0/0；F_TEST_GAP 63/0/0/0；G_GOV_GATE 75/4/0/0；I_DOC_HYGIENE 38/4/0/2。

- 复跑：`timeout 300 python3 reports/PROJECT-GOVERNANCE-01/root-scan/_tools/summarize_landed.py` → 同时写 `_gen/landed_stats.json`。
- ⚠️ 该脚本**跳过列数≠10 的行**（当前跳过 0 行），并打印 `malformed_rows_skipped` 供核对。
- **已有 `VOID` 与 `UNVERIFIABLE` 的合格样例**（接手可作判据范本）：
  - `VOID`：`M5b-C-07` —— 旧判据（旧宪章 §18.1 只冻结四投影、ZEA 不在内）已被 `ASTROCS_DESIGN.md §5.3` **相反替换**（冻结八投影含 ZEA）；残余缺口（registry 仅 4 投影）另见 GAP-011。**这就是"设计已变 ⇒ VOID 且写最新替代"的标准写法。**
  - `UNVERIFIABLE`：`M5a-I-001`（SCI 逐位等价 vs ALG 1e-6/1e-12 是否构成冲突 ⇒ 缺负责人裁决层级关系）；`M7-I-101`（8 处复杂度节锚在位但原 finding 无逐字引文 ⇒ 缺重推口径与基准数据）。
- ✅ **`VOID` 口径裁定与范本（前任裁决，接手沿用；由 `H_NUMERIC_ALL` 的提问触发）**：`VOID` = **原判据与"现行权威明文要求"相反、或其所依路径/条款已废止** ⇒ 不构成偏差 ⇒ 判 VOID；**不得**因为"科学上仍有风险/仍有争议"而改判 OPEN（违反三禁令之二）。**但 VOID ≠ 无风险**：对冻结 SCI 的科学异议须另走 `ENGINEERING_SPEC.md §3` 的科学变更/负责人裁决通道，并在第 10 列写明"其科学异议另需 SCI 变更/裁决（建议登记 NEXT-PACK）"。
  - 已核准范本（可直接引用）：**`M7-H-104`**（原判据与 `docs/science/NOISE_MODEL.md §7/§9` 的 `max(var,1e-12)`/负预测 clamp 明文要求相反；前任实测 `:21/:39/:49/:53/:75/:84/:91/:92` + `docs/algorithms/NOISE_ESTIMATION.md:21/:39` 复核确认 → 维持 VOID）；**`M5b-C-07`**（旧宪章 §18.1 四投影被 `ASTROCS_DESIGN.md §5.3` 八投影含 ZEA 相反替换）；**`V10-N-09`**（权威链无"环境变量严格解析"条款，且实测 `AIO_LOG_INFO=0` 即默认值）。
- ⚠️⚠️ **【通用规则·已在多片复现】账本 `fix_state/verified_state` 不可作为结论依据 —— 必须按证据判，冲突时以证据为准并登记"待复核"。**
  - 已复现的冲突实例（截至收口）：**`M8-H-001`**（账本 `NOT_A_DEFECT` → 读码发现 `GaiaTraceCtx` 在 `omp parallel for` 内共享、`trace->nodes++` 非原子，与"只原子更新"注释相反 ⇒ 证据判 **OPEN**）；**`M3-E-001`（P0）**（账本 `FIXED/FIXED-PARTIAL` → `E_TRACE_BREAK_P0P1` 在**新 HEAD** 上复跑仍为 **OPEN**：`docs/TRACEABILITY.csv:20` 仍 `status=VERIFIED`/`test_ids=TEST-CAL-001`，而该测试只测测光比例应用、`ac_correct_frame` 不在 public_api）；**`M3-C-002`/`M3b-F-01`**（账本 `PARTIAL`，分片判 OPEN-残余面）。
  - 反向也成立：账本 `OPEN` 而实物已被删除的（如 `A_SCI_DEF_P2` 的 `M7-A-204`/`V12-N-15`、`C_ALG_IMPL_b` 的 `V7-N-09`）应判 `RESOLVED`/`VOID`，不得沿用账本状态。
  - **操作口径**：合并时对**每条** ledger 标 `FIXED/PARTIAL/VERIFIED/NOT_A_DEFECT` 的行做一次"证据优先"复核；`REBASE_TABLE` 第 10 列注明"账本状态 vs 本次结论"。
- ✅ **GAP 比对基准 = GAP-001..GAP-030（前任已裁定，接手直接用）**：`GAP_AUDIT.md` 里 **22 条是 `### GAP-0NN` 标题式**（001–022），**8 条是 `**GAP-0NN　…**` 粗体式**（023–030，散落在 §5.5/§5.6…）。
  ⇒ **只 `grep '^### GAP-'` 会漏掉 8 条**（多名分片 subagent 已因此误报"只有 22 条"，并把 `M2a-C-6`↔GAP-024、`M3-C-008`↔GAP-008 等比对写得口径不一）。
  **判定命令（合并前必跑）**：
  ```bash
  grep -oE '^(### |\*\*)GAP-[0-9]+' 工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md | grep -oE 'GAP-[0-9]+' | sort -u | wc -l   # 必须 = 30
  grep -oE 'U-[0-9]{2}' 工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md | sort -u   # U-01..U-08（另有 U-00 字样，需甄别）
  ```
  **处置建议**：合并时对第 9 列做一次**统一重比对**（按 30 条 GAP + U-01..U-08），把分片写的"无/与 GAP-0xx 重复"按同一基准归一；**重复不合并、保留原 ID**（ROOT-004 硬规则）。

### 13.5 接手最短路径（建议顺序）
1. `git rev-parse HEAD main origin/main` → 记 SHA；
2. 读 `SHARD_BRIEF.md`（口径+映射+字段）与本文 §1–§4；
3. 按 §8 复跑 1–5 条，刷新分片表；
4. 处理 `D_COMMENT_P1`（§13.1 两条路径选一）；
5. 等 17 片落盘 → 写 `merge_rebase.py` → 出 `REBASE_TABLE.md`；
6. 出 `SUMMARY.md`（四态统计 + OPEN 的 P0/P1/P2 + 映射计数 + 下一轮候选 + §11 三份根文档处置）+ `P0_RECHECK.md`（用 §13.4 补齐）；
7. 按 §12 逐门自证 → 汇报。

