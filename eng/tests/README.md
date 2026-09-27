# tests

本目录是仓库的测试之家：按主题分目录的测试套件与套件索引在这里，供 ctest 与机器门调用。

## 职责边界

- 放：单元测试、合同负例、集成测试、科学 Oracle、追溯与 lint 检查等套件，套件索引 test_index.csv 与公共测试设施 testkit/。
- 不放：生产代码（lib/）、门禁注册表（eng/ci/）、合同 schema 正本（eng/contracts/）、验收证据（artifacts/）。
- 断言口径按测试规范与科学测试矩阵执行；测试与被测模块共址时可复用其代码。

## 内容

- unit/ —— C++ 单元测试（算法、AIO、管线、块流等）。
- contracts/ —— 合同负例与统一对象合同测试。
- integration/ —— 分阶段集成测试（p1_integrate、p2_integrate、p3_export）。
- oracle/ —— 科学 Oracle（gaia_oracle.py 等）。
- sciencelint/ —— 科学口径一致性检查测试。
- traceability/ —— 追溯矩阵与追溯性测试。
- testkit/ —— 测试工具包：注册表、schema、fixtures 与 testkit.spec.md。
- test_index.csv —— 各套件的运行命令、用例数与结论索引。
- 其余主题套件：abi/、api/、arch/、artifact/、backend/、cli/、common/、config/、conformance/、cpu/、glossary/、io/、monitoring/、pipeline/、quality/、realdata/、results/、runtime/、system/、validation/、version/。

## 上游

上游：ENGINEERING_SPEC.md §5（测试规范）；docs/contracts/TEST_MATRIX.md（科学测试矩阵）；docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）。