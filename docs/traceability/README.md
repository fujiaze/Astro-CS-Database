# traceability

本目录存放八层追溯矩阵的机器合同与矩阵数据，把科学定义、算法、合同、架构、模块、实现、测试串成可机检的追溯链。

## 职责边界

- 放：追溯 ID 格式与跨层链规则、逐模块矩阵数据（CSV/JSON）、层级词表与告警基线。
- 不放：矩阵的检查器实现（在 eng/tools/ 与 eng/ci/）；追溯总纲与验证层级条款（在 docs/ASTROCS_DESIGN.md §12.4）。

## 内容

- `TRACEABILITY_SPEC.md` —— 追溯矩阵的机器合同：ID 格式、唯一性、跨层关系与 schema。
- `TRACEABILITY_LAYERS.csv` —— 层级词表：每层的列组、状态取值与必填范围。
- `TRACEABILITY_MATRIX.csv` —— 逐模块八层追溯矩阵的表格形态。
- `TRACEABILITY_MATRIX.json` —— 同一矩阵的机读形态。
- `traceability_warn_baseline.json` —— 追溯检查告警面的基线登记。

## 上游

上游：docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）。
