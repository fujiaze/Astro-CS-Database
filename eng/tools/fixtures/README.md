# fixtures

配置一致性检查器的已登记差异台账：承载已审计且尚未修复的默认值不符条目。

## 职责边界

- 放：差异台账数据文件，条目含 kind/reason/owner/audit_ref/exit_condition 五字段。
- 不放：检查器实现（在 `eng/tools/config_consistency_check.py`）、其他域的台账（在 `eng/ci/ledgers/`）、豁免登记（在 `eng/ci/exemptions.json`）。

## 内容

- `config_consistency_known_divergences.json` —— 已登记差异台账：未登记差异一律判红、登记项必须逐条可复现（不再复现即判台账失效）、字段缺一即退出码 2、`--strict` 忽略台账以暴露全部差异；差异修复后条目同步删除。

## 上游

- 本目录数据文件未注册于 `eng/ci/checks.json`（由其消费检查器在运行时读取）。
- 台账 fail-closed 语义见 `docs/engineering/01_CHECKS.md` §1 注册表原则，门禁分级见 `docs/engineering/03_GATES.md`。
