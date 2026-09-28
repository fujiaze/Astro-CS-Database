# algorithms_phase1

本目录存放 normalize 阶段 8 个科学模块的插件工作细节：从原始 light 帧到标准化单帧产品的逐节点职责与边界。

## 职责边界

- 放：normalize 各节点模块（定标、修复、检测、PSF、天测、测光、噪声 SNR、Drizzle）的工作细节。
- 不放：科学公式正本（在 docs/science/）；算法推导与门表（在 docs/science/algorithms/）；阶段详细设计（在 docs/design/PHASE1_DETAILED_DESIGN.md）。
- 说明：phase1 仅为文档分组的内部指代，代码中算法模块并联放置。

## 内容

- `01_calibration.md` —— 减偏置、暗流、平场的定标与不确定度传播。
- `02_cosmetic.md` —— 坏点（hot/cold 像素）、坏列等异常像素的检测与修复，validity 标志（不做宇宙线剔除，见该篇 §1）。
- `03_star_detection.md` —— 源检测：位置、质心/矩、源身份与 selection function 参数。
- `04_psf.md` —— 空间变化 PSF 模型的估计、参数、残差与适用域。
- `05_platesolve.md` —— 参考星表匹配解算天体测量解，生成并验证 ICRS WCS。
- `06_photometry.md` —— PSF 拟合域测光，把本帧信号映射到统一线性测光坐标系。
- `07_noise_snr.md` —— 逐像素噪声、逐源 SNR、深度、点源信息量与帧级 SNR 的估计。
- `08_drizzle.md` —— 按 WCS 重采样到球面 HEALPix/HiPS 格点，输出标准化单帧产品。

## 上游

上游：docs/ASTROCS_DESIGN.md §4.2（Phase1 节点流程）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。
