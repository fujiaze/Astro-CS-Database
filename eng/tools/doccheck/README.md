# doccheck

文档一致性检查器族：索引双向闭合、行号锚存活、过程痕迹、工程约束与规范极性一致。

## 职责边界

- 放：`docs/**` 与根文档的机器检查器，及其台账与豁免登记数据。
- 不放：文档正文（在 `docs/`）、检查项注册表（在 `eng/ci/checks.json`）、追溯矩阵（在 `eng/tools/traceability/`）。

## 内容

- `check_doc_index.py` —— 双向层级索引闭合门：索引结构与覆盖、根指针可达、下级文档「上游」抬头全覆盖、`docs/` 路径引用可解析，附 `--self-test` 负例面。
- `check_alg_line_anchors.py` —— 文档行号锚与源文件实测比对，锚漂移即判红。
- `check_doc_hygiene.py` —— 正式文档过程痕迹门与已知限制台账 ID 可解析门，附 `--self-test`。
- `check_engineering_constraints.py` —— 工程规范条文约束检查。
- `check_frozen_string_disambiguation.py` —— 冻结串在 schema 常量、登记表与示例三处逐字一致及消歧说明在位。
- `check_spec_polarity_consistency.py` —— 规范正负极性表述跨文档一致。
- `check_test_index_live.py` —— 测试索引与在库测试一致。
- `check_version_namespaces.py` —— 版本与命名空间声明一致。
- `dangling_ledger.json` —— 跨域未修引用台账，条目只减不增。
- `doc_fact_authority.json` —— 事实键唯一正本登记。
- `doc_hygiene_exempt.json` —— 过程痕迹门的登记豁免面。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`DOC-INDEX`、`DOC-INDEX-SELFTEST`、`ALG-LINE-ANCHORS`、`CHK-DOC-HYGIENE`、`CHK-DOC-HYGIENE-SELFTEST`、`CHK-FROZEN-STRING-DISAMBIG` 及其 `-SELFTEST`、`ENG-CONSTRAINTS`、`VERSION-NAMESPACES`、`SPEC-POLARITY-CONSIST-01` 及其 `-SELFTEST`、`CHK-UNIT` 步骤 `TEST-INDEX-LIVE`。
- 检查项条目见 `docs/engineering/01_CHECKS.md`，门禁分级见 `docs/engineering/03_GATES.md`。
