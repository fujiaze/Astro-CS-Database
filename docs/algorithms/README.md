# algorithms（行号锚设施）

本目录承载算法文档的机器锚设施：`anchors/`（锚合同、检查器、未解析登记）把
[docs/science/algorithms/](../science/algorithms/) 的算法文档逐符号锚定到 `lib/` 实现，
由 `eng/ci/checks.json` 的锚门在 CI 中复验。

## 职责边界

- 放：锚合同（`anchor_contract.json`）、锚检查器与契约文档、未解析锚登记。
- 不放：算法正文文档（在 [docs/science/algorithms/](../science/algorithms/)）、
  通用文档检查器（在 `eng/tools/doccheck/`）。
