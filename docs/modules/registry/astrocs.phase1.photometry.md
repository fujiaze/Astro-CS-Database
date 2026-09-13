---
id: MOD-astrocs-phase1-photometry
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: 5ecc60df2d5021d18be04e0e6359d45b7b125b33
upstream: [SCI-PHOT-001, ALG-PHOT-001, ALG-PHOT-002, API-P1-005]
downstream: [TEST-P1-PHOT-001]
---

# 模块 astrocs.phase1.photometry

> P1-PHOT-DOC 事实修订（2026-09-07，wave W1）：本页由源码核对后修订——
> 合同 ID 由占位（SCI-P1-PHOT-001/ALG-002/TEST-P1-PHOT-001）更正为真实
> 冻结 ID（SCI-PHOT-001 / ALG-PHOT-001..002 / DATA-P1-PHOT / API-PHOT-001 /
> SRC-PHOT-001 / TEST-PHOT-DESIGN-001）；模块级事实以
> lib/photometric_calib/README.md（r1，CONTRACT_READY）与现行生产实现
> lib/photometric_calib/cpp/（现状构建=cpp/Makefile:11 g++ -shared
> -fopenmp → photometric_calib.dll + cpp/build.ps1:9，未编入根 CMake 主
> 构建）为准；descriptor 占位 ID（module_adapters.cpp:531-548
> p1_photometry_descriptor）由 P1-PHOT-INT 对齐本页，不得反向作为冻结
> 依据。port DATA 编目（psf→DATA-P1-PSF/sources→DATA-P1-SOURCES/
> fluxes→DATA-P1-FLUX）为编排层词汇，模块合同 DATA 层=DATA-P1-PHOT
> （DATA_SEMANTICS §14）。生产调用=orchestrator.cpp:2474
> run_stage_photometric → :2714 pc_calibrate_simple_with_gaia_f64_v2 /
> :2790 _v2（dll_loader.cpp:40/:54）；lib/phase1/photometry（Photometer
> aperture 旧符号，CMakeLists.txt:429-432 静态库，仅单测
> tests/unit/p1_wcs_phot_test）为计划迁移旧符号（README §9）。
> DISP-PHOT-001..009 登记（PHOTOMETRIC_FIT §13.3），整改归
> P1-PHOT-IMPL/INT。

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `psf` | `DATA-P1-PSF` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `fluxes` | `DATA-P1-FLUX` | 可 | `UnitId::ELECTRON` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-005` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.photometry`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(heavy+serial 资源门禁止); worker 数=ThreadBudget.max_workers(禁 hardware_concurrency);
确定性=固定顺序输出(1/N 等价已验)。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码=ACS_ERR_*(API 合同); 取消=host cancel 回调; 无 checkpoint(Phase3 原子写)。

## 独立 synthetic 验证命令与容差

`TEST-P1-PHOT-001` 对应测试(逐任务 TASK_RESULT 证据); 容差=验收冻结。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-002` 合同边界。
