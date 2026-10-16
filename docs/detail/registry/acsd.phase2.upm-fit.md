# Mosaic 公共面与加性差模拟合

生产 fit 联合拟合公共信号面与逐帧加性差，供 apply 仅扣差模、保留公共面。可辨识性、连通分量规范、通带/PSF/乘性差的边界由 [UPM 正本](../../science/sky/UPM.md)规定，不在登记卡另立科学模型。

## 生产入口与对象

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_upm_fit` 对应 `acsd.phase2.upm-fit`：读取 `p2_samples.json`，装配几何/配置，调用 `p2_sky_plane_build_joint`，保存 `p2_sky_plane.bin` 和拟合诊断。该 .bin 当前是模型序列化载体，不能按扩展名猜数组布局。apply 打开同一模型；writer 读其真实 hash/审计读数。

公共头为 [sky_plane.h](../../../lib/algorithms/coverage/include/astro/phase2/sky_plane.h)，实现为 [sky_plane.cpp](../../../lib/algorithms/coverage/src/sky_plane.cpp)。`p2_sky_plane_default_config`、`_default_adaptive_config`、`_geometry_from_controls`、`_derive_node_spacing`、`_build`、`_build_adaptive`、`_build_joint`、`_info`、`_save/open/close` 提供模型生命周期。

## 字段、单位和配置

| 对象 | 合同 |
|---|---|
| `P2SkySample` | 同帧身份、ICRS sky、背景值与其方差/权重、支持/质量 |
| `P2SkyPlaneGeometry` | control/footprint 派生的几何和可用范围；不能由目录名猜目标天区 |
| `P2SkyPlaneConfig` | 公共面节点、样条阶、逐帧梯度阶、规范模式、rank_rtol、稳健迭代、逐帧残差场及 worker |
| `P2SkyPlaneInfo` | 参数数/秩、条件数、自由度/残差、可辨识读数、model_hash 与模型配置 |
| 模型求值 | 背景值/差模沿实际信号面亮度单位；sky 为 degree，节点间距为 degree |

联合构建先求公共面和低阶项，再从残差建立逐帧场并去公共模，按配置重解。保留某个加权公共量是代数性质，不能据此认定真实低频天空已无偏恢复。单帧或断连区的延拓/规范不增加观测信息。

配置从 `p2_sky_plane_default_config` 和实际 adapter 解析得到；结构体字段与 CLI 白名单是两个消费面，不能只凭库字段宣称用户可配。乘性恒等、颜色差和 PSF 差也不能被加性拟合的成功状态吸收。

## 库 UPM 接口与生产边界

[upm.h](../../../lib/algorithms/coverage/include/astro/phase2/upm.h) / [upm.cpp](../../../lib/algorithms/coverage/src/upm.cpp)另有 `p2_upm_build/build_geo`、`_raw_weight`、`_save/open/info`、`_component_gauges`、`_convergence` 与 dense cache 接口，局部 session/工具可消费。它们不是当前 CLI fit→apply 数值链的模型载体。

库 `P2ControlObservation.control_ivar` 是背景估计方差的倒数；`p2_upm_raw_weight` 处理 quality/control_ivar。不能用点源 SNR²/F_ref²代替该统计量，也不能把几何可靠性或 support 命名为普适精度。

## 所有权、错误与验证入口

build 返回 opaque model，由调用方用对应 close 释放；输入样本借用至同步调用结束。模型参数/系数与必要缓冲驻留，求值按需执行；dense cache 是另外的计算缓存。worker 由宿主预算传入，固定块/帧归约仍需以实际容差验证，不凭线程局部和宣称跨 worker 逐位一致。

配置/几何缺失、文件损坏、不可计算与不可辨识诊断分开；模型返回码与 CLI 退出码不能直接等同。收敛或秩读数是解释模型的证据，不是新增研发审批关卡。

验证从本页 build_joint、info、save/open 和[拟合/应用链](../mosaic/pipeline.md)进入：已知加性场、每连通分量规范、弱重叠、断连、真低频星云、PSF/色差阴性世界、参数恢复与留出残差。尚无实测不能写已完成无接缝；生产差异与相邻接口限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
