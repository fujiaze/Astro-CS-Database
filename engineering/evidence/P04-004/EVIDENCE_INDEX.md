# P04-004 证据索引

## 证据清单

| 文件 | 描述 | 关键验证点 |
|------|------|-----------|
| TASK_REPORT.md | 任务报告 | 实现内容、代码变更、兼容性、回滚方案 |
| TEST_REPORT.md | 测试报告 | 346/346 通过，8 个 P04-004 用例，0 退化 |
| REVIEW_REPORT.md | 独立复核报告 | VERDICT: PASS |
| cancel_timeout_impl.json | 实现细节 JSON | 接口、配置、错误码、机制参数 |
| capture_evidence.ps1 | 证据捕获脚本 | 可复现的证据收集流程 |
| capture_summary.txt | 证据捕获汇总 | 6 个场景的 exit_code 与文件状态 |
| test_full_output.log | 完整测试输出 | 346 通过, 0 失败 |

## 命令输出证据

| 文件 | 命令 | exit_code | 验证点 |
|------|------|-----------|--------|
| capabilities.stdout.log | `orchestrator.exe capabilities` | 0 | 含 cancelled/TIMEOUT/CANCELLED/ASTROCS_TIMEOUT/ASTROCS_CANCELLED |
| capabilities.stderr.log | 同上 | 0 | 日志输出 |
| cancel_signal.stdout.log | `orchestrator.exe stage1 --frame nonexistent.fits --cancel-on-signal` | 1 | --cancel-on-signal 被接受 |
| cancel_signal.stderr.log | 同上 | 1 | 含 P04-004 启用日志 |
| atomic_cleanup.stdout.log | `orchestrator.exe stage1 --frame nonexistent.fits --output <file>` | 1 | stage1 失败 |
| atomic_cleanup.stderr.log | 同上 | 1 | 原子清理日志 |
| partial_keep.stdout.log | `orchestrator.exe stage1 ... --config <allow_partial_output:true>` | 1 | stage1 失败 |
| partial_keep.stderr.log | 同上 | 1 | 部分输出保留 |
| partial_config.json | 配置文件 | - | `{"allow_partial_output":true,"log_level":"ERROR"}` |
| timeout_parse.stdout.log | `orchestrator.exe stage1 ... --config <stage_timeouts:...>` | 1 | 配置解析 |
| timeout_parse.stderr.log | 同上 | 1 | 含 stage_timeouts/READ_FITS 日志 |
| timeout_config.json | 配置文件 | - | `{"stage_timeouts":{"READ_FITS":10.0,...}}` |
| timeout_trigger.stdout.log | `orchestrator.exe stage1 --frame <real.fits> --config <READ_FITS:0.001>` | 9 | TIMEOUT 触发, error_msg="READ_FITS 超时" |
| timeout_trigger.stderr.log | 同上 | 9 | 含 watchdog 超时日志, 原子清理日志 |
| timeout_trigger_config.json | 配置文件 | - | `{"stage_timeouts":{"READ_FITS":0.001}}` |

## 证据捕获汇总（capture_summary.txt）
```
capabilities exit=0
cancel-on-signal exit=1
atomic cleanup exit=1 before=True after=False
allow_partial exit=1 before=True after=True
timeout_parse exit=1
timeout_trigger exit=9 (9=TIMEOUT) output_exists=False
```

## 复现步骤
1. 编译：`cd lib/orchestrator/cpp && mingw32-make -f Makefile && mingw32-make -f Makefile test_orchestrator_cli`
2. 测试：`.\tests\test_orchestrator_cli.exe`
3. 证据：`pwsh -File engineering/evidence/P04-004/capture_evidence.ps1`
