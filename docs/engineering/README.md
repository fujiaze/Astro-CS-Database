# 工程正本

`docs/engineering/` 承载 ACSD 的架构、行为合同与工程标准，是权威链的一级正本，与
`docs/science/`（公式、常数、判据、推导）同级。给实现代码、做审核与验收的开发者阅读。

## 目录

| 路径 | 内容 |
|---|---|
| `UNIFIED_OBJECTS.md` | 顶层正本：13 个统一数据对象到 canonical schema 的逐对象合同 |
| `architecture/` | 系统架构、数据流与执行模型、模块地图 |
| `contracts/` | 行为合同：命令行、配置、日志与错误、调度器、命名块、落盘形态、异步 I/O、所有权、原子发布、运行清单校验、运行时公共面 |
| `api/` | 对外与跨库接口：`api/PUBLIC_API.md` 与 `abi/` 的 C ABI 基础层与安全装载器 |
| `standards/` | 工程标准：代码（`CODE.md`）、注释（`COMMENT.md`）、数值、并发、缓存、兼容性、优化、文档、依赖 |
| `resources/` | 性能模型与资源判据、benchmark、CPU 变体与后端、可观测性 |
| `build/` | 构建图、构建节点与工具链、发布与版本 |
| `testing/` | 测试标准与容差规则、验证证据标准与四层验收判据 |
| `data/` | 数据工件合同、阶段间产品交换、产物库、产品溯源 |
| `governance/` | 文档治理、追溯、双线文件域、未决登记 |

## 阅读顺序

先读 `../ACSD_DESIGN.md`，再读 `architecture/ARCHITECTURE.md` 建立全局视图，然后按你要改的
行为面进 `contracts/`，按你要验证的面进 `testing/`。公式与容差来源一律回 `docs/science/`，
落地细节回 `docs/detail/`。

## 两条写作口径

- 文档之间与文档对外部文献的引用统一用数字编号加文末参考文献，不使用「见第几节」式的跳转锚。
- 文档随代码持续维护，只写现行设计，不写沿革叙事。