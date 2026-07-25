# 06 CLI 控制协议 v1

## 1. 目标

未来 GUI/JavaScript 只通过 CLI 协议控制任务。协议必须稳定、可版本化、可测试，不能依赖解析人类日志。

## 2. 进程模式

v1 至少支持：

- 单命令模式：一次进程执行一个 stage1/stage2 job；优先用于批处理和测试。
- 守护/会话模式：后续可选，使用 JSONL stdin/stdout；不得阻塞当前真实数据里程碑。

## 3. stdout/stderr

- stdout：只输出 UTF-8 JSON Lines 机器事件。
- stderr：人类日志，可重定向到文件。
- 禁止 DLL 直接向 stdout 打印调试文本。

## 4. 命令

```text
astrocs stage1 --request request.json
astrocs stage2 --request request.json
astrocs inspect-hiss file.hiss --json
astrocs inspect-hcsd file.hcsd --json
astrocs capabilities --json
```

配置优先级必须固定：CLI 显式参数 > request JSON > project config > built-in default。最终合并配置保存快照与 hash。

## 5. 事件

至少包括：accepted、stage_started、progress、quality_metric、warning、stage_completed、failed、cancelled、completed。Schema 见 `contracts/cli_event_schema_v1.json`。

## 6. 退出码

- 0：完整成功且输出验证通过。
- 2：请求/配置错误。
- 3：依赖或模块能力错误。
- 4：输入数据错误。
- 5：算法失败或质量门限失败。
- 6：输出写入/验证失败。
- 7：用户取消。
- 8：恢复状态不兼容。
- 9：内部异常。

细分错误使用 JSON `error.code`，见注册表。

## 7. 取消与超时

长阶段必须暴露进度或心跳。单命令模式可响应 Ctrl+C/终止事件并写 cancelled 结果；未来会话模式可接收 cancel 命令。所有外部进程、网络/数据库访问和可能阻塞操作必须设置明确超时。

## 8. 安全输出

最终输出先写 `.partial`，验证完成后原子改名。失败时保留或删除 partial 由配置决定，但不得让扫描器把它当作正式 HISS/HCSD。
