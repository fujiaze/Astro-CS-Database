# standards

本目录存放各领域的编码与工程标准，以及外部国际标准的冻结注册表。

## 职责边界

- 放：标准正文（API、C ABI、代码、注释、并发、数值、I/O、错误处理、日志、测试、基准、发布、文档）与标准注册表。
- 不放：科学公式（在 docs/science/）；合同条款（在 docs/contracts/）；工程总规范（在 ENGINEERING_SPEC.md）；注册表检查器（在 checks/ 子目录）。

## 内容

- `STANDARDS_REGISTRY.md` —— 领域到国际标准、版本、条款、符合性与偏差指针的冻结注册表。
- `API_STANDARD.md` —— 公共 API 标准（C ABI 边界与注释要求）。
- `C_ABI_STANDARD.md` —— C ABI 编码标准。
- `CODE_STANDARD.md` —— 代码结构与组织标准。
- `COMMENT_STANDARD.md` —— 注释标准。
- `CONCURRENCY_STANDARD.md` —— 并发标准。
- `NUMERIC_STANDARD.md` —— 数值标准（量纲、标度、精度与容差口径）。
- `IO_STANDARD.md` —— I/O 标准。
- `ERROR_HANDLING_STANDARD.md` —— 错误处理标准。
- `LOGGING_DIAGNOSTICS_STANDARD.md` —— 日志与诊断标准。
- `DOCUMENTATION_STANDARD.md` —— 文档标准。
- `TEST_STANDARD.md` —— 测试标准。
- `BENCHMARK_STANDARD.md` —— 基准测试标准。
- `RELEASE_STANDARD.md` —— 发布标准。
- `checks/` —— 注册表的机器校验脚本。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（模块与 ABI）、§5.3（冻结投影集合）、附录 B（外部标准与文献）、ENGINEERING_SPEC.md §8（文档集与机器一致性检查）。
