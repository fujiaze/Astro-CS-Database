# 模块 acsd.phase3.properties

> 上游：docs/ACSD_DESIGN.md §8.5（模块与 ABI）

Registry production 节点（唯一源 = module_adapters.cpp 的 p3_properties_descriptor，
节点操作 `read_properties`）。职责：从 HiPS 树读出并严格解析 properties
（`hips_properties_parse`，lib/algorithms/coverage/hips_properties.h，
ALG-P3-001 必需键校验、非法显式拒、无 silent default），出 `props`
（DATA-P3-PROPS）；消费面 lib/phase3_session/p3_export.cpp。不做：信号/覆盖面
计算、球面重采样（P3-RSMP）、WCS 计划（P3-WCS）、FITS 写盘（P3-FITS）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `hips` | `DATA-HIPS-001` | 必 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::HEALPIX` |
| `props` | `DATA-P3-PROPS` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::HEALPIX` |

invalid = properties 缺失或键非法即 fail-closed（`hips_properties_parse` 返回
false + 错误文本），不产出半成品 props（ALG-P3-001：非法显式拒）。

## 公共 header、核心 symbol 与生命周期

解析实现 = lib/algorithms/coverage/hips_properties.h 的 `hips_properties_parse`；节点入口 =
P3NodeOp::Properties（`read_properties`，节点绑定表）；编排级 API = API-P3-001
（docs/engineering/api/PUBLIC_API.md「export 会话生命周期合同」，phase session extern "C"）；生命周期
create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`acsd.phase3.properties`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON（键集 = API-P3-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据）；解析面为纯函数（同输入同输出，无共享状态）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §7.2；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P3-PROPS-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；
容差=NOT_VERIFIED（未取得验收证据；解析面按必需键集合判定，无数值容差）；
设计依据 = docs/science/algorithms/PHASE3_RESAMPLE.md（ALG-P3-001）。

## 已知限制

见 artifacts/evidence/known-limitations-ledger/LIMITATIONS.md 与 `ALG-P3-001` 合同边界
（docs/science/algorithms/PHASE3_RESAMPLE.md 的 properties 校验条款）。
