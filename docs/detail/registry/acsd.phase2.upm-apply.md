# Mosaic 加性差模应用

本节点将帧信号扣到所选公共面，消费 fit 的同一模型。施加语义与保真边界由 [UPM 正本](../../science/sky/UPM.md)规定：只扣逐帧偏差，公共天空保留；颜色、PSF 和未知乘性失配不自动成为可扣背景。

## 真实入口和数据交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_upm_apply` 对应 `acsd.phase2.upm-apply`。它读取实际 `p2_sky_plane.bin`、coverage、samples 及帧产品，调用 `p2_sky_plane_open` 与 `p2_sky_plane_eval_delta_block`，输出 `p2_corrected.json`、逐帧 corrected 数据及可用方差工作面。reject 和 integrate 消费这些对象。

descriptor 的 upm_model/calibrated_frames→corrected 是排序/载荷声明；真实模型是 sky-plane，不能要求不存在于生产链的 `p2_upm_model` 文件仅充依赖条件。

## 接口、数值与单位

[sky_plane.h](../../../lib/algorithms/coverage/include/astro/phase2/sky_plane.h)声明 `p2_sky_plane_eval_delta`、`_eval_delta_block`、`_eval/_eval_block`、`_info`、`_frame_delta` 及 `_open/close`；[实现](../../../lib/algorithms/coverage/src/sky_plane.cpp)由 fit 复用。

| 字段/载荷 | 语义 |
|---|---|
| frame_id | 与采样/模型同一身份，不能按 compact 或输入位置猜帧 |
| ra/dec | ICRS degree；每叶位置由真实目标网格生成 |
| eval_delta 输出 | 逐帧低阶差与残差场；不是包括公共面的全量背景 |
| corrected | raw−delta；继承真实球面面亮度单位，负值与零合法 |
| support/validity | 覆盖/资格，不能作为像元统计权重 |
| corrected variance | 可用时为内部统计对象，条件、拟合耦合及缺项随 provenance 表达 |

当前 apply 有基于控制观测的残差制造者方差计算路径；存在此路径不证明任意图像位置、相关性、模型失配与交叉项已完整描述。精度权重/实际条件由[噪声正本](../../science/noise_snr/NOISE_SNR.md)解释，具体消费见[集成卡](acsd.phase2.integrate.md)。

## 所有权、并行和兼容 API

模型由 open 获取，作用域结束用 `p2_sky_plane_close`；逐帧独立读句柄、tile 缓冲和工作文件由 adapter/AIO 管理，不需复制全帧常驻。Runtime 提供并行预算，错误/取消沿节点接口传播；不能把无段内取消参数写成已具备任意时刻取消。

[upm.h](../../../lib/algorithms/coverage/include/astro/phase2/upm.h)另有 `p2_upm_calibrate_block`、`_evaluate_c`、`_materialize_dense_n`、`_dense_read_block`，用于库模型/缓存；生产 apply 不改走该场。dense stale=2 是该库缓存语义，不是所有节点统一错误码。`p2_upm_close` 与 sky-plane close 不能混用。

## 配置、错误与验证入口

apply 不独立拟合新参数，消费 fit 保存的配置/模型；不能重复施加 Normalize 光度 scale。缺实际模型、坏模型或错误帧绑定须报具体错误，不静默改扣其它场；无覆盖保持 NaN。合法数据组合不能因为仪器/滤镜字符串不同而被拒绝。

验证从 `p2_sky_plane_eval_delta_block` 与[Mosaic 管线](../mosaic/pipeline.md)进入：已知差模、公共面保留、跨 tile 连续、单帧/空洞支持、缺模型与未知 frame、方差 Monte Carlo、同输入不同块/worker。实测状态和相邻差异仅按[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)报告，本页不新增数值成功结论。
