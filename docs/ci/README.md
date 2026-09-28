# ci

> 上游：docs/ci/CI_SPEC.md（CI 规范入口与总则）；沿权威链 = docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）

本目录是 CI 规范层：登记全部检查项、流水线 profile、门禁分级与 CI 工件落位，回答每次提交「查了什么、挡不挡、证据在哪」。

## 职责边界

- 放：检查项文档侧登记、流水线与 profile 定义、门禁分级与阻断规则、工件与证据落位规范。
- 不放：检查器实现与注册表（在 eng/ci/）；验收标准（在 ACCEPTANCE_SPEC.md）；追溯矩阵（在 docs/traceability/）。

## 内容

- `CI_SPEC.md` —— CI 规范的入口与总则。
- `01_CHECKS.md` —— 检查项目录（与 eng/ci/checks.json 双向登记）。
- `02_PIPELINE.md` —— CI 流水线与 profile 定义。
- `03_GATES.md` —— 门禁分级与阻断规则。
- `04_ARTIFACTS.md` —— CI 工件与证据落位规范。
