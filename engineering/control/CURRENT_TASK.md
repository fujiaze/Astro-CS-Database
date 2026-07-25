# 当前任务：P04-003 capabilities 与 inspect 命令

读取 `tasks/P04-003.md` 并执行。扩展 capabilities 输出 (版本号/支持模块/schema 版本), 增强 inspect 命令 (配置差异比较/输出格式选项)。

## 上一任务完成情况

- P04-002 JSONL 事件与稳定错误码: DONE (VERDICT: PASS)
  - 证据: evidence/P04-002/
  - 关键变更: jsonl_event_schema.json 扩展 13 种事件 + error_code_registry.csv 新增 12 条错误码 (TIMEOUT/CANCELLED/模块特定码) + orchestrator.h AstroCsExitCode 扩展 + cli_command.cpp output_jsonl_event_ex + cmd_inspect/cmd_request 错误路径 JSONL 事件 + cmd_capabilities 三元组扩展
  - 229/229 测试通过 (Part 1-7, 0 退化)
  - 错误码一致性保证: 进程退出码 == JSONL exit_code == error.numeric_code
  - 双事件并发策略保持向后兼容 (stage_started+stage_start, stage_completed+stage_end, failed+error)
  - 残留: TIMEOUT/CANCELLED 未实际触发 (留待 P04-004); 模块特定码 20-28 未实际使用 (留待 P05+); 嵌套合并保持 P04-001 实现 (GUI 暂无需求)

## P04-003 依赖

- P04-002 (DONE)
- P01-003 (DONE)

## 执行步骤

1. 扩展 capabilities 输出 (版本号/支持模块/schema 版本)
2. 增强 inspect 命令 (配置差异比较/输出格式选项)
3. 更新契约文件 (如有变更)
4. 编写测试验证新功能
5. 回归测试确保 P04-001/P04-002 既有功能未退化

完成独立复核后, 更新状态并进入依赖满足的下一任务。
