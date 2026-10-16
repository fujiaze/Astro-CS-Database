# Mosaic 候选栈排异

排异决定同一天球位置哪些帧样本进入集成，不合并数值、不生成权重。方法与小样本定义来自[排异正本](../../science/integration/REJECTION.md)；预测误差、实际保留集合与条件方差需按[噪声正本](../../science/noise_snr/NOISE_SNR.md)解释。

## 生产入口与步骤

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_reject` 对应 `acsd.phase2.reject`，读取 corrected 元数据和样本工作面，调用 planning、资格收集、显式 kernel、大尺度 mask 后处理。输出 `p2_rejection.json` 及 accepted、nrej、candidates、逐样本 mask 工作面，由 integrate 消费。

公开头 [rejection.h](../../../lib/algorithms/coverage/include/astro/phase2/rejection.h)，实现 [rejection.cpp](../../../lib/algorithms/coverage/src/rejection.cpp)。

| 接口 | 合同 |
|---|---|
| `p2_reject_plan_resolve` / `_resolve_n` | AUTO 在 planning 解析；输入 nominal 为几何可贡献帧数，不是资格过滤后 n_eff |
| `p2_collect_candidate_stack` | frame-major strided gather，产 compact 栈与 source_indices |
| `p2_reject_stack_ex` | 只接显式 method；decision reasons 和栈 status 分开 |
| `p2_large_scale_apply` | 低/高 mask 原地做连通结构生长，只增拒绝范围 |
| `p2_rejection_semantic_id`、`_applicability`、`_percentile_band_min_n` | 返回实际方法身份、适用提示和小 N 策略读数 |
| `p2_reject_stack` | 连续输入兼容 API，不作为 CLI 实际 gather 路线描述 |

## 数据、索引和方法

`P2CandidateStack` 包含 double values/weights、frame_ids、count 和可选 prior_sigma/prior_sky。values 沿 corrected 球面面亮度单位，weights 必须与所估量/单位配对。kernel 不重新解释 sky 或投影，compact 样本关联真实原帧必须经 source_indices。

逐样本 reason 为 ACCEPTED、REJECTED_LOW、REJECTED_HIGH、UNDERDETERMINED；低/高由相对阈值方向决定，与原值是否为负无关。栈级 status 另含 OK、MIN_SAMPLES、ALL_REJECTED、INVALID_INPUT、INVALID_CONFIGURATION、INVALID_METHOD、INTERNAL_ERROR。

生产 `acsd_adaptive_pixel` 当前小 N≤3 保守不排异，4–5 percentile，N≥6 winsorized；其它 profile 和显式方法有各自解析。方法枚举还包括 sigma、averaged、linear-fit、ESD、RCR、median-sigma、minmax 及显式 prior-sigma 入口，不能把枚举全集称 AUTO 的生产输出。percentile 的电平比例判据不等于预测噪声 sigma；其适用域见科学分册。

## 配置、所有权与边界

配置来自 `reject` 的方法/profile/normalization 和 large_scale；参数按方法分结构，不能用同名 low/high 跨方法解释。当前 CLI large_scale 默认开与库/工具默认关分开记录，不将源码默认差异改写为科学结论。

内核无文件 I/O/线程预算，调用者提供 reasons/scratch/输入缓冲；像素/tile 并行由 Runtime 组织。large_scale 需要帧内邻域 mask，不能在任意独立微块上直接声称相同结果。调用 rc 与 status 都要检查，空栈 status 属排异域，不冒用集成 NO_CANDIDATES。

同头的 `p2_reject_classify` 和 probability 校准接口是另一组库能力；没有真实生产输入/消费者证据时，不将其宣称为 CLI 已完成污染机制分类或移动源独立层。最终产品仍为一个 signal HiPS。

## 验证入口

[路由单元入口](../../../eng/tests/unit/test_rejection_routing.py)检查 planning，[Mosaic 管线](../mosaic/pipeline.md)追至真实生产。独立注入检验覆盖无污染、异常点/轨迹、负信号、空栈、小样本、源索引映射、结构生长边界和选择偏差。当前科学/实现边界集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页未执行测试。
