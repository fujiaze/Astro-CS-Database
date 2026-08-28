# 控制包内容清单

## 入口与施工

- `START_PROMPT.txt`：启动提示词。
- `00_READ_FIRST.md`：硬约束、状态机、停止条件。
- `01_PRODUCT_ARCHITECTURE.md`：单一 CLI 与私有 backend 产品架构。
- `02_TASK_LEDGER.csv`：98 个有序原子 Task 的唯一状态源。
- `03_TASK_DETAILS.md`：每个 Task 的动作、产物、测试和 PASS 条件。
- `15_CONTINUOUS_CHECKPOINTS.md`：C0--C9 连续机器检查点及控制/审核循环。

## 专项合同

- `04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md`
- `05_CPU_BACKEND_ABI_AND_PACKAGING.md`
- `06_BENCHMARK_AND_PROFILE_SPEC.md`
- `07_RESOURCE_MONITOR_AND_UTILIZATION_GATE.md`
- `08_SCIENCE_SYNTHETIC_AND_EXTERNAL_REVIEW.md`
- `09_LINUX_WINDOWS_BUILD_RELEASE.md`
- `10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md`
- `11_AGENTS_MD_REQUIRED_BLOCK.md`
- `12_V3_V4_MIGRATION.md`
- `13_ALPHA_VERSION_AND_PHASE3.md`
- `14_V4_COVERAGE_MATRIX.md`

## 机器资产

- `schemas/`：cpu profile 与 CLI event v1 schema。
- `templates/`：commit、finding、build/test/profile/resource、traceability、science claim、review capsule、release/checkpoint、summary 表。
- `scripts/validate_control.py`：控制包结构、依赖和关键概念检查。
- `scripts/package_final.py`：最终审核包白名单选择。
- `scripts/validate_final_package.py`：大小、类型、hash、ledger、build/test/resource、alpha 双平台与 verdict 门禁。

`SHA256SUMS` 对除自身外的全部文件生成，ZIP 根目录必须是本目录名，不得夹带历史包或上传数据。

