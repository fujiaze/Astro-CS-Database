# 模块 astrocs.phase1.calibration

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 phase1_descriptor）。
职责：master 生成（sigma-clip + median/mean 合并）与单帧校准（bias/dark/flat，
dark_opt 双分支）——生产源 lib/algorithms/calibration/src/，头
lib/algorithms/calibration/include/astro_calibration.h（12 导出：ac_generate_master_bias/
dark/flat、ac_calibrate_frame(+_f64)、ac_correct_frame(+_f64)、ac_set_num_threads、
ac_version）；生产调用 lib/phase1_session/p1_session.cpp（calibrate 阶段
ac_calibrate_frame + cosmetic 阶段 ac_correct_frame）。不做：天体测量/测光定标、
FITS 读写（astro_image_io）、母版分组匹配（orchestrator）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `frames` | `DATA-P1-FRAME` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `calibrated` | `DATA-P1-CAL` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

模块级 API = API-CAL-001（docs/contracts/PUBLIC_API.md；头 astro_calibration.h、
实现 lib/algorithms/calibration/src/）；编排级 API = API-P1-001（phase session
extern "C"，签名源 docs/api/PHASE1_API_V1.md）；生命周期
create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.calibration`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据）；归约口径 = docs/science/algorithms/CALIBRATION_ALGORITHMS.md §6。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-CAL-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；
容差=NOT_VERIFIED（未取得验收证据）；设计冻结容差 = TEST-CAL-DESIGN-001
（CALIBRATION_ALGORITHMS.md §9），既有共址测试 = lib/algorithms/calibration/tests/。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-P1-CAL-001` 合同边界。
