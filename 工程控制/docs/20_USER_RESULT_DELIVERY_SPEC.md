# 用户结果交付规范

最终交付目录 `dist/AstroCS-v1.2-validation/` 至少包含：

- CLI/算法构建产物及版本；
- 优化后的浏览器；
- 银心 no-gradient 与 gradient HCSD；
- 相同视角截图和可选短录屏；
- T1–T4 清单与校准映射；
- WCS/Photometry/SNR/Mosaic/Browser 报告；
- 一键回归入口和 SHA-256；
- 已知限制、所需 Gaia/TestData 外部路径；
- 用户打开结果的最简说明。

不把 TestData 或 Gaia 私有大数据复制进发布包，引用路径和 hash 即可。
