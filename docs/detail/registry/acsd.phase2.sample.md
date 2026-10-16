# Mosaic 背景控制采样

采样从已发现的逐帧 HiPS 读取局部背景观测，为公共面/逐帧差模拟合提供稀疏值及其自身方差。背景 control 精度、参考点源精度与最终像元精度是不同统计量；定义依 [UPM 正本](../../science/sky/UPM.md)和[噪声正本](../../science/noise_snr/NOISE_SNR.md)。

## 实际入口与交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_sample` 对应 `acsd.phase2.sample`：读取 `p2_coverage.json`，生成/复用 `p2_frame_id`，以 probe/fill 调用 `p2_sample_controls_cached_with_halos`，写 `p2_samples.json`。fit 消费 sky_samples、control 几何、观测/帧映射；apply 还使用控制统计量解释校正方差。

公共头为 [sampler.h](../../../lib/algorithms/coverage/include/astro/phase2/sampler.h)，实现为 [sampler.cpp](../../../lib/algorithms/coverage/src/sampler.cpp)。基础入口 `p2_sample_controls`、缓存入口 `p2_sample_controls_cached` 和 with_halos 共享采样；后者接可空 `P2GaiaHaloCaps`，签名存在不表示运行确实进行了 Gaia 查询。当前 adapter 的停用帽路径应按实际分支理解，不能将参数声明当已生效掩膜。

## 数据与采样语义

| 对象 | 字段/单位 |
|---|---|
| `P2ControlNode` | control_id、tile_ipix、gx/gy、ICRS ra_deg/dec_deg、leaf_ipix |
| `P2ControlObservation` | frame/control/leaf ID，value、uncertainty、control_variance、control_ivar、support、snr/available、quality_flags |
| value/uncertainty | 原信号面亮度单位及同单位标准误；不是未经球面化的 ADU 积分量 |
| control_variance/control_ivar | patch 背景估计量的方差及倒数；与单 leaf ivar 分开 |
| counts | geometry controls、accepted controls、overlap controls 和 accepted observations 分别登记 |

网格由 union 几何和配置决定，不由 SNR 定义。背景 patch 用有限/support 样本，经亮端裁剪、局部容差、污染/保留比例与 Catalogue veto。保留负值；Catalogue 查询不进行新的星点检测。sky_samples 可包含单帧 clean 区域，重叠约束所需观测集合与之分开。

当前 control 方差含 `control_k_corr` 与中位数近似；这只是特定估计器/几何的模型，不能把默认 1.4 称全域物理常数。空间相关性与有效样本数的适用域由噪声正本解释。

## API、配置与所有权

`p2_sampler_default_config` 给默认；`p2_frame_id` 返回内容派生 uint64 ID，0 为失败；`p2_stats_median/mad` 为共享统计入口。buffer 由调用者 probe 后分配，out_n 给实际需求；容量截断规则依头文件，不能把调用成功当已完整收齐。

`P2SamplerConfig` 承载网格、patch、clip、保留比例、Catalogue/星帽、k_corr 和 cpu_workers。CLI 的 `model` 实际消费由 `p2_sample_cfg_from_doc` 解析，和库默认/工具入口区分；不在本页另列无消费者键。halo/回退参数存在时也不代表相应分支可用。

按 cell/帧工作分片，worker 使用独立 AIO 读句柄，输出按固定索引组织；预算由 Runtime 传入，不自选硬件线程数。大中间 cells 随 coverage 增长，不能声称内存只随最终 accepted 点数增长。取消由上层协作处理。

## 错误、验证与能力边界

参数、帧身份、open/read、容量/规模和异常由返回码及 err 文本说明；单点丢弃计数不是整个节点执行失败。缺 Catalogue、缺 ivar 和零背景尺度分别记录，不能补出虚假的 reference precision。

验证从本头 probe/fill、`p2_sky_patch_estimate` 及 [Mosaic 管线](../mosaic/pipeline.md)进入：纯噪声/已知梯度、亮源污染、空洞、缺产品、计数守恒、控制方差 Monte Carlo、线程等价。现有规整 sparse SNR 或 Catalogue 的可读性不证明 canonical 控制已贯通；相邻差异只见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。本页未重跑验证。
