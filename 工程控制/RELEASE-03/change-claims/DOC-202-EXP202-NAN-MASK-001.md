# 变更 claim：DOC-202-EXP202-NAN-MASK-001 — NaN 处置「掩膜」定案落地（撤销 DISP-DRZ-004 的 CLOSED）

- 控制包：RELEASE-03 / 任务 **DOC-202**（`工程控制/RELEASE-03/tasks/DOC-202.md`）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §9（产品无效值语义唯一口径）、§5.3（导出输入语义）、§0.2（详细文档层不得与本设计相反）
- 依据（裁决正本）：`工程控制/RELEASE-03/GAP_AUDIT.md` **§5.1 EXP-202 定案「掩膜」**
  （样本级掩膜 + 覆盖级 NaN + 强制计数；三数据面 + 多轮独立复核）
- 依据（文字稿，逐字采用）：`run/RELEASE-02/实验/E08-NaN处置/results/evidence_block_draft.md` §2/§4
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（科学正确性优先 + **变更 claim** + 一致性回归）；`AGENTS.md` §1.1/§8
- 状态：**已落地**（文档侧）；**代码侧**待改（`drizzle_engine.cpp` 由「传播」改「掩膜」+ 补计数暴露）
- 影响类：**科学语义级**（**改**无效值处置口径；**不改**任何公式形式、常数、默认容差、SCI/ALG 冻结定义、数值结论）

## 1 变更内容

| # | 文件 | 原状 | 改为 | 依据 |
|---|---|---|---|---|
| 1 | `docs/standards/NUMERIC_STANDARD.md` §MUST（原 `:13`） | 「输入校验返回显式 INVALID_* 状态，禁止 NaN 传播为合法产品」（含糊，与 `STANDARDS_REGISTRY` 的 DISP-DRZ-004 相反） | **rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`** 唯一口径：样本级掩膜 + 重归一 + 覆盖级 NaN（`signal=NaN ∧ support≤0` 互推）+ 强制计数 `n_rejected_nonfinite`；**禁止** 0/±Inf 替代；**禁止**静默剔除 | EXP-202 定案；E08 文字稿 §2 |
| 2 | `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a | 只有全局 `invalid_policy` 一行，**无** `invalid_handling` 子块 | **新增 `invalid_handling` 子块**（`rule_id/aggregation/zero_eligible_samples/zero_substitution/rejection_counting/count_field`）+ 定义表 + 4 条处置规则 + 输入情形对应表 + 下游可判定性 + 一句话版本（**逐字采用 E08 文字稿 §2/§4**） | EXP-202；前台追加指示 |
| 3 | `docs/standards/STANDARDS_REGISTRY.md` `D.drizzle` DEVIATION 字段（原 `:164-165`） | `DISP-DRZ-004` 被**闭环移除**（「现行实现为值 NaN 经 `F_p` 传播、不掩膜」） | `DISP-DRZ-004` **加回** DEVIATION；闭环声明作废 | EXP-202 三面实测推翻「传播」判据 |
| 4 | 同文件 D.drizzle 清单行（原 `:173`） | 该行 `CONFORMANT` + `~~DISP-DRZ-004~~ 已闭环` | **`PARTIAL`** + `DISP-DRZ-004` **TRACKED/OPEN**（附 rule_id 与整改归属 P1-DRZ-IMPL） | 同上 |
| 5 | 同文件 D.drizzle 偏差表（原 `:183`） | `已闭环（2026-09-20）` | **`高（P1）` / TRACKED/OPEN**，写明「被三面实测推翻」与 rule_id | 同上 |
| 6 | 同文件 §3 偏差索引（原 `:271`） | `CLOSED` | **`TRACKED`** | 同上 |

## 2 为什么必须撤销 CLOSED（证据链）

- **原「闭环」判据** = 现行实现为**值 NaN 经 `F_p` 传播、不掩膜**
  （`docs/science/DRIZZLE.md:116`；`drizzle_engine.cpp:1898-1902`；
  回归 `lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp:517-537` `p1drz_negative`）。
- **该判据把「实现现状」当成了「正确口径」**，与 `NUMERIC_STANDARD.md` 原「禁止 NaN 传播为合法产品」相反
  ——这正是 GAP_AUDIT **V11** 登记的「NaN 契约相反」。
- **EXP-202 定案 = 掩膜**：三数据面（① 纯合成；② HST 真实信号 + 科学生产噪声梯度；③ `testdata` 真实数据）
  + 多轮独立复核，判据非退化（能红能绿）。⇒ 「传播」判据**被推翻**，`DISP-DRZ-004` **必须回到 TRACKED/OPEN**。
- **一致性回归**：`docs/standards/checks/check_standards_registry.py --root .` →
  **本任务前 = `STANDARDS_REGISTRY_FAIL`**（`C7_deviation_index_rows_resolve` 与
  `C9_checklist_deviation_backref` 双红：清单行引 `DISP-DRZ-004` 而 DEVIATION 字段未含）；
  **本任务后 = `STANDARDS_REGISTRY_PASS` / rc=0**（见 §4）。

## 3 唯一口径声明（防「两套文字」）

- 正本 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a 的 `invalid_handling` 块；
- `docs/standards/NUMERIC_STANDARD.md` §MUST 与 `docs/standards/STANDARDS_REGISTRY.md` D.drizzle
  **只引用同一份文字（rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`），不得出现第二套**；
- 三处口径不一致时以 DATA-002 §2a 为准。

## 4 验收证据

| 命令 | 结果 |
|---|---|
| `python3 docs/standards/checks/check_standards_registry.py --root .` | **rc=0 / `STANDARDS_REGISTRY_PASS`**（本任务前 rc=1 / `STANDARDS_REGISTRY_FAIL`） |
| `grep -n 'NAN-SAMPLE-MASK-COVERAGE-NAN' docs/standards/NUMERIC_STANDARD.md docs/standards/STANDARDS_REGISTRY.md docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` | 三处命中，同 rule_id |
| `grep -n 'DISP-DRZ-004' docs/standards/STANDARDS_REGISTRY.md` | DEVIATION 字段 / 清单行 / 偏差表 / §3 索引四处一致（无 CLOSED 残留） |

## 5 影响面与残留

- **不改**任何公式、常数、默认容差、SCI/ALG 冻结定义；`docs/science/DRIZZLE.md:116` 与
  `docs/algorithms/DRIZZLE_GEOMETRY.md` 的旧「传播」措辞**不在 DOC-202 文件域** ⇒ 归 **DOC-205**。
- **机器形态**：`contracts/data/phase_product_exchange.schema.json` 的 `invalid_handling` 键
  **不在 DOC-202 文件域**（`contracts/**`）⇒ 归 **DOC-203**；未同步前以 DATA-002 §2a 为准。
- **代码侧**：`drizzle_engine.cpp` 由「传播」改「掩膜」+ 补 `n_rejected_nonfinite` 暴露 ⇒ 归
  **P1-DRZ-IMPL / FIX 系列**（本 claim 只订正文档侧）。
- **日志**：`run/RELEASE-03/logs/DOC-202-gates-3.log`、`DOC-202-runchecks-*.log`。
