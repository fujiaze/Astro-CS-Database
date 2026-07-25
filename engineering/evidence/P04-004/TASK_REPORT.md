# P04-004 任务报告：取消、超时与 partial 输出

## 任务元数据
- 任务编号：P04-004
- 阶段：P04（CLI 与编排器增强）
- 依赖：P04-002（JSONL 事件与稳定错误码）
- Gate：G4
- 基线 commit：83b0b9c（P04-003）
- 工作树状态：lib/orchestrator/cpp/src/orchestrator.cpp 有未提交修改（+26/-3 行）

## 目标
统一取消 token、stage 超时和原子输出清理，确保：
1. CLI 参数 `--cancel-on-signal` 支持 Ctrl+C 取消
2. 每个 stage 可配置独立超时（秒）
3. 失败/取消/超时时删除部分输出（原子性）
4. 可选保留部分输出（`allow_partial_output=true`）

## 实现内容

### 1. 取消机制（cancel token）
- **OrchestratorConfig** 新增字段：`stage_timeouts`（map）、`allow_partial_output`（bool）
- **Orchestrator** 新增方法：`request_cancel()`、`is_cancelled()`、`is_timed_out()`、`reset_cancel_timeout()`
- **取消 token**：`std::atomic<bool> cancel_token_`，通过 `request_cancel()` 设置
- **信号处理**：`cli_command.cpp` 注册 SIGINT 处理器，Ctrl+C 时调用 `orch->request_cancel()`
- **全局指针**：`g_active_orchestrator` + `g_cancel_on_signal_enabled`，仅 `--cancel-on-signal` 启用时生效

### 2. Stage 超时机制
- **配置解析**：`parse_stage_timeouts()` 从 config JSON 解析 `{"stage_timeouts":{"READ_FITS":10.0,...}}`
- **Watchdog 线程**：每个 stage 启动独立 watchdog 线程，到达 deadline 时设置 `timeout_flag_`
- **自适应轮询**：超时 ≤100ms 时用 1ms 轮询，否则用 100ms 轮询（避免错过短超时窗口）
- **stage 检查点**：每个 stage 开始前和结束后检查 `cancel_token_`/`timeout_flag_`，触发时跳过/中止

### 3. 原子输出清理
- **RAII 守卫**：`AtomicOutputGuard` 析构时检查 `success_flag`，失败时调用 `cleanup_partial_output()`
- **覆盖所有路径**：参数校验失败、DLL 加载失败、stage 失败、取消、超时均触发清理
- **stage1 + stage2**：两个 stage 函数均有 `AtomicOutputGuard`

### 4. Partial 输出
- **配置**：`allow_partial_output=true` 时保留部分输出（`AtomicOutputGuard` 跳过清理）
- **默认 false**：严格原子性，失败时无残留

## 关键 bug 修复

### Watchdog 短超时窗口遗漏（本次修复）
- **症状**：`stage_timeouts.READ_FITS=0.001`（1ms）时，超时未触发，stage1 成功返回（exit=0）
- **根因**：watchdog 固定 100ms 轮询，stage 在 65ms 内完成并设置 `stage_watchdog_stop_=true`，watchdog 醒来后看到停止标志直接返回，未设置 `timeout_flag_`
- **修复**：超时 ≤100ms 时改用 1ms 轮询，确保在 stage 完成前检测到 deadline
- **验证**：修复后 timeout_trigger 测试 exit=9（TIMEOUT），output 已删除（原子性）

## 代码变更

### lib/orchestrator/cpp/include/orchestrator.h（P04-003 已提交）
- `OrchestratorConfig` 新增 `stage_timeouts`、`allow_partial_output`
- `Orchestrator` 新增 `request_cancel()`、`is_cancelled()`、`is_timed_out()`、`reset_cancel_timeout()`
- `set_stage_timeouts()`、`set_allow_partial_output()` 公共接口
- `parse_stage_timeouts()` 静态方法
- `cancel_token_`、`timeout_flag_`、`stage_watchdog_stop_` 原子成员

### lib/orchestrator/cpp/src/orchestrator.cpp（本次修改 +26/-3）
- `parse_stage_timeouts()` 实现
- `cleanup_partial_output()` 实现
- `reset_cancel_timeout()` 实现
- `run_stage1()` / `run_stage2()`：watchdog 线程 + AtomicOutputGuard + 自适应轮询
- **关键修复**：`sleep_ms = (timeout_sec < 0.1) ? 1 : 100`（stage1 + stage2 均修复）

### lib/orchestrator/cpp/include/cli_command.h（P04-003 已提交）
- `cmd_stage1()` / `cmd_stage2()` 新增 `cancel_on_signal` 参数

### lib/orchestrator/cpp/src/cli_command.cpp（P04-003 已提交）
- `--cancel-on-signal` 参数解析
- `p04004_sigint_handler` / `p04004_register_signal_handler` / `p04004_unregister_signal_handler`

### lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp（P04-003 已提交）
- `test_part9_p04_004_cancel_timeout_atomicity()`：8 个测试用例

## 兼容性
- 向后兼容：`--cancel-on-signal` 默认 false，不影响现有命令
- `stage_timeouts` 默认空，不配置超时则无 watchdog
- `allow_partial_output` 默认 false，保持严格原子性

## 回滚方案
- 删除 orchestrator.cpp 中 P04-004 标记的代码段（grep "P04-004"）
- 恢复 orchestrator.h 中 P04-004 新增成员与方法
- 恢复 cli_command.cpp 中 P04-004 信号处理代码
- 恢复 test_orchestrator_cli.cpp Part 9

## 残留风险
- Watchdog 1ms 轮询在极端高负载下可能仍有延迟（best-effort，非硬实时）
- `--cancel-on-signal` 仅在 stage1/stage2 生效，REPL 模式未集成
- 信号处理器中调用 `request_cancel()` 是 async-signal-safe 的（atomic store），但不能调用 LOG（已在 handler 外处理）
