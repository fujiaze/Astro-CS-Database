# validation

本目录存放验证体系的设计档案：科学冻结面登记与 QA 矩阵产品族（位于子目录 v6）。

## 职责边界

- 放：科学冻结面与三重佐证登记、QA 矩阵及配套政策/目录文档（v6/ 子树）。
- 不放：CI 检查与门禁执行规范（在 docs/ci/）；追溯矩阵（在 docs/traceability/）；验收标准正文（在 ACCEPTANCE_SPEC.md）。

## 内容

- `SCIENCE_FREEZE.md` —— 科学冻结面与三重佐证登记。
- `v6/` —— 科学 QA 矩阵产品族：门规格、基线比较、负向 mutation 目录、Oracle 与零用例政策、P0 门族账本及其索引。

## 上游

上游：docs/ASTROCS_DESIGN.md §12（验证体系）、§12.1（科学正确性与三重佐证）、§12.5（状态阶梯）。
