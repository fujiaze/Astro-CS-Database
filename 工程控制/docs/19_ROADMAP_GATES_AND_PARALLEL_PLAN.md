# 路线、Gate 与并行计划

- G9：v1.1事实基线与共享检测主线确认；
- G10：T1–T4 和全部 Master 映射；
- G11：权威星对WCS闭环Gate v2、PlateSolve全量无回归、WCS契约冻结；
- G12：SNR/HISS 和 Stage1 全量；
- G13：银心三片真实梯度与叠加；
- G14：浏览器持久 I/O、异步加载和缓存；
- G15：GPU Tile Renderer 与可视性能；
- G16：统一回归、用户结果包和发布候选。

P15 浏览器工作可在 G10 后与 P11–P13 并行，但最终 P16 的真实视觉验收依赖 G13 的银心 HCSD。
