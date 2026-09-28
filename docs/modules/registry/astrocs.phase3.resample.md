# 模块 astrocs.phase3.resample

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

本页登记 registry 的占位 descriptor（phase3_descriptor：module_id=
`astrocs.phase3.resample`，端口 `hips`/`tile`、合同 ID SCI-P3-RES-001/ALG-P3-003
为 P2 模板口径）；该 descriptor 现状按会话委托注册（工厂委托 P3Api session
adapter）。Phase3 实际重采样面 = astrocs.phase3.resample2（生产源
lib/algorithms/resample/p3_resample.cpp + 头 p3_resample.h；会话消费
lib/phase3_session/p3_session.cpp 的 sampler 段）。不做：properties 解析
（P3-PROPS）、WCS 计划（P3-WCS）、FITS 写盘（P3-FITS）、输出校验（P3-VER）。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `hips` | `DATA-HIPS-001` | 必 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::PIXEL` |
| `tile` | `DATA-TILE-001` | 可 | `UnitId::SURFACE_BRIGHTNESS` | `CoordinateFrame::HEALPIX` |

invalid = 上游 `props`/`wcs_plan` 非法即显式拒（fail-closed，无 silent default）；
数值 invalid 依 DATA-P3-RES（DATA_SEMANTICS §29）。

## 公共 header、核心 symbol 与生命周期

编排级 API = API-P3-001（docs/api/PHASE3_API_V1.md，phase session extern "C"）；
本模块无独立 C ABI（会话委托注册）；实际重采样入口 =
lib/algorithms/resample/p3_resample.h（`open_ex`/`p3_order_select` + 逐像素
nearest/bilinear 分派）；生命周期 create→validate→run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase3.resample`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON（键集 = API-P3-001）。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据）；重采样确定性与归约口径 =
docs/science/algorithms/PHASE3_RSMP_IMPL.md（ALG-P3-RSMP-IMPL-001）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P3-RES-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；
容差=NOT_VERIFIED（未取得验收证据）；设计冻结面 = ALG-P3-RSMP-IMPL-001 TEST-DESIGN；
探针 = eng/tests/backend/p3_resample_probe_main.cpp + eng/tests/backend/test_p3_resample.py。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 docs/science/algorithms/PHASE3_RSMP_IMPL.md 的缺陷
登记节；实际重采样面的登记见 astrocs.phase3.resample2.md。
