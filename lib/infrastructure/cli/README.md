# cli（命令行入口 / infrastructure）

## 1. 职责与边界

AstroCS **唯一可执行入口** `astrocs`（Windows 交付名 `ACSD Cli.exe`）的命令编排层：

- **职责**（薄入口，ASTROCS_DESIGN §6.1）：命令解析、配置预检、模板生成、运行确认、
  机器输出（`--json` 下 stdout 恰一个 JSON 文档）、JSONL 事件、取消与退出码；
- **非职责**：科学公式（唯一家在 `lib/algorithms/`）、FITS/HiPS 读写（`infrastructure/aio`）、
  线程池（scheduler/runtime）。本目录**不实现**任何科学计算，只把已校验的配置交给
  `lib/infrastructure/cli/runtime_client`（CLI runtime client）→ Runtime/pipeline。

## 2. 命令树（唯一，ASTROCS_DESIGN §6.2）

```text
normalize --json <config.json> | --template [-o <path>] | --help
mosaic    --json <config.json> | --template [-o <path>] | --help
export    --json <config.json> | --template [-o <path>] | --help
help / --version / doctor / benchmark
```

三个命令**平级独立**：各自独立进程、独立恢复、独立验收；**禁止**隐式串接
（ASTROCS_DESIGN §1.2）。`normalize`/`mosaic`/`export` 与内部会话阶段
（`phase_scope`）的对应是**机械映射**，只用于路由到既有会话实现，外部命令名
与用户可见输出一律使用 `normalize`/`mosaic`/`export`。

## 3. 落位

| 路径 | 内容 |
|---|---|
| `command_tree.h` | 唯一命令树表（命令名 → 会话 + 旗标白名单）+ `help` 文本生成 |
| `session_commands.h` | 会话路由 + 会话侧旗标名 + 配置模板（`--template`）|
| `normalize/` | `normalize` 子命令入口（薄适配：预检 + 确认 + 委托会话）|
| `mosaic/` | `mosaic` 子命令入口 |
| `export/` | `export` 子命令入口 |

用户可见命令名/旗标的唯一事实源是 `command_tree.h`；根 `lib/infrastructure/cli/parser.cpp` 只从该表
取白名单，不再自带命令清单。构建接线（根 `CMakeLists.txt` 的 `astrocs` target）
由 INT-001 登记；本层当前以头文件形式被 `lib/infrastructure/cli/commands.cpp` 消费。

## 4. 测试

- `eng/tools/check_cli_command_layer.py`（机器检查，含负例）：命令树完整 + 旧命令 rc=2 + 退出码稳定；
- `eng/tests/cli/test_command_tree.py`：rc 矩阵与平级独立性实测；
- `eng/tests/cli/test_cli_build.py`：单 target 构建 + `--help` golden。
