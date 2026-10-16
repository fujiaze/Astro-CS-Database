# Mosaic 数值集成与精度消费

集成把同位置已校正/已接受样本作数值归约。reducer 与权重策略分工由[集成正本](../../science/integration/INTEGRATION.md)定义；参考点源与面亮度精度的区别由[噪声正本](../../science/noise_snr/NOISE_SNR.md)定义。最终对外只发布 signal HiPS，内部 support、wsum 和方差不因此删除。

## 实际入口与权重来源

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p2_op_integrate` 对应 `acsd.phase2.integrate`，读取 `p2_corrected.json` 与 `p2_rejection.json`，构造外部数值 weights，调用 `p2_validate_candidate_weights` 和 `p2_integrate_pixel`。输出 `p2_integrated.json` 及 signal/support/wsum/nused/nrej 工作数组供 writer。

当前优先用已声明可用的 corrected variance，其次已有逐样本 ivar，缺失才使用兼容 SNR 链。[weight_chain.cpp](../../../lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp)处理来源/配对，`snr_path_requested/effective/reason` 记录请求与实际路径。dense 请求当前明确不可用。

`p2_sparse_layer_probe` 读取属性 `ACSD_SPARSE_SNR_LAYER` 引用的规整网格；不是 canonical `frame_science.json` reader。不存在层时的具名帧级路径、声明层损坏的错误和真正 canonical 可估计模型必须分开。兼容 SNR²/F_ref²得到 I_F，不能直接宣称面亮度最优权重；转换还需同条件 T。具体限制见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，本页不切换算法。

## reducer 公开合同

签名头 [integrate.h](../../../lib/algorithms/coverage/include/astro/phase2/integrate.h)，实现 [integrate.cpp](../../../lib/algorithms/coverage/src/integrate.cpp)。

| 对象/入口 | 合同 |
|---|---|
| `P2PixelStack` | values、weights、support、accepted、count；调用方拥有数组 |
| values/weights | 同一个面亮度标度；权重是调用方提供的数值，若解释逆方差则单位为信号单位平方的倒数 |
| 空 weights/support/accepted | 库 API 分别表示等权、全覆盖、全接受；CLI 实际提供权重数组 |
| `p2_integrate_pixel(in,out)` | 固定输入索引顺序归约正权样本；同时返回 signal、support、n_used 和诊断计数 |
| support reducer | max(accepted support)，包含合法零权 accepted；不是精度权重或精确覆盖并集 |
| `p2_validate_candidate_weights` | 有限非负，零合法；负/NaN/Inf 非法 |

accepted 样本中的非有限值、非法 support 或 weight 不能被 reducer 静默当正常数据。有限零和负信号合法。成功 signal 是加权均值；无有效贡献为 NaN。

## 状态、资源与错误

`P2IntegrateStatus` 现有六态：OK、NO_CANDIDATES、ALL_REJECTED、ZERO_VALID_WEIGHT、INVALID_INPUT、NON_FINITE_OUTPUT。最后一态区分合法输入归约溢出，不能继续写成仅五态，也不能把溢出结果当有效零。

纯 reducer 不持有 cache、不做 I/O、不分配长期大面，不独立建线程池。调用者在像素/tile 间按 Runtime 预算并行，每像素内序固定；资源取消与工作文件生命周期由 adapter 管理。rc 与 status 需分别检查，局部枚举不直接当 CLI 退出码。

普通像元 ivar 平均的最优性依相同被估量、独立随机误差和固定接受集合；PSF/通带失配、共同母版和数据驱动排异不能靠 1/Σw 宣称完整误差传播。最终无第二个 support/variance/ivar 科学图像层。

## 验证入口

从本页 reducer/weight-chain 与[Mosaic 管线](../mosaic/pipeline.md)进入，检查已知加权均值、零权惰性、accepted support 最大值、六态、极端溢出、帧索引、请求/实际路径和 T 不同的独立反例。[稀疏重建研究入口](../../../eng/tests/synthetic/test_p4_dense_snr.py)不自动验证 canonical 生产转换；本页未运行数值测试。
