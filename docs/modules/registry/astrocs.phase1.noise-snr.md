---
id: MOD-astrocs-phase1-noise-snr
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-NOISE-001, ALG-NOISE-001, API-NOISE-001]
downstream: [TEST-P1-SNR-001]
---

# 模块 astrocs.phase1.noise-snr

> P1-NOISE-DOC 事实修订（2026-09-07，wave W1）：本页由源码核对后修订——
> 合同 ID 由占位（SCI-P1-SNR-001/ALG-004/TEST-P1-SNR-001）更正为真实
> 冻结 ID（SCI-NOISE-001..015 / ALG-NOISE-001..003 / DATA-P1-NOISE /
> API-NOISE-001）；模块级事实以 lib/snr_estimator/README.md（r1，
> CONTRACT_READY）与现行生产实现 lib/snr_estimator/cpp/（现状构建=
> cpp/Makefile:5,12 g++ -shared → snr_estimator.dll + cpp/build.ps1:29，
> 未编入根 CMake 主构建）为准；descriptor 占位 ID（module_adapters.cpp
> p1_noise_snr_descriptor）由 P1-NOISE-INT 对齐本页，不得反向作为冻结
> 依据。port DATA 编目（DATA-P1-FLUX/DATA-P1-SNR）为编排层词汇，模块
> 合同 DATA 层=DATA-P1-NOISE（DATA_SEMANTICS §13）。

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `fluxes` | `DATA-P1-FLUX` | 必 | `UnitId::ELECTRON` | `CoordinateFrame::ICRS` |
| `snr` | `DATA-P1-SNR` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-006` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.noise-snr`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(heavy+serial 资源门禁止); worker 数=ThreadBudget.max_workers(禁 hardware_concurrency);
确定性=固定顺序输出(1/N 等价已验)。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码=ACS_ERR_*(API 合同); 取消=host cancel 回调; 无 checkpoint(Phase3 原子写)。

## 独立 synthetic 验证命令与容差

`TEST-P1-SNR-001` 对应测试(逐任务 TASK_RESULT 证据); 容差=验收冻结。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-004` 合同边界。
