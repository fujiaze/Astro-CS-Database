# diagnostics

本目录存放故障诊断与处置的细则文档。

## 职责边界

- 放：高频错误的症状—定位—复现—不变量条目化处置说明。
- 不放：错误码与退出码合同（在 docs/contracts/LOG_AND_ERROR_CONTRACT.md）；结构化日志 schema（在 docs/architecture/observability/）；故障排查总入口（在仓库根 TROUBLESHOOTING.md）。

## 内容

- `TROUBLESHOOTING.md` —— 诊断与故障处置细则，按固定条目组织故障场景。

## 上游

上游：docs/ASTROCS_DESIGN.md §7.2（配置、事件与退出码）。
