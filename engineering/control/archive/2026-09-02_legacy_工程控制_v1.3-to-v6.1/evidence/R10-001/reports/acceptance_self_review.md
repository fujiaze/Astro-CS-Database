# R10 交付前自审（ACCEPTANCE_CHECKLIST 29 项）

日期: 2026-08-04 | 审核人: 项目 Agent（R10 阶段 8）

| # | 检查项 | 状态 | 证据 |
| --- | --- | --- | --- |
| 1 | main 工作，无新 Stage1 分支 | ✅ | `git branch -a`: 仅 main + feature/astrocompute-runtime |
| 2 | ACR ref 前后一致 | ✅ | feature/astrocompute-runtime 未触碰 |
| 3 | FAST 和 Stage2 未修改 | ✅ | R10 范围未改动 healpix_stack C++ 实现（8653bd5 仅清理其 python/ 测试目录）；FAST 无提交 |
| 4 | 正式运行只有 `orchestrator.exe <json>` | ✅ | main.cpp 参数解析 |
| 5 | 无 REPL、run、run-batch、复杂科学 CLI | ✅ | 已删除（commit 85fa651） |
| 6 | 无 CLI 科学覆盖 | ✅ | 参数只来自 JSON |
| 7 | 使用成熟 JSON 库与严格 Schema | ✅ | nlohmann/json + `additionalProperties:false`（commit 8c616f1） |
| 8 | 模板含全部路径、精度和输出 | ✅ | `lib/orchestrator/configs/stage1.template.json` |
| 9 | 生产 Python wrapper/adapter/CLI 为 0 | ✅ | 仅 18 个测试/工具 .py，全带标记（python_audit.csv） |
| 10 | 保留 Python 全部有非生产标记 | ✅ | 18/18 含 `NON_PRODUCTION_TOOL_ONLY` |
| 11 | 无 Python 环境可运行 | ✅ | orchestrator.exe 静态链接，独立运行（--version 无 PATH 依赖） |
| 12 | FP32 各阶段实际 float32 | ✅ | precision_trace.jsonl（内部 double 拟合已注明） |
| 13 | FP64 各阶段实际 float64 | ✅ | precision_trace.jsonl（星点检测 float32 例外已注明） |
| 14 | FP64 日志不再出现 data float32 | ✅ | fp64 日志: `data_f64 (no float32 downgrade)` |
| 15 | FP32 Drizzle 不再 double 累计 | ✅ | fp32 日志: 累计器 FP32（PREC-009 修复） |
| 16 | HISS signal/SNR 双 dtype | ✅ | `add_tile` / `add_tile_f64`，signal_dtype 0/1 |
| 17 | Reader/Browser 双 dtype | ✅ | `aio_hiss_read_tile_signal_f64` / `aio_hiss_query_pixel_f64` |
| 18 | HISS_VERIFY 遍历全部 Tile | ✅ | 285/285（两个精度模式） |
| 19 | SNR 全部点完成原因分类 | ✅ | n_dropped 全归因 INVALID_PSF |
| 20 | 合成模块和组合链完整 | ✅ | synthetic_hiss_precision.log 63/63 |
| 21 | 仅一张真实帧逐 Gate | ✅ | T4 Red 单帧，未跑批量 |
| 22 | 正式测试零失败 | ✅ | FP32/FP64 gate 均 exit_code=0 |
| 23 | Wiki 与 README 无权威冲突 | ✅ | Wiki 已同步；README SSOT 声明已删 |
| 24 | source 无生成产物 | ✅ | 源码树已清理（lib/ 忽略产物 201 项，保护归档/模块 DLL）；交付 source 经 git archive 生成，0 个 exe/dll/o |
| 25 | SHA256 列出全部交付文件 | ✅ | `run/temp/r10_delivery/SHA256SUMS.txt` 390 项全部校验通过；ZIP 见交付包 |
| 26 | 所有外部进程有 timeout | ✅ | stage_timeout_sec（read..browser_verify） |
| 27 | 未宣称整个 Phase1 已完成 | ✅ | 文档统一声明"尚未闭合" |

## 结论

27/27 项全部闭合。
