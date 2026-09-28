# 模块 astrocs.phase2.resample

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

## 职责与明确非职责

本页登记 registry 的占位 descriptor（phase2_descriptor：module_id=
`astrocs.phase2.resample`，端口 `calibrated`/`resampled`、合同 ID SCI-P2-RES-001/
ALG-P2-RES-001 均为 P2 模板口径），该 descriptor 现状按占位注册（与
lib/phase2_session 会话面同一调度），实现面无独立模块目标（迁移目标未落地）。
Phase2 实际重采样路径 = lib/phase2_session 会话面。不做：积分/排异（P2-INT/P2-REJ）、
UPM（P2-UPM）、马赛克写出（P2-WRITE）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `calibrated` | `DATA-P1-CAL` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `resampled` | `DATA-P2-RES` | 可 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

编排级 API = API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN；phase session
extern "C"）；生命周期 create→validate→run→inspect→destroy 由会话承接。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase2.resample`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据）；占位 descriptor 无独立实现面，
确定性口径随 lib/phase2_session 会话面（PHASE2_API_V1 §2 并发条款）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P2-RES-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；
容差=NOT_VERIFIED（未取得验收证据；占位 descriptor 无冻结数值容差）。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-P2-RES-001` 合同边界。
