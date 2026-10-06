# 模块 acsd.phase3.verify

> 上游：`docs/ACSD_DESIGN.md`「模块与 ABI」一节

Registry production 节点（唯一源 = module_adapters.cpp 的 p3_verify_descriptor，
节点操作 `verify_output`）。职责：对已写 FITS 做独立重开校验（READONLY 重开 →
逐 HDU 尺寸/像素/哈希比对，`p3_output_verify`/`p3_output_verify_ex`，
lib/algorithms/fits_output/p3_output.h），出 `verified`（DATA-P3-VER）；
会话消费面 lib/phase3_session/p3_export.cpp（`aio::verify_fits_file`）。
不做：写出本身（P3-FITS）、重采样（P3-RSMP）、properties/WCS 计划
（P3-PROPS/P3-WCS）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `fits` | `DATA-P3-FITS` | 必 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::PIXEL` |
| `verified` | `DATA-P3-VER` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

invalid = 重开失败、HDU 尺寸或像素不符、sha256 不匹配即 fail-closed（返回非 OK
状态，不产出半成品 verified；判据 = `p3_output_verify_ex`）。

## 公共 header、核心 symbol 与生命周期

校验实现 = lib/algorithms/fits_output/p3_output.h 的 `p3_output_verify`/
`p3_output_verify_ex`（重开路径 `aio::verify_fits_file`，lib/phase3_session/p3_export.cpp）；
节点入口 = P3NodeOp::Verify（`verify_output`，节点绑定表）；编排级 API = API-P3-001
（docs/engineering/api/PUBLIC_API.md「export 会话生命周期合同」，phase session extern "C"）；生命周期
create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`acsd.phase3.verify`; execution_class=`io`;
parallel_ok=False; 配置=phase config JSON（键集 = API-P3-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`io`; parallel=否(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据）；校验面为只读比对（同输入同判定）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计协作取消口径；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P3-VER-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；
容差=NOT_VERIFIED（未取得验收证据；尺寸/像素/哈希比对为精确判定）；设计依据 =
docs/science/algorithms/PHASE3_FITS_IMPL.md（FITS 写出与校验的实现级合同）。

## 已知限制

见 artifacts/evidence/known-limitations-ledger/LIMITATIONS.md 与 `ALG-P3-005` 合同边界
（docs/science/algorithms/PHASE3_FITS_IMPL.md 的校验条款）。
