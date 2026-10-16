# Mosaic 输入发现与覆盖并集

本节点枚举逐帧 HiPS 的产品几何，计算目标阶和覆盖并集，给后续采样与空间调度提供帧/区域映射。用户可自由组合设备、滤镜和带宽；单位/几何损坏仍需报具体错误。科学边界见[最高设计](../../ACSD_DESIGN.md)和 [Mosaic 管线](../mosaic/pipeline.md)。

## 入口与生产者、消费者

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_coverage` 对应 `acsd.phase2.coverage`，调用 [coverage.h](../../../lib/algorithms/coverage/include/astro/phase2/coverage.h) 的
`p2_coverage_build(hips_paths,n_inputs,out)`。当前分两次查询容量/回填，并写 `p2_coverage.json`，供 sample、apply、write 等消费。局部 `p2_session` 也可调用该库。

[coverage.cpp](../../../lib/algorithms/coverage/src/coverage.cpp)检查实际产品，保留各帧 filter 字符串而不作跨帧全等拒绝。坐标/ordering、tile_width、signal/support 语义及属性可解析性是数据检查，不能写成统一物理通带的证明。

## 字段、坐标与内存

| 类型/字段 | 合同 |
|---|---|
| `P2HipsInputInfo` | hips_path、frame_id、max_leaf_order、n_tiles、filter_passband、frame_type、hips_ordering |
| `P2MocCell` | order/ipix 整数 NESTED 位置；须随实际层级解释，tile 阶与叶阶相差 9 |
| `P2CoverageResult` | 调用方分配 inputs/union_cells，n_inputs/n_union_cells/target_order/status/error |
| target_order | 当前取各输入最大叶阶的最小值；不是独立配置的科学目标分辨率已实现 |
| union | 几何并集，允许断连；不提供点源信息量或面亮度统计权重 |

`p2_coverage_free` 清零 POD，不释放调用方数组；调用者拥有所有缓冲。整数集合处理无独立线程预算，文件读取经 AIO；Runtime 节点管理生命周期和取消。覆盖数/并集不是背景有效观测边，也不自动给出 UPM 连通分量的绝对零点。

## 公共辅助 API 与错误

同头的 `p2_coverage_support_classify` 区分 uncovered、covered_unsupported、supported、unavailable；coverage/support 的 known 位不能用零替代。另有 `p2_tile_boundary_owner`、`p2_deterministic_reduction_order` 及 `p2_weight_source_token_reject`。它们分别处理边界归属、整数定序及具名来源检查，不生成科学权重，也不能据头内辅助 API 推断 CLI 全部调用它们。

主 build 返回 0/非零并带 `error[512]`；probe 容量与普通失败需要结合需求量判断。缺属性、坏数据、非 NESTED 等按真实 reader 错误上行；空 filter 或不同 filter 字符串不自动构成物理坏帧。

## 验证入口与边界

从 `p2_coverage_build` probe/fill、[coverage 实现](../../../lib/algorithms/coverage/src/coverage.cpp)及 [Mosaic 管线](../mosaic/pipeline.md)的调用者进入核对：已知并集、断连、降阶父聚合、空集、缺产品、同天区几何与混合通带。target_order、帧身份及输出响应的当前限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页未进行运行验证。
