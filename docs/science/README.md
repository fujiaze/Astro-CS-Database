# ACSD 科学正本阅读入口

首先阅读 [最高设计](../ACSD_DESIGN.md)、[统一模型](unified/UNIFIED_SCIENCE_MODEL.md)、[数据语义](unified/DATA_SEMANTICS.md) 与 [适用域](unified/SCIENCE_SCOPE.md)。**Normalize 的唯一横向科学因果链**是 [normalize/NORMALIZE_SCIENCE.md](normalize/NORMALIZE_SCIENCE.md)，不是目录下各旧算法分册的多套拼接。

阶段一按因果关系阅读：[校准](calibration/CALIBRATION.md) → [IPV 天测的物理几何](detection/ASTROMETRY.md) → [Gaia 引导星点](detection/STAR_DETECTION.md) 与 [PSF](psf/PSF.md) → [Gaia XP 光度](photometry/PHOTOMETRY.md) → [参考 SNR](noise_snr/NOISE_SNR.md) → [球面 Drizzle](drizzle/DRIZZLE.md)。IPV 在解算过程中先私有盲检候选星，并不要求原始 FITS 已有 WCS；这个 solver 候选目录不同于求解后用于科学测光的 Gaia 引导目录。

阶段二：参考精度空间重建仍见 [NOISE_SNR](noise_snr/NOISE_SNR.md)，相对天空在 [UPM](sky/UPM.md)，最终合成见 [INTEGRATION](integration/INTEGRATION.md)。阶段二最终**仅一个 signal 科学通道的 HiPS**，但阶段一可保留科学必需的 PSF/噪声/稀疏控制辅助对象。阶段三 [HiPS → FITS](projection/HIPS_TO_FITS.md)、[重采样](resample/RESAMPLE.md)。

代码实现结构与当前真实功能分离阅读：[阶段一工程实现](../engineering/architecture/NORMALIZE_PIPELINE.md) 和 [文档收敛与事实](../engineering/governance/NORMALIZE_CONVERGENCE.md)。旧 science/algorithms/normalize 与 detail/registry 可作为源码定位/历史接口，不拥有与 Normalize 科学正本冲突的第二套物理定义。历史 P1–P5 实验按自身输入/方法保留原结论，不自动升级为当前 E2E 证明。
