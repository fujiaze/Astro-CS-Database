# 稳健叠加与 HCSD 质量规范

真实叠加必须使用 `has_snr=true` 的 32 份 HISS，权重为可追溯的局部 SNR²；每像素或每 Tile 记录覆盖数、有效样本数和拒绝数的统计摘要。

验证：

- 3 样本以上区域的异常值剔除；
- 合成注入卫星线/热点的拒绝；
- SNR 权重与等权结果差异符合公式；
- 输出确定性或明确的浮点容差；
- HCSD leaf index、sorted ipix、meta/provenance 可独立读取。

Orchestrator 进度必须反映真实工作；不得保留空 `STACK` 阶段。可合并为 `MOSAIC_STACK`，或把 gradient/robust_stack/write_hcsd 分成真实事件。
