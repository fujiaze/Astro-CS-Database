# R10-001 证据索引

日期: 2026-08-04 | 阶段: R10 纠正控制包（JSON 唯一入口 + 真正双精度）

## 报告

| 文件 | 内容 |
| --- | --- |
| `reports/abi_report.md` | 双精度 ABI、结构体打包修复、跨 DLL 精度传播 |
| `reports/snr_reconciliation.md` | SNR 控制点 1979/1947 有效核算与原因分类 |
| `reports/implementation_report.md` | 提交清单、唯一入口、Python 边界、双精度实现 |
| `reports/science_correctness_report.md` | 科学约束遵守、5 个关键 Bug 修复、合成+单帧验证 |
| `reports/known_issues_report.md` | 已知问题与精度例外（如实记录） |
| `reports/acceptance_self_review.md` | 29 项验收自审 |

## 数据

| 文件 | 内容 |
| --- | --- |
| `reports/precision_trace.jsonl` | 每阶段 input/compute/accumulator/output dtype（运行时证据） |
| `reports/single_frame_gate_results.csv` | FP32/FP64 单帧 gate 结果（config/output SHA256、耗时、日志） |
| `reports/python_audit.csv` | 剩余 18 个 Python 文件审计（全部非生产标记） |

## 原始日志（位于 `run/logs/r10/`）

| 文件 | 用途 |
| --- | --- |
| `run/logs/r10/fp32_snr_fix_verify_20260804.log` | FP32 单帧逐 Gate 全日志（635 KB，285/285 Tile） |
| `run/logs/r10/fp64_snr_fix_verify_20260804.log` | FP64 单帧逐 Gate 全日志（610 KB，285/285 Tile） |
| `run/logs/r10/synthetic_hiss_precision.log` | 合成 HISS FP32/FP64 bit-exact 测试（63/63 通过） |
| `run/logs/r10/python_entry_audit.csv` | 阶段 3 清理审计（清理前快照） |
| `run/logs/r10/build_aio_f64.log` / `build_orch_f64.log` | FP64 编译日志 |

## 产物

| 路径 | 说明 |
| --- | --- |
| `run/temp/r10_validation/output/fp32/frame.hiss` | FP32 单帧 HISS（285 Tile） |
| `run/temp/r10_validation/output/fp64/frame.hiss` | FP64 单帧 HISS（285 Tile） |
| `run/temp/r10_validation/fp32/stage1.json` | FP32 验证配置 |
| `run/temp/r10_validation/fp64/stage1.json` | FP64 验证配置 |

## Git 引用

- 范围: `85fa651..8cb22a9`（R10 实现提交）
- 当前 HEAD: `8cb22a9`（阶段 8 交付前）
- 分支: main（无 Stage1 分支）
