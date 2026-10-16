# Mosaic 单信号 HiPS 写出

writer 将集成信号和内部归一对象写为一个 signal 科学图像通道。产品范围来自[最高设计](../../ACSD_DESIGN.md)与[Mosaic 管线](../mosaic/pipeline.md)，不能以共用写库支持多 flags 反推本阶段必须发布误差层。

## 生产入口和输入

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_write` 对应 `acsd.phase2.write`。它消费 `p2_integrated.json` 和相应 signal/support/wsum 工作数组，读取 coverage、samples、sky-plane 和 rejection 的实际来源，写马赛克及 `p2_final.json`。

生产入口是七节点 `P2NodeModule`，不是 `lib/algorithms/coverage/tools/stage2.cpp::main`。工具的配置、默认、退出整数与产物能力不能直接当正式 `mosaic` 命令事实。

## 写库 API 与数值

共用 [aio_hips.h](../../../lib/infrastructure/aio/include/aio_hips.h) / [aio_hips_writer.cpp](../../../lib/infrastructure/aio/src/hips/aio_hips_writer.cpp)提供 begin、write_signal_support_tile、set_provenance、finalize/abort 和 verify_product_set。当前 `p2_op_write` 明确设置 `flags=AIO_HIPS_PRODUCT_SIGNAL`。

| 输入/输出 | 合同 |
|---|---|
| integrated signal | 已接受样本的合成球面面亮度；真实单位/光度状态来自输入链 |
| support、wsum、内部方差 | 仍用于有效性、归一及内部统计；不发布为第二科学图像层 |
| `AstroSphereTileView` | 以 flux_sum/covered_area 适配 signal；这两个是写入视图中间量，不改产品为积分通量 |
| nside/order | 叶 nside、NESTED、tile=512；实际叶阶与 tile 阶分清 |
| 输出 | signal 层级、properties、MOC、manifest 与运行摘要；无覆盖 NaN，真实零/负值保留 |

tile FITS 与 NESTED local 的转换复用共享实现，不能各端复制手性公式。输入/模型 hash、排异 profile 与实际可用性由真实工作产物读取，不编造通过状态。最终单信号形态不能被 `uncertainty_available` 的内部模型事实误写成 variance/ivar 已发布。

## 响应、所有权与并行

writer 登记 `response_distribution` 并可写混合响应；不同 filter 字符串不阻止用户合并。单个 obs_filter 不自动等于精确总体响应，尤其有效帧集/权重随位置变化时。当前响应分布边界集中见[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

tile 变换可在 Runtime 预算下并行写独立槽，实际 writer 句柄按序串行调用；输入数组/视图由 adapter 持有至写调用结束。finalize/abort 释放句柄；AIO 完成、目录发布和运行摘要必须分别检查，不由单次 write 返回推断整个目录原子性已成立。

## 配置、错误与验证入口

读取本次输出位置、dtype/精度、目标几何和真实 provenance；不独立选择科学权重。I/O、模型读回、产品一致性或哈希失败通过节点 Result 上行，和合法精度能力受限分开。最终发布纪律依[原子发布合同](../../engineering/contracts/ATOMIC_PUBLISH.md)。

验证从 `p2_op_write` 与 `aio_hips_verify_product_set` 进入，独立枚举仅一个科学通道、真实零/NaN、所有已声明层级、tile 手性、来源 hash、混合响应和失败残留。内部 support 仍参与计算是源码事实；本页未执行写出或重开实验，不将目录自洽等同科学保真。
