# 插件文档：cli（命令行入口）

> 上游：ASTROCS_DESIGN.md §7.1（命令树）、§7.2（配置、事件与退出码）

## 1. 职责与边界

- **职责**：三个平级命令（`normalize` / `mosaic` / `export`）的入口：命令解析、配置校验、检查页面（绿/橙/红 + yes 确认）、模板生成、帮助、机器输出、取消与退出码；科学公式实现在 `lib/algorithms/` 各模块，FITS/HiPS 读写由 `infrastructure/aio` 承担，线程池由 scheduler/runtime 统一管理；**薄入口**。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §6（CLI 合同）、§4.5（运行前预检）
- `eng/contracts/schemas/phase_config*.schema.json`

## 3. 输入/输出数据合同

- **输入**：命令行参数 + `--json` 挂载的 phase_config。
- **输出**：stdout 机器 JSON（`--json` 时恰一个 JSON 文档）、JSONL 事件、退出码；运行产物落 `output_dir`。
- 参考：`eng/contracts/schemas/cli_output.schema.json`、`eng/contracts/schemas/events.schema.json`。

## 4. 算法与公式要点

- 命令树（唯一）见最高设计 §6.2：`help / --version / doctor / benchmark / normalize|mosaic|export --json|--template|--help`；
- 三个命令直接以命令名调用（`normalize` / `mosaic` / `export`），是平级独立命令；`phase1|2|3` 仅为内部命名；
- **运行前预检（检查页面）**：见最高设计 §4.5——三档（🟢 correct / 🟠 warn 不阻塞 / 🔴 error 阻塞），三档都显示完整检查页面；**无 error 时（correct 与 warn）都需用户输入 `yes` 确认才运行**，`-y`/`-yes` 跳过确认；存在 error 时 `-y`/`-yes` 不能越过；`-force` 跳过整个检查步骤直接运行（后果由用户承担）；
- `benchmark` 直接输出 profile 到**安装目录**（自动生成/更新），后续运行时自动读取；
- `help` 直接输入即为详细帮助；
- 取消：协作取消 → 关 writer → incomplete manifest → 隔离临时产物 → exit 9；
- stdout 无日志污染；事件走 JSONL（schema_version/event_id/run_id/kind 含 progress/resource/artifact/backend/final）。

## 5. 配置项

（CLI 自身配置极少；主要转发 phase_config 校验规则）

| 字段 | 默认 | 说明 |
|---|---|---|
| `json` | —— | 配置路径（必填于运行） |
| `template` | —— | 生成模板 |
| `output` | stdout | 模板输出路径 |
| `help` | —— | 帮助与字段说明 |
| `-y` | —— | 跳过运行确认（自动 yes） |
| `-force` | —— | 跳过整个检查步骤直接运行（后果由用户承担） |

## 6. 接口/ABI

- entrypoint：`main()` → 解析 → 预检 → 确认 → 调度执行 → 退出码；
- 退出码唯一源 `lib/include/astrocs/exit_codes.h`（0/2/3/4/5/6/7/8/9/10/70）。

## 7. 错误与边界

- 参数错误 → 2；输入缺失/格式错 → 3；科学验证失败 → 4；ABI/加载失败 → 5；执行失败 → 6；I/O → 7；输出完整性 → 8；取消/超时 → 9；资源门禁 → 10；未分类 → 70；
- 存在 error 时**强制阻断**运行（`-y`/`-yes` 不能越过；`-force` 跳过整个检查步骤）；
- 未捕获异常 → 脱敏 crash report，不泄露凭据。

## 8. 测试与 Oracle

- 命令树/帮助/模板输出测试；
- 预检页面测试：correct / warn / error 三档展示与阻断行为；
- 每个退出码的可达测试；
- 取消路径（Ctrl-C）无半成品；
- `--json` 输出恰一个 JSON 文档、无日志污染；
- 跨平台同失败同码。
