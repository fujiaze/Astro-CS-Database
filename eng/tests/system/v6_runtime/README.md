# eng/tests/system/v6_runtime — RUNTIME-CI-001 运行面系统测试

本目录是 V6 并行工程包 **RUNTIME-CI-001（Wave 9）** 的系统级收口测试。write_scope =
eng/tests/system/v6_runtime/；根/公共 CMakeLists 注册由控制器在 W9 之后以独立集成提交
完成（CONTROLLER_LOG C-004.4 / C-007 AR-033）。

## 被测单一事实源

- lib/infrastructure/cli/v6_runtime_contract.h —— 统一线程预算（§10.4）、CLI 模式路由、SO-05
  记录/裁决分离（§10.5/§17.6）、三 Phase 隔离（§3.2）、每线程字段面。
- lib/infrastructure/cli/monitor.h + lib/infrastructure/cli/resource_recorder.h —— heavy 运行自动记录（含每线程 CPU
  与 I/O wait）。
- lib/infrastructure/cli/v6_mode_gate.h + lib/infrastructure/cli/commands.cpp —— 显式 --mode / --export-mode
  CLI 选择与 fail-closed 拒绝。

## 构建与运行（自包含）

    cmake -S eng/tests/system/v6_runtime -B run/v6/RUNTIME-CI-001/build -DCMAKE_BUILD_TYPE=Release
    cmake --build run/v6/RUNTIME-CI-001/build -j 8
    ctest --test-dir run/v6/RUNTIME-CI-001/build --output-on-failure

## 用例

| CTest 目标 | 判据 |
|---|---|
| v6_runtime_contract_units | 契约全量正/负向聚合 |
| v6_runtime_mode_routing | FZ-MODE-PRODUCTION / DEFERRED / FIELD-WEIGHTMODE / FZ-P3-MODES |
| v6_runtime_budget_single_source | §10.4 进程唯一预算源 + 租约/超额订阅拒绝 |
| v6_runtime_so05_policy | SO-05 未签字前恒 record_only、绝不硬失败 |
| v6_runtime_phase_isolation | §3.2 禁止聚合/隐式串接入口 |
| v6_runtime_metric_fields | §10.5 必采字段键集合与完备性 |
| v6_runtime_contract_negative | 违规注入逐条被检出 |
| v6_runtime_determinism_positive | 规范归约跨 worker/调度逐字节一致 |
| v6_runtime_determinism_negative | 共享累积（结合序=调度序）必被检出 |

## 相关工具（eng/tools/v6/）

- v6_runtime_oracle.py —— 独立结构 Oracle（源码/配置静态判定，非自证）。
- v6_runtime_mutation_driver.py —— 负向 mutation：违反资源门/预算/确定性/CLI 路由必红。
- v6_determinism_driver.py —— V6 三模式入口 + Phase1/2/3 写盘路径的 taskset 预算逐字节一致回归。
- v6_ctest_driver.py —— 逐名 CTest 门（目标未注册时清晰 FAIL，不静默通过）。
- v6_cli_mode_matrix.py —— 真实二进制 CLI 模式路由矩阵。
- check_v6_runtime_closure.py —— CI 统一入口（closure / cli-mode / resource-gate / mutations / budget）。
