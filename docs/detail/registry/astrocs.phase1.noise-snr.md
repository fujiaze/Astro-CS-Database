# 模块 astrocs.phase1.noise-snr

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-NOISE-001..015 / ALG-NOISE-001..003 / DATA-P1-NOISE / API-NOISE-001。
> 模块级事实以 lib/algorithms/noise_snr/README.md（CONTRACT_READY）与现行生产实现
> lib/algorithms/noise_snr/cpp/ 为准（现状构建 = cpp/Makefile:5,12 g++ -shared →
> snr_estimator.dll + cpp/build.ps1:29，未编入根 CMake 主构建）。端口 DATA 编目
> （DATA-P1-FLUX/DATA-P1-SNR）为编排层词汇，模块合同 DATA 层 = DATA-P1-NOISE
> （DATA_SEMANTICS §13）。
> 噪声模型 A = `NoiseWeightModelV1`（正本 docs/science/NOISE_MODEL.md），为唯一生产模型；
> 噪声 σ 来源 = 局部 patch + 星点掩膜 + 饱和过滤（推导与逐符号锚 =
> docs/science/algorithms/NOISE_ESTIMATION.md §2–§5）。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_noise_snr_descriptor）。
职责：估空背景稳健方差并出 variance/ivar 平面（噪声模型 A）——生产内核
lib/algorithms/noise_snr/cpp/（noise_model.cpp 的 patch 采集与 σ 估计）、模块入口
src/module_entry.cpp、phase1 包装面 wrapper_phase1/snr_frame_science.{h,cpp}。
不做：PSF/测光本身、SCI/ALG 合同之外的扩展。

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

模块级 API = API-NOISE-001（docs/engineering/PUBLIC_API.md 噪声/SNR 节；头
include/astrocs/information_weight.h、include/astrocs/noise/{types,variance_plane_policy,saturation_policy}.h，
入口 src/module_entry.cpp，导出面 src/astrocs_p1_noise.def）；编排级 = API-P1-006
（phase session extern "C"）；生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.noise-snr`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-SNR-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；容差=NOT_VERIFIED（同源）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-NOISE-001..003` 合同边界（缺陷与整改登记 =
docs/science/algorithms/NOISE_ESTIMATION.md §13.3）。
