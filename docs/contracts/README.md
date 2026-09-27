# contracts

本目录是 ACSD 数据与接口合同的正本所在，规定数据对象、配置、日志错误、产品工件与对外 API 等契约的字段、语义与登记方式。

## 职责边界

- 放：跨阶段共享的合同条款正文与机读登记（索引、注册表、锚 JSON）。
- 不放：科学公式与算法推导（在 docs/science/ 与 docs/algorithms/）；架构叙述（在 docs/architecture/）；机器校验用 schema 正本（在 eng/contracts/）。

## 内容

- `INDEX.yaml` —— 合同索引，登记每份合同的标识、路径与修订信息。
- `DATA_SEMANTICS.md` —— 数据语义唯一正本，逐 DATA-* 条款与标准块表。
- `DATA_ARTIFACTS.md` —— 产品工件清单与原子发布合同。
- `UNIFIED_OBJECTS.md` —— 统一对象注册表与兼容映射说明。
- `unified_object_registry.json` —— 统一对象注册表的机读形态。
- `API_CONTRACTS.csv` —— 合同条目的表格化清单。
- `PUBLIC_API.md` —— 对外 API 合同（API-* 条款）。
- `CONFIG_CONTRACT.md` —— 三类配置的键表、块结构与默认值合同。
- `LOG_AND_ERROR_CONTRACT.md` —— 日志行格式、run_log 与 manifest 日志字段、错误对象与退出码映射。
- `PIPELINE_BLOCK_CONTRACT.md` —— 阶段内命名块元数据、生命周期状态机与 provenance 流转。
- `SCHEDULER_CONTRACT.md` —— 三阶段调度器统一 entrypoint、DAG 声明与探针事件 schema。
- `PERF_GATE_CONTRACT.md` —— 性能门判据与监控字段的执行语义。
- `HIPS_STORAGE_FORM_CONTRACT.md` —— 落盘形态合同（扩展名、归档容器布局、索引 schema、哈希口径）。
- `DUAL_LINE_CONTRACT.md` —— 双线文件域与合并点规则。
- `TEST_MATRIX.md` —— 合同—测试映射矩阵。
- `API-001.md` —— C ABI 与模块接口合同。
- `ARCH-001.md` —— 架构合同。
- `RT-001.md` —— 运行时合同。
- `config_separation_anchors.json` —— 配置分域锚的机读登记。

## 上游

上游：docs/ASTROCS_DESIGN.md §3.1（数据对象）、§3.2（三类配置）、§7（CLI 合同）、§8（软件架构）、§8.4（模块与 ABI）、§10（I/O 与原子产品）、§12.4（验证层级与四层验收）。
