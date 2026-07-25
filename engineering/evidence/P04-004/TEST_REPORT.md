# P04-004 测试报告

## 测试汇总
- **测试套件**：test_orchestrator_cli.exe（Part 1-9 集成测试）
- **通过**：346
- **失败**：0
- **退出码**：0（成功）

## 测试环境
- 编译器：g++ (MSYS2 MinGW64) -std=c++17 -O2 -fopenmp -static
- 操作系统：Windows 11
- shell：PowerShell 7
- 测试数据：testdata/results/Galaxy_Center_T4/.../01_calibrated.fits（4500×3600 像素）

## Part 9: P04-004 取消/超时/原子性（8 个用例）

### 测试 1: capabilities 包含 cancelled 事件与 TIMEOUT/CANCELLED 错误码
- **命令**：`orchestrator.exe capabilities`
- **验证**：stdout 含 `"cancelled"`、`"name": "TIMEOUT"`、`"name": "CANCELLED"`、`ASTROCS_TIMEOUT`、`ASTROCS_CANCELLED`
- **结果**：PASS
- **证据**：capabilities.stdout.log

### 测试 2: --cancel-on-signal 参数被接受
- **命令**：`orchestrator.exe stage1 --frame nonexistent.fits --output <out> --cancel-on-signal`
- **验证**：exit_code != 0（stage1 失败）；stderr 不含"未知参数"；stderr 含 "cancel-on-signal" 或 "P04-004"
- **结果**：PASS（exit=1，--cancel-on-signal 被接受，信号处理器注册日志输出）
- **证据**：cancel_signal.stdout.log, cancel_signal.stderr.log

### 测试 3: 原子性 - stage1 失败时删除部分输出
- **命令**：预创建假输出文件 → `orchestrator.exe stage1 --frame nonexistent.fits --output <file>`
- **验证**：exit_code != 0；假输出文件被删除（allow_partial_output=false 默认）
- **结果**：PASS（before=True, after=False）
- **证据**：atomic_cleanup.stdout.log, atomic_cleanup.stderr.log, capture_summary.txt

### 测试 4: allow_partial_output=true 保留部分输出
- **命令**：预创建假输出文件 → `orchestrator.exe stage1 --frame nonexistent.fits --output <file> --config <allow_partial_output:true>`
- **验证**：exit_code != 0；假输出文件被保留
- **结果**：PASS（before=True, after=True）
- **证据**：partial_keep.stdout.log, partial_keep.stderr.log, partial_config.json, capture_summary.txt

### 测试 5: request_cancel / is_cancelled / reset_cancel_timeout 单元测试
- **验证**：
  - 初始状态 is_cancelled()=false, is_timed_out()=false
  - request_cancel() 后 is_cancelled()=true, is_timed_out()=false
  - reset_cancel_timeout() 后 is_cancelled()=false, is_timed_out()=false
  - set_stage_timeouts() / set_allow_partial_output() 调用成功
- **结果**：PASS（6 个断言全部通过）

### 测试 6: stage_timeouts 配置解析
- **命令**：`orchestrator.exe stage1 --frame nonexistent.fits --output <out> --config <stage_timeouts:READ_FITS:10.0,...>`
- **验证**：stderr 含 "stage_timeouts" 和 "READ_FITS"
- **结果**：PASS（配置解析日志输出，含 4 个 stage 超时）
- **证据**：timeout_parse.stdout.log, timeout_parse.stderr.log, timeout_config.json

### 测试 7: 超时触发测试（真实 FITS + 0.001s 超时）
- **命令**：`orchestrator.exe stage1 --frame <real.fits> --output <out> --config <stage_timeouts:READ_FITS:0.001>`
- **验证**：exit_code == 9（TIMEOUT）；stderr 含"超时"；输出文件已删除（原子性）
- **结果**：PASS（exit=9, output_exists=False）
- **证据**：timeout_trigger.stdout.log, timeout_trigger.stderr.log, capture_summary.txt
- **关键指标**：READ_FITS duration=0.0649s，watchdog 在 1ms 后检测到超时

### 测试 8: --cancel-on-signal 不影响其他命令
- **命令**：`orchestrator.exe capabilities --cancel-on-signal`
- **验证**：stdout 含 "schema_version"（capabilities 正常输出）
- **结果**：PASS

## 回归测试
- Part 1-8（REPL/CLI/Checkpoint/DLL/Logger/P04-001/P04-002/P04-003）：全部通过，0 退化
- 总断言数：346（Part 9 新增 ~30 个断言）

## 关键 bug 修复验证
### 修复前
- timeout_trigger 测试：exit_code=0（SUCCESS），超时未触发
- 原因：watchdog 100ms 轮询，stage 65ms 完成，watchdog 被停止标志拦截

### 修复后
- timeout_trigger 测试：exit_code=9（TIMEOUT），超时正确触发
- watchdog 自适应轮询：≤100ms 超时用 1ms 轮询，确保短超时被检测

## 测试结论
- **VERDICT: PASS**
- 所有 P04-004 功能（取消/超时/原子性/partial）均通过测试
- 0 退化（Part 1-8 全部通过）
- 关键 bug（watchdog 短超时窗口遗漏）已修复并验证
