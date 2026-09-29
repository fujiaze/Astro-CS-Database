# 模块 astrocs.phase1.photometry

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-PHOT-001 / ALG-PHOT-001..002 / DATA-P1-PHOT / API-PHOT-001 /
> SRC-PHOT-001 / TEST-PHOT-DESIGN-001。模块级事实以
> lib/algorithms/photometry/README.md（CONTRACT_READY）与现行生产实现
> lib/algorithms/photometry/cpp/ 为准（现状构建 = cpp/Makefile:11 g++ -shared
> -fopenmp → photometric_calib.dll + cpp/build.ps1:9，未编入根 CMake 主构建）。
> 端口 DATA 编目（psf→DATA-P1-PSF / sources→DATA-P1-SOURCES / fluxes→DATA-P1-FLUX）
> 为编排层词汇，模块合同 DATA 层 = DATA-P1-PHOT（DATA_SEMANTICS §14）。生产调用 =
> orchestrator.cpp run_stage_photometric → pc_calibrate_simple_with_gaia_f64_v2 /
> _v2（dll_loader.cpp 加载）。lib/algorithms/photometry/wrapper_phase1 的 Photometer
> aperture 面为迁移目标面（README §9），当前由共址单测
> eng/tests/unit/p1_wcs_phot_test 引用。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_photometry_descriptor）。
职责：源星表 ↔ 参考星表双向最近邻配对（KD-tree，2.0px）+ 星等预过滤 + IRLS/Tukey
稳健零点求解（scale=10^(−location)）——生产入口 pc_calibrate_simple_with_gaia_f64_v2、
_v2（头 lib/algorithms/photometry/cpp/include/photometric_calib.h，6 导出）。不做：
逐像素 ivar（边界 = docs/science/PHOTOMETRY.md）、星点检测、星表缓存管理。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `sources` | `DATA-P1-SOURCES` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `fluxes` | `DATA-P1-FLUX` | 可 | `UnitId::ADU`（与 DATA_SEMANTICS §14.1 psf_flux 一致） | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-PHOT-001（docs/contracts/PUBLIC_API.md 测光节；头
lib/algorithms/photometry/cpp/include/photometric_calib.h：PC_API/extern "C" 不抛异常，
gaia_client_handle 借用不持有，spec_stars/spectra_buf 调用内释放，out_* 调用方分配）；
编排级 = API-P1-005（phase session extern "C"）；生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.photometry`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-PHOT-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；容差=NOT_VERIFIED（同源）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-PHOT-001..002` 合同边界（缺陷与整改登记 =
docs/science/algorithms/PHOTOMETRIC_FIT.md §13.3）。
