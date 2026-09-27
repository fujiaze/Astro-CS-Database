# science

本目录是科学公式与科学定义的权威层：全链共用的量纲、信号、方差语义与各专项 SCI 条款正本，其他文档的科学断言向此收敛。

## 职责边界

- 放：科学公式、定义、适用域、判据口径与不确定度传播规则（SCI-* 条款）。
- 不放：算法推导与实现锚定（在 docs/science/algorithms/）；模块工作细节（在 docs/plugins/）；文献与一手查证（在 docs/references/ 与 docs/research/）；工程门与容差表（在 docs/science/algorithms/GATES_AND_TOLERANCES.md）。

## 内容

- `UNIFIED_SCIENCE_MODEL.md` —— 跨阶段统一科学模型：量纲、信号、方差语义总纲。
- `SCIENCE_SCOPE.md` —— 科学范围与核心科学方法的边界。
- `CALIBRATION.md` —— 定标科学公式（bias/dark/flat）与不确定度传播。
- `STAR_DETECTION.md` —— 星点检测定义与检测阈值口径。
- `PSF.md` —— PSF 模型、FWHM 与质心/形状参数定义。
- `PSF_SIGNAL_WEIGHT.md` —— PSF 信号权重与帧级 SNR 定义。
- `ASTROMETRY.md` —— WCS/SIP 天测公式与外部闭环口径。
- `PHOTOMETRY.md` —— 测光公式与测光一致性判据的误差预算。
- `NOISE_MODEL.md` —— 噪声模型与 σ 估计公式（含星点掩膜与天空预算）。
- `CONTROL_WEIGHT_SNR.md` —— 控制点权重与 SNR 三口径的适用域与公式。
- `DRIZZLE.md` —— Drizzle 重采样公式与面亮度归一。
- `INTEGRATION.md` —— 逆方差叠加与 SNR 重建公式。
- `UNCERTAINTY_AND_COVARIANCE.md` —— 不确定度与协方差传播的统一口径。
- `PHASE2_UPM.md` —— 统一相对模型（UPM）公式与可辨识性。
- `REJECTION.md` —— 逐像素排异定义与合法性窗口。
- `PHASE3_HIPS_TO_FITS.md` —— HiPS 到 FITS 投影导出公式与核语义。
- `ACR_EQUIVALENCE.md` —— ACR 等价性的科学边界（生产不可达）。

## 上游

上游：docs/ASTROCS_DESIGN.md §1（项目定位）、§2（核心科学方法）、§3.1（数据对象）、§4.2（Phase1 节点流程）、§4.4（输出合同）、§5（mosaic 各节）、§6（export 各节）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。
