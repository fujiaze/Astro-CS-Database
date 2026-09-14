# 修复账本（FIX_LEDGER）· 隔壁怎么用这一份

- **数据面**：`FIX_LEDGER.csv`（521 条，机器读写）· `FIX_LEDGER.jsonl`（同数据逐行 JSON）· `FIX_LEDGER.md`（P0 82 条的人读表）
- **生成器**：`python3 问题扫描/_tools/gen_fix_ledger.py` —— 由前台重跑；**重跑会按 `id` 保留你填的处置列，只刷新判定列**，所以你可以放心直接编辑 CSV。
- 来源：`问题扫描/findings/<类别>/p<N>/*.md` 的条目正文（P0 82 / P1 262 / P2 118，九类齐全）。

## 一、列的归属（越界即视为污染，前台会重跑覆盖）

| 列 | 谁能写 |
|---|---|
| `id · priority · category · producer · release_blocker · owner_decision · title · position · evidence · impact · clause · related · suggested_disposition · evidence_file · source_line_state` | **只读（判定列，审计结论）** |
| `fix_state · fix_commit · fix_date · regression_test · fixed_by · fix_note · verified_state · verified_by · verified_date` | **你们填（处置列）** |

## 二、处置列的取值（请用枚举原文，别自造词）

- `fix_state`：`OPEN`（默认）/ `FIXING` / `FIXED` / `PARTIAL` / `CANNOT_REPRODUCE` / `NEEDS_DECISION` / `DISPUTED`
  - `PARTIAL`：**必须**在 `fix_note` 写清"残余是什么"（本审计大量条目是"修了一半"型）。
  - `NEEDS_DECISION`：卡在科学口径/条文冲突上 → 同时在 `fix_note` 写 `owner_decision` 列已有的编号（如 `A-08`），**不要自己定科学口径**（宪章 §1.1/§1.2）。
  - `DISPUTED`：你们认为这条判错了 → 在 `fix_note` 写**反证锚点**（`path::symbol` + 当场命令），**不要改判定列**。
- `fix_commit`：`main` 上的短 SHA（宪章 §14.5 只在 main 原子提交）；一次提交修多条就重复同一 SHA。
- `fix_date`：`YYYY-MM-DD`（审计当日为 2026-09-14）。
- `regression_test`：**这一列是硬要求**，取值 `ADDED:<ctest名或文件::用例>` / `NOT_NEEDED:<一句理由>` / 留空 = **视为未加锁**
  - 为什么硬要：本审计最大的一类复发根因就是**修过但没人守住**（簇 9「修复即扩散」六站点、`B2-A17` 正按错误口径交付）。**没有回归锁的 `FIXED` 会被 R 层降级为 `PARTIAL`。**
- `fixed_by`：域代号或节点（如 `SA-IO`、`vm-bj`）。
- `fix_note`：自由文本，一行以内；含 `#` 或逗号无妨（CSV 已加引号）。
- `verified_*` 三列**留给 R 层复验填**，你们不要动。

## 三、顺序建议（§17.10：P0/P1 清零才可发布，故 `release_blocker=Y` 共 **344** 条）**

1. **先筛 `priority=P0` 且 `release_blocker=Y`**（82 条）。其中真正阻塞发布的建议按这个次序（前台裁定，理由见 `SUMMARY.md` 簇号）：
   - **FD-F-003**（Windows C++ 单测自 09-07 起一次没跑过而账面全绿）、**M5a-G-002**（CPU 门吃 1 核口径、机器 16 核，历史 177 条里按容量口径过 85% 仅 6 条）、**M5b-G-01**（验收的是 `cmake -S cli` 兼容图产物、交付的是根图；且改验交付物即红）、**M6a-G-001 + FD-G-002 + FD-F-001/F-002**（门不会红 / 红被豁免 / 子串断言）、**M8a-G-001**（许可与依赖登记面：同目录 5 行自相矛盾，GSL PUBLIC 而登记面零命中）、**M3-A-002/M4-A-003**（§6.3 四份互斥，**卡 A-01/A-02 裁决**）、**A-36 相关**（`providers/` 交付单元携带未登记 `ALG-0NN` 作 `sci_contract_id`）。
2. **`NEEDS_DECISION` 的条目请先标出来给我**——我在 `40_OWNER_DECISIONS.md` 集中提请负责人，不在代码里替负责人裁决。
3. 修的时候顺带遵守：**同一条事实只在一个 `id` 上登记处置**；若你们把两个 `id` 合成一次提交，两行都要填同一 SHA。

## 四、三件别做

1. **不要删行、不要改 `id`**（会破坏 `findings/` 与 `_merge/` 的追溯链，宪章 §17.2）。要作废一条走 `fix_state=DISPUTED` + 反证。
2. **不要为了过某个门去改科学公式、默认容差或阈值**（§12.3、§17.6；本审计已多处记录"为过检查而写与宪章相悖规则"的实例）。
3. **不要动 `问题扫描/**` 下除本账本以外的任何文件**（审计档案与 findings 是证据面，改了就断链）。

## 五、与审计其余部分的关系

- 条目全文与证据：见每行的 `evidence_file`（`问题扫描/findings/...`）。
- 跨域主题与"唯一总述条目"归属：`问题扫描/INDEX.md` §三（**同义总述别再立一份**）。
- 需负责人拍板的：`问题扫描/40_OWNER_DECISIONS.md`（A 组 36 条 / B 组 / C 组 12 条 / D 组 6 条）。
- R 层复验规则（含四条硬规则与八个悬项）：`问题扫描/_merge/00_R_TIER_PLAN.md`。
