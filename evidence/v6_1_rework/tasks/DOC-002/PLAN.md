# DOC-002: 重建当前版本追溯矩阵

任务 ID: DOC-002
Gate: G7
依赖: RT-009; P1-004; P2-007; P3-006
平台: Linux
变更类别: documentation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-002：

> 矩阵必须从当前合同、public AST、registry、CMake、test registry 和现场 evidence
> 生成。工具无默认 V5 路径，不硬编码 claim 计数。每行真实检查 doc anchor、header
> symbol、implementation symbol、test executable/case、oracle、evidence hash。
> 故意改函数名/删除测试/换单位均失败。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 矩阵从当前源生成 | docs/TRACEABILITY.csv 63 行(requirement/algorithm/module/api/impl/test/evidence 列) | c01 |
| 每行真实检查 symbol/test/evidence | check_traceability PASS(63 行全 ok, symbols 13/13, 0 断链) | c01 |
| 无 V5 默认/硬编码计数 | check_final_traceability PASS(66 claims, VERSION 单源, 旧入口标退出) | c02 |
| DATA artifact 一致性 | check_data_artifacts PASS(18 schemas) | c03 |

## 检查命令

- c01: check_traceability.py → rows=63 ok=63 broken=0 symbols=13/13
- c02: check_final_traceability.py → 66 claims, VERSION 单源, phase3 真实
- c03: check_data_artifacts.py → DATA_ARTIFACTS_PASS schemas=18

## 测试结果

- 3 项检查全 PASS(追溯矩阵/最终追溯/DATA artifact)

## 说明

- 追溯矩阵在 R0 系列已从当前合同+AST+registry+test 生成; 本任务复验当前 SHA
  (含 G6 全部变更) 的一致性。
- 无生产代码变更, 仅证据收集与台账登记。
