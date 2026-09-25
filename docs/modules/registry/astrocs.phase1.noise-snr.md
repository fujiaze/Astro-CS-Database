---
id: MOD-astrocs-phase1-noise-snr
module_id: astrocs.p1.noise_snr
aliases: [astrocs.p1.noise, astrocs.phase1.noise]
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-NOISE-001, ALG-NOISE-001, API-NOISE-001]
downstream: [TEST-P1-SNR-001]
---

> 人工内容（MANUAL）：本页无 GENERATED-ANCHOR，非生成器所有；重跑 eng/tools/quality/gen_module_readmes.py 不会覆盖本页。

# 模块 astrocs.phase1.noise-snr

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

> 合同 ID = SCI-NOISE-001..015 / ALG-NOISE-001..003 / DATA-P1-NOISE /
> API-NOISE-001；模块级事实以 lib/algorithms/noise_snr/README.md（r1，
> CONTRACT_READY）与现行生产实现 lib/algorithms/noise_snr/cpp/（现状构建=
> cpp/Makefile:5,12 g++ -shared → snr_estimator.dll + cpp/build.ps1:29，
> 未编入根 CMake 主构建）为准；descriptor 占位 ID（module_adapters.cpp
> p1_noise_snr_descriptor）以本页为准；冻结依据 = 本页本身，以免反向作为冻结
> 依据。port DATA 编目（DATA-P1-FLUX/DATA-P1-SNR）为编排层词汇，模块
> 合同 DATA 层=DATA-P1-NOISE（DATA_SEMANTICS §13）。
> **噪声模型 A 为唯一生产模型**；噪声 σ 来源 = 局部 patch + 星点掩膜 + 饱和过滤。

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `photprov` | `DATA-P1-PHOTPROV-001` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `cleaned` | `DATA-P1-COSMETIC` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `fluxes` | `DATA-P1-FLUX` | 必 | `UnitId::ADU` | `CoordinateFrame::ICRS` |
| `snr` | `DATA-P1-SNR` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-006` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.noise-snr`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json 无 astrocs.phase1.noise-snr.determinism 条目；未读到的结论不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-SNR-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（证据源 eng/ci/ledgers/module_page_evidence.json 无 astrocs.phase1.noise-snr.verification 条目）；容差=NOT_VERIFIED（同上）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-004` 合同边界。
