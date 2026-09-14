# V9 片3-4 · P2 两条（列入"永不红"清单，不算疏漏但要计账）

### V9-N-11（P2·设计即静默）`API-DOCS`、`UNIT-CLOSURE` 在 `ci/run.py:103 EMPTY_OUTPUT_SILENCE_EXEMPT` 名单内 ⇒ **exit 0 且 stdout/stderr 全空仍记 PASS**
- 这条是**显式登记的设计**，不算邻站疏漏（V9 原话，我保留），但必须计入"永不红"清单：**空输出即绿 = 没有任何东西被证明**。改法：从名单移除，或要求两门输出结构化计数并以 `A-38` 的裁定决定 0 项是否合法。**related** `FD-F-003`、`V15-N-13`（产物 isfile 不判内容）、`A-38`

### V9-N-13（P2·`--focus`/`--changed-from` 类 CI 口径漏门）`changed_paths` **99/130 门不覆盖其自身 checker 脚本路径**、**124/130 不含 `ci/checks.json`**
- 例：`CON-API-CONTRACTS` 的 paths = `docs/**,lib/**,cli/**,tests/**,contracts/**` 却**不含 `tools/quality/**`**；**全部 39 道 `CTEST-*` 与 21 道 `UT-*` 同样不含 `tools/**`**；只有 6 门覆盖登记面（`WIN-CANDIDATE-VALIDATE`、`WORKFLOW-REGISTRY-BINDING`、`CI-BINDING-TESTS`、`CTEST-REGISTRATION`、`KNOWN-FAILURES-BASELINE-VERIFY`、`KNOWN-FAILURES-BASELINE-CHECK`）。
- **限定（V9 自限，我照收）**：CI 实跑用 `--profile`（`ci-linux.yml:141`、`ci-windows.yml:110` 均不带 `--changed-from`）⇒ **本条不造成 CI 漏门，只让 `--focus`/`--changed-from` 两种类 CI 口径漏门** ⇒ 判 P2。**但改 checker 而不改 `changed_paths` = 定向复跑看不到该门的新行为**，与 `V9-N-12` 合成同一个自证漏洞。表见 `V9_gates_table.md`（130 行逐门，机器生成、可复跑 `scripts_v9_table.py`）。
- **related** `V9-N-12`、`V9-N-14`、`C-19`
