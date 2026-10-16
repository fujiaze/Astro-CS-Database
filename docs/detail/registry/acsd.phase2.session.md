# Mosaic 局部模型会话

本页登记 `p2_session_*` 进程内 facade：coverage、sample、UPM build、可选 persist。完整用户 `mosaic` 命令由[七节点管线](../mosaic/pipeline.md)执行，另含 apply、reject、integrate 和最终 signal HiPS 写出；二者不能混称。

## 接口与调用关系

公开头 [p2_session.h](../../../lib/phase2_session/p2_session.h)、实现 [p2_session.cpp](../../../lib/phase2_session/p2_session.cpp)。接口为 `p2_session_create(host,out)`、`p2_session_validate(handle,config_json)`、`p2_session_run(handle,config_json)`、`p2_session_inspect(handle,out_manifest_json)`、`p2_session_destroy(handle)`，返回 `acsd_status`。C++ `acsd::phase2::last_error` 仅用于诊断。

此会话直接调用 `p2_coverage_build`、采样和 `p2_upm_build/save`。CLI 经 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `P2NodeModule` 分节点执行，生产 fit 使用 `p2_sky_plane_build_joint`；不能因保留 P2Api 会话适配就把库 UPM 模型认作生产扣除模型。

## 数据、配置与所有权

| 对象 | 语义 |
|---|---|
| host | allocator/logger/cancel/budget，寿命覆盖 handle |
| config | hips_paths/output_dir，局部 upm 参数与 persist_upm/upm_save_path；以本 facade validate/run 的实际消费为准 |
| coverage、observations | 从产品枚举与采样产生；sky 为 ICRS degree，样本值继承信号单位 |
| model | 本会话构建的 UPM 模型，调用方不能视为完整马赛克 |
| inspect | created/complete/failed 及阶段/模型信息；不是最终 signal 发布凭据 |

创建者拥有 handle，单 handle 串行，不同 handle 可独立运行。输出 JSON 经 host allocator 分配/释放；模型与缓冲由会话 RAII 持有并用相应 close/free 清理。阶段边界检查取消，UPM worker 数来自 host budget；局部会话不建立跨命令内存共享。

## 错误与验证入口

返回 `ACS_ERR_*`，细节由错误摘要与 manifest 说明；纯配置检查与文件/模型构建错误分开。输出路径和配置字段是否被 validate 接受不证明 run 真正消费该字段。

验证从本头五函数检查生命周期、分配配对、非法类型、取消、persist/open；完整七节点检查从 [Mosaic 实现导航](../mosaic/pipeline.md)追至 CLI。科学模型由 [UPM 正本](../../science/sky/UPM.md)拥有；本页不以局部会话成功代替完整合成或无接缝实测。
