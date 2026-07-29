# v1.2 阶段目标与边界

## 最终阶段目标

在 CLI 模式下，用 TestData 的真实数据稳定完成：校准 → 单次星点检测共享 → PlateSolve → 标准 WCS/SIP → PSF → Gaia 光谱积分 → 测光 → SNR → Drizzle → HISS；再用银心三片 Red 32 帧完成球面梯度、稳健叠加和 HCSD，并用优化后的球面浏览器流畅查看。

## 本阶段不做

- 不开发完整业务 GUI；
- 不让 JavaScript 直接读取 PipelineFrame 或 DLL；
- 不重写已无回归的 PlateSolve 核心匹配算法；
- 不先改 HCSD 格式；
- 不扩展到所有目标的大规模科学生产，先完成银心标准验证。

## 正式完成定义

只有科学链、马赛克和浏览器三者共同通过，v1.2 才完成。CLI 工程测试通过但真实数据 `n_matched<=1`、`has_snr=false` 或浏览器卡顿均不得发布。
