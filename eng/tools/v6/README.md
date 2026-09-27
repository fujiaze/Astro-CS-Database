# v6

V6 运行面验证工具族：运行时闭环 Oracle、确定性回归、数值等价比对、负向 mutation 与 CLI 模式矩阵。

## 职责边界

- 放：运行面结构 Oracle 与收口入口、跨 CPU 预算写盘等价驱动、冻结容差数值比对、负向注入驱动、真实二进制模式矩阵、CTest 按名验收。
- 不放：被检查的运行面实现（在 `lib/infrastructure/`）、检查项注册表（在 `eng/ci/checks.json`）、C++ 契约测试本体（在 `eng/tests/`）。

## 内容

- `check_v6_runtime_closure.py` —— 收口 CI 入口，按子命令编排 Oracle / CLI 模式 / 资源门 / mutation / budget 自测，输出 JSON 证据，任一失败非零退出。
- `v6_runtime_oracle.py` —— 独立结构 Oracle（R1–R8 硬规则）：模式路由表唯一落位、三 Phase 隔离、统一预算登记、必采字段面、确定性契约键。
- `v6_determinism_driver.py` —— 确定性驱动：同一输入在不同 CPU 预算下重复执行写盘测试，先逐字节比对，不同时转数值容差比对。
- `v6_numeric_equiv.py` —— 数值等价判据：identical / within_tolerance / differs 三档，整数与 mask、NaN 位置精确一致，浮点按冻结容差。
- `v6_runtime_mutation_driver.py` —— 负向 mutation 驱动：临时树先跑干净 Oracle 必须绿，逐条注入违规必须红，漏检即驱动失败。
- `v6_cli_mode_matrix.py` —— CLI 模式路由矩阵（真实二进制端到端）：拒绝集与放行集逐 token 断言退出码与拒绝理由。
- `v6_ctest_driver.py` —— CTest 门驱动：按名验收期望目标，目标缺失清晰判红而非静默通过。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-CONTRACT-TEST`（`V6-NEGATIVE-MUTATION`、`V6-CTEST-INTEGRATION`）、`CHK-UNIT`（`V6-CTEST-UNIT`）、`CHK-ORACLE`（`V6-RUNTIME-CLOSURE`）、`CHK-SYNTH-P2`（`V6-CLI-MODE-ROUTING`、`V6-CLI-MODE-MATRIX`）、`CHK-NWORKER`（`V6-DETERMINISM`）、`CHK-NWORKER-TOLERANCE`、`CHK-RESOURCE`（`V6-RESOURCE-GATE-POLICY`）。
- 检查项条目见 `docs/ci/01_CHECKS.md`，门禁分级见 `docs/ci/03_GATES.md`。
