# V9 收尾 · P1/P2（机制④门清单 + 旧证据 + 两例"疑红实绿"）

### V9-N-18（P1·机制④四门清单）`CON-CONFIG-CONTRACTS`、`CON-BUILD-GRAPH`、`CON-SCIENCE-UNITS`、`CON-TEST-CONTRACTS`、`CON-DOC-SYMBOLS` 五道门的判据是**字面量/子串 + 人工白名单**
- 要点：`CON-SCIENCE-UNITS` 用**计数阈值 229/42 vs ≥20/≥5**（⇒ 计数一涨门自绿，正对 `E7`「禁字面量钉死在册计数」）；`CON-TEST-CONTRACTS` 有 `synthetic_gate` 兜底 + `TST≥5`；**`CON-DOC-SYMBOLS` 三向 substring + 人工白名单 + `repo.rglob` 含未跟踪影子树**（⇒ 本机与 CI 结论可不同，同 `V9-N-05`）。
- 判据性质：**这些门"绿"不能证明合同一致，只能证明字面量在场**。⇒ 归 `C-16`（禁以文本命中当行为断言）与 `E4`；改法：符号/结构断言（AST、schema、产物字段）。
- **related** `V4-N-16`、`V12-N-02`（截断串在场）、`C-16`、`E7`、`V9-N-05`

### V9-N-19（P2·旧证据不可用）6 个 `DEEP-*` 门的最新证据停在 **2026-09-05T20:48:42Z**，而其中 **两门现已 `waivable=false`**
- 与 `V9-N-14`（同批次 `DEEP-SAN-ASAN`/`DEEP-COV-CPP` 的 `waivable` 由 `true→false` 翻转**无一致性门拦截**）合成一条完整因果：**豁免状态变了，而证据还是九天前那次以"可豁免"身份跑的结果** ⇒ 非豁免门的现状态**无任何有效证据**（它们又只在 schedule 触发的 `linux-deep`）。
- 建议：`waivable` 翻转 ⇒ 强制重跑并作废旧证据（`E12` 建议：门登记项带 `evidence_valid_until`/`evidence_commit` 比对）。**related** `V9-N-14`、`M8-F-004`、`C-19`、`E2`

### V9-N-20（登记为**负结果**，防邻站误修）两例"疑红实绿"，V9 自行纠正后**不立危害条**
- `CON-BUILD-GRAPH`：V9 早期粗口径判 FAIL，**用真实逻辑复算为 PASS**；`CON-COMMENTS` 按其自身口径复算亦 0 findings（其"假绿"成因见 `V9-N-08`，属判据缺陷非红项）。⇒ 记这两条的价值与"判否负清单"相同：**避免邻站花时间修一个不存在的红**。
- **related** `V9-N-08`、`V7-N-09`（同类判否清单）、纪律⑧
