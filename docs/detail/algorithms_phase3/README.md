# algorithms_phase3

> 上游：docs/ASTROCS_DESIGN.md §6.2（export 流程）、§6.3（投影算法）。

本目录存放 export 阶段 3 个科学模块的插件工作细节：投影、重采样与 FITS 写出。

## 职责边界

- 放：投影 registry、重采样采样核、FITS 流式写出三模块的职责与边界。
- 不放：科学公式正本（在 docs/science/）；算法推导（在 docs/science/algorithms/）；阶段详细设计（在 docs/detail/PHASE3_DETAILED_DESIGN.md）。
- 说明：phase3 仅为文档分组的内部指代，代码中算法模块并联放置。
- 佐证：科学断言以 docs/science/ 为权威，佐证要求见 docs/engineering/DOCUMENT_GOVERNANCE.md §2。

## 内容

- `14_projection.md` —— WCS 投影 registry：内置投影算法的适用域、奇点、坐标语义与正反变换。
- `15_resample.md` —— 按输出像素中心定位输入 HEALPix，按采样核重采样并传播不确定度。
- `16_fits_output.md` —— 把重采样产品流式写出为测量意义明确的 FITS 文件（HDU、WCS、provenance）。
