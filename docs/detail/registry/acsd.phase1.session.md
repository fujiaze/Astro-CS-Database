# Normalize 局部会话

本页登记进程内 `p1_session_*` facade。完整用户命令由 [Normalize 生产链](../../engineering/architecture/NORMALIZE_PIPELINE.md) 的八个节点执行；本 facade 仅覆盖读取、校准、cosmetic 和校验写出。

## 入口与调用边界

公开头为 [p1_session.h](../../../lib/phase1_session/p1_session.h)，实现为 [p1_session.cpp](../../../lib/phase1_session/p1_session.cpp)。接口为 `p1_session_create(host,out)`、`p1_session_validate(handle,config_json)`、`p1_session_run(handle,config_json,async_io_depth)`、`p1_session_inspect(handle,out_manifest_json)`、`p1_session_destroy(handle)`；返回 `acsd_status`。配置与 inspect 文本均用 `acsd_span_u8` 传递。C++ `acsd::phase1::last_error` 提供诊断摘要。

[scheduler adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 保留 `P1Api` 会话接入能力；CLI 的 `run_phase1_block` 经 `P1NodeModule` 分别调用八个 `p1_op_*`，不会因持有本组五函数就获得完整 WCS、测光、参考精度或 HiPS 能力。本页不登记独立生产 DAG 节点。

## 数据、所有权与执行

| 输入或输出 | 合同 |
|---|---|
| host | allocator、logger、cancel、budget，创建时注入；有效期覆盖会话 |
| config | lights、masters、output_dir 及校准/cosmetic 参数；允许字段和类型由 validate 实现检查 |
| 校准结果 | 行主序图像，内部像元中心用零基索引；单位随校准输入声明 |
| manifest | 本局部会话的阶段状态、文件和错误；其它科学域不可用，不冒称完整 Normalize 产品 |

创建者唯一拥有 handle，同一 handle 串行使用，不同 handle 可分别运行。`inspect` 输出经 host allocator 分配，调用方经同一 host free 释放。AIO 图像使用 `aio_free_image_data`；算法输出缓冲由调用方管理。

`async_io_depth` 接受 0、1、2；有预算时预读下一帧。校准线程数由 host budget 注入，取消在帧边界检查。此局部会话的线程/取消行为不能代替八节点链的实际接线说明。

## 错误与验证入口

返回码使用 `ACS_ERR_*`，错误由会话 manifest 和 `last_error` 说明；失败不写成 complete。校准与 cosmetic 的状态须按实际调用分别记录，未启用修复与已修复不能混写。

验证入口为本头文件五函数和 [Normalize 测试入口](../../../eng/tests/pipeline/test_stage_normalize.py)；前者检查生命周期、host 分配配对、非法配置和取消，后者检查完整命令。测试入口存在不表示已执行或得到科学支持。完整数据/能力要求见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)；具体实施边界见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
