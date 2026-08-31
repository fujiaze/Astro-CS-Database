# CLI-001: CLI 稳定 command 层

状态: **PASS** — HEAD=`d3f0b37`
规则: 拆分 CLI 单体并建立稳定 command 层 (kRules 表驱动; 无硬编码路径分发)。

## Command 层结构
- `cli/main.cpp` (1521 行) 以 `CmdRule kRules[]` 表驱动命令分发 (规则表 = 唯一事实源)。
- 命令函数: cmd_config_init / cmd_config_validate / cmd_show_effective / cmd_stub / cmd_run_pipeline。
- 每个 command 签名统一: `int cmd_*(const Parsed&, astrocs::JsonlEmitter&)` — 稳定接口。
- 解析: parse_args → Parsed (路径/标志/值); 未知路径拒; 缺失值拒。

## 稳定出口
- `--version` → `0.10.0-alpha.2+g<commit>` (版本单源 VERSION)
- `--help` / `-h` → 用法 + 规则表
- 退出码: 0=OK, 非 0=错误 (exit_codes.h 单一来源)

## 验证
- kRules 表驱动分发 (无 argv[1] 硬编码 if 链)
- 每个 rule.path 唯一; allowed flags 白名单
- cmd_* 签名统一 (Parsed + JsonlEmitter)
- CLI 协议测试 (tests/cli/test_cli_protocol.py) 全过
