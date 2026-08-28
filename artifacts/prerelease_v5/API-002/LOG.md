# API-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS API-002 行(命令树/schema v1/exit codes/JSONL events/cancel-crash;验收=golden help+schema+exit tests 双平台源一致); 控制包 04(79 行 6 节); API-001(acs_cancel); ARCH-002(单入口)。

## 动作
1. 新建 docs/api/CLI_PROTOCOL_V1.md(API-CLI-001): §1 命令树 13 条逐条落地+handler→Phase API 追溯(phase1→API-003/phase2→API-004/phase3→API-005/synthetic→SYN/benchmark→BENCH harness)+禁另发 benchmark exe; §2 退出码 11 条冻结+唯一源 exit_codes.h; §3 stdout/stderr 纪律(--json 恰一文档/--events-jsonl 每行一事件/stdout 无日志污染); §4 JSONL v1 十必含字段+五 kind 扩展字段+sequence 单调+GUI 只消费协议; §5 取消崩溃(acs_cancel 令牌+incomplete manifest+不留伪完整产物+70 脱敏 crash report); §6 04 §6 六条 checker 合同(1-5 Linux 可验/6 属 WIN-FAT)。
2. 权威声明: 与 04 冲突以 04 为准(文件头); 04 §1 命令逐条交叉引用。
3. 机器门 tests/api/test_cli_protocol.py 6 用例: 命令树覆盖 04 全 13 条/退出码 11 条+唯一源/JSONL 字段冻结/取消崩溃语义/06 六条 checker 合同/04 权威交叉核对。

## 验证
- 全量回归 unittest **58/58 OK**(新增 6)。

## 产物
docs/api/CLI_PROTOCOL_V1.md; tests/api/test_cli_protocol.py; 本日志。

## PASS 判定
命令树/退出码/JSONL/cancel-crash 全部与 04 逐条一致并声明权威序; handler 追溯到 Phase API; 机器化一致性检查器合同立约(CLI-002 golden 落地)。API-002 = PASS。
