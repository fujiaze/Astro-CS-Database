# quality

本目录存放代码质量度量的基线记录：复杂度与覆盖率的首次测量结果。

## 职责边界

- 放：质量基线数字、度量域与工具口径说明。
- 不放：检查器实现（在 eng/tools/quality/）；检查注册（在 eng/ci/checks.json）；质量门禁规则（在 docs/ci/03_GATES.md）。

## 内容

- `complexity_baseline_v1.md` —— 复杂度基线：度量域、工具口径与基线数值。
- `coverage_baseline_v1.md` —— 覆盖率基线：适用 profile、合同约束与基线数值。

## 上游

上游：docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）。
