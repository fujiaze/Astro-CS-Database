# 模块 acsd.phase1.writer

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_writer_descriptor）。
职责：把叠加产品写为 FITS（WCS/provenance 关键字随写）——实现面 =
lib/infrastructure/aio 的 `aio_write_fits`，节点登记名 = acsd_phase1_writer_v1
（模块节点绑定表）。不做：像素算法、校准/叠加本身、静默覆盖既有产品。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `stacked` | `DATA-P1-STACK` | 必 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::ICRS` |
| `fits` | `DATA-P1-FITS` | 可 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::ICRS` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

节点入口 = API-P1-008（phase session extern "C"；节点登记名 acsd_phase1_writer_v1）；
写出实现 = lib/infrastructure/aio 的 `aio_write_fits`/`aio_read_fits`（FITS/XISF 唯一
I/O 层，原子发布按 IO-003 合同）；生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`acsd.phase1.writer`; execution_class=`io`;
parallel_ok=False; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`; parallel=否(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-WR-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；容差=NOT_VERIFIED（同源）。

## 已知限制

见 artifacts/evidence/known-limitations-ledger/LIMITATIONS.md 与 `ALG-P1-WR-001` 合同边界。
