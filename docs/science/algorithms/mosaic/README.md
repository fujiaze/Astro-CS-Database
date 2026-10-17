# Mosaic 算法

本目录现行科学算法：

- [Phase2 公共面/加性差模/P5 研究边界](PHASE2_UPM_IMPL.md)：现行 `sky_plane.cpp` Schur/IRLS/逐分量 gauge/own、退役 `upm.cpp` 的 F1–F6 有效保全、三帧非 SPD #80 数值诊断与研究 #76。
- [Phase2 单 Signal HiPS Writer](PHASE2_MOSAIC_WRITE.md)：真实 `p2_op_write` 数据/次序/单位/输出，混合响应的正确标签，以及 Writer→Export #79 provenance。
- [背景控制与退役 UPM 求解器补充](UPM_SOLVER.md)：保留对照与精度研究，不将 `M+C` 旧核误作当前生产。

接口与配置按[正式 Mosaic 模块](../../../detail/mosaic/modules.md)；投影后响应身份按[Export 模块](../../../detail/export/modules.md)。统计估计量与可辨识性分别引用[UPM](../../sky/UPM.md)、[噪声](../../noise_snr/NOISE_SNR.md)和[集成](../../integration/INTEGRATION.md)，无新增第二种产品科学合同。
