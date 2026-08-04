# R10 实现报告

日期: 2026-08-04 | 阶段: R10 纠正控制包（阶段 1-7 已完成，阶段 8 交付进行中）

## 1. 目标

按 `AstroCS_Phase1_JSON_Orchestrator_TrueDualPrecision_Correction.zip` 纠正:

1. 纠正虚假完成状态
2. 只允许 `orchestrator.exe <json>` 单一入口
3. 彻底移除生产 Python 封装
4. 实现真正全链路 FP32/FP64
5. 用合成数据和单帧分段验收
6. 严禁触碰 ACR

## 2. 提交清单（`85fa651..8cb22a9`）

| Commit | 内容 |
| --- | --- |
| `85fa651` | 编排器 JSON 唯一入口重构（删除 REPL/run/run-batch） |
| `8c616f1` | 手写 JSON 解析器替换为 nlohmann/json |
| `8653bd5` | Python 生产层彻底清理（421 文件删除，剩余全部带非生产标记） |
| `bd036f3` | 真正全链路 FP32/FP64 双精度 ABI 改造 |
| `f654766` | FP64 HISS_VERIFY 修复（精度感知 signal 读取） |
| `c6efe31` | HISS_VERIFY 全 Tile 验证 + SNR 点原因分类 |
| `8cb22a9` | SnrControlPoint 结构体打包 bug 修复 |

## 3. 唯一入口验证

```powershell
orchestrator.exe <stage1.json>        # 正式科学运行
orchestrator.exe --help|--version|--print-schema|--validate <json>|--inspect <hiss>  # 诊断
```

- 无 REPL、无 `run`/`run-batch`、无 stage1/stage2 子命令、无 CLI 覆盖
- Schema 严格校验（`additionalProperties: false`，nlohmann/json + 内嵌 Schema）
- 相对路径以 JSON 文件所在目录解析（`json_config.cpp` `resolve_path`）
- 所有输入/输出/日志位置只来自 JSON

## 4. Python 边界

- 生产 Python wrapper/adapter/CLI 全部删除（约 421 个文件，commit `8653bd5`）
- 剩余 18 个 Python 文件全部为 tests/tools，全部含 `NON_PRODUCTION_TOOL_ONLY` 标记
- 审计 CSV: `reports/python_audit.csv`（18 行，production_reachable 全为 NO）
- 正式运行无 Python 环境可依赖

## 5. 双精度实现

- 公共层: `AstroScalarType` + `PrecisionContext`（`lib/common/include/`）
- AIO: `aio_set_precision_mode(int)`，FITS/XISF 按模式读入 `data` 或 `data_f64`
- Drizzle: FP32/FP64 双累计器，HISS signal 双 dtype（signal_dtype 0/1）
- HISS_VERIFY: 全 Tile 遍历（285/285），精度感知 signal 读取
- 运行时证据: `reports/precision_trace.jsonl`（每阶段 input/compute/accumulator/output dtype）

## 6. 验收方式

- 合成数据: HISS Writer/Reader FP32/FP64 roundtrip bit-exact（63 项全过）
- 单帧真实数据: T4 Red 一帧，FP32/FP64 各跑全 8 阶段 + HISS_VERIFY
- 结果: 285/285 Tile 通过，exit_code=0（见 `single_frame_gate_results.csv`）

## 7. 未做（明确范围外）

- 710 帧全量回归、T1-T4 批量、银心三片
- FAST 研究、Stage2 修改、ACR 接入
- 性能优化
