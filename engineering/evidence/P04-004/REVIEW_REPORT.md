# P04-004 独立复核报告

## 复核元数据
- 任务：P04-004 取消、超时与 partial 输出
- 复核日期：2026-07-25
- 基线 commit：83b0b9c（P04-003）
- 复核范围：lib/orchestrator/cpp/（orchestrator.h/cpp, cli_command.h/cpp, test_orchestrator_cli.cpp）

## 复核检查项

### 1. 取消机制（cancel token）
- [x] `OrchestratorConfig` 含 `stage_timeouts` 和 `allow_partial_output` 字段
- [x] `request_cancel()` / `is_cancelled()` / `is_timed_out()` / `reset_cancel_timeout()` 方法存在
- [x] `cancel_token_` / `timeout_flag_` / `stage_watchdog_stop_` 使用 `std::atomic<bool>`
- [x] `--cancel-on-signal` 参数在 stage1/stage2 中正确解析
- [x] SIGINT 处理器仅在 `--cancel-on-signal` 启用时注册，命令完成后注销
- [x] 信号处理器中仅调用 async-signal-safe 操作（atomic store）
- [x] `g_active_orchestrator` / `g_cancel_on_signal_enabled` 全局原子指针

**结论**：PASS

### 2. Stage 超时机制
- [x] `parse_stage_timeouts()` 正确解析 JSON `{"stage_timeouts":{"STAGE":seconds,...}}`
- [x] watchdog 线程在 stage 开始前启动，deadline = now + timeout_sec
- [x] watchdog 到达 deadline 时设置 `timeout_flag_`（检查 cancel/stop 避免误触发）
- [x] stage 结束后设置 `stage_watchdog_stop_=true` 并 join watchdog（无线程泄漏）
- [x] stage 开始前/结束后检查 `is_cancelled()` / `is_timed_out()`，触发时中止
- [x] **自适应轮询**：`sleep_ms = (timeout_sec < 0.1) ? 1 : 100`（stage1 + stage2 均实现）
- [x] 超时触发时 `result.exit_code = TIMEOUT(9)`，`result.error_msg = "STAGE 超时"`

**结论**：PASS

### 3. 原子输出清理
- [x] `AtomicOutputGuard` RAII 守卫在 stage1 和 stage2 中均存在
- [x] 析构条件：`!success_flag && !allow_partial && !path.empty()`
- [x] `cleanup_partial_output()` 删除文件，文件不存在视为成功
- [x] 覆盖所有失败路径：参数校验、DLL 加载、stage 失败、取消、超时
- [x] `current_output_file_` 在 stage 开始时设置

**结论**：PASS

### 4. Partial 输出
- [x] `allow_partial_output` 默认 false（严格原子性）
- [x] `orc_getJsonBool(config_json, "allow_partial_output", false)` 解析
- [x] `AtomicOutputGuard` 构造时传入 `config_.allow_partial_output`
- [x] `set_allow_partial_output()` 公共接口可供单元测试

**结论**：PASS

### 5. 错误码一致性
- [x] TIMEOUT(9) / CANCELLED(10) 错误码在 capabilities 中声明
- [x] ASTROCS_TIMEOUT / ASTROCS_CANCELLED 字符串码在 capabilities 中声明
- [x] `cancelled` 事件类型在 capabilities events 中声明
- [x] stage1/stage2 失败时返回对应错误码（TIMEOUT=9, CANCELLED=10）

**结论**：PASS

### 6. 测试覆盖
- [x] Part 9 包含 8 个测试用例，覆盖所有 P04-004 功能
- [x] 测试 1: capabilities 含 cancelled/TIMEOUT/CANCELLED
- [x] 测试 2: --cancel-on-signal 被接受
- [x] 测试 3: 原子性 - 失败时删除部分输出
- [x] 测试 4: allow_partial_output=true 保留部分输出
- [x] 测试 5: request_cancel/is_cancelled/reset_cancel_timeout 单元测试
- [x] 测试 6: stage_timeouts 配置解析
- [x] 测试 7: 超时触发（真实 FITS + 0.001s 超时）
- [x] 测试 8: --cancel-on-signal 不影响其他命令
- [x] 346/346 通过，0 退化

**结论**：PASS

### 7. 关键 bug 修复验证
- [x] **症状**：短超时（1ms）未触发，stage 成功返回 exit=0
- [x] **根因**：watchdog 100ms 轮询，stage 65ms 完成，watchdog 被停止标志拦截
- [x] **修复**：自适应轮询（≤100ms 超时用 1ms 轮询）
- [x] **验证**：修复后 exit=9（TIMEOUT），output 已删除（原子性）
- [x] **stage2 同步修复**：确认 stage2 watchdog 也使用自适应轮询

**结论**：PASS

### 8. 代码质量
- [x] 所有 P04-004 代码用 `// P04-004:` 注释标记，便于追溯
- [x] 日志使用 LOG_INFO/LOG_WARN，符合日志规范
- [x] 无 hardcoded magic number（超时阈值 0.1s 有注释说明）
- [x] 线程安全：atomic + mutex 保护共享状态
- [x] RAII 模式确保异常安全

**结论**：PASS

### 9. 兼容性
- [x] `--cancel-on-signal` 默认 false，不影响现有命令
- [x] `stage_timeouts` 默认空，不配置则无 watchdog
- [x] `allow_partial_output` 默认 false，保持严格原子性
- [x] Part 1-8 全部通过，0 退化

**结论**：PASS

## 复核结论

**VERDICT: PASS**

所有 P04-004 目标已实现并通过测试：
1. 统一取消 token（cancel_token_ + --cancel-on-signal）
2. Stage 超时（watchdog 自适应轮询 + timeout_flag_）
3. 原子输出清理（AtomicOutputGuard RAII）
4. Partial 输出（allow_partial_output 配置）

关键 bug（watchdog 短超时窗口遗漏）已发现并修复，修复后所有测试通过。
