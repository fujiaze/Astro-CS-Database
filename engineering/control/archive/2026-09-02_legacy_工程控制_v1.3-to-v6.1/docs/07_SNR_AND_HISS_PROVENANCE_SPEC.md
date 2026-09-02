# SNR 与 HISS provenance 规范

Photometric 成功后必须生成有限的 `snr_phot`、PSF 球面控制点和 HISS 稀疏 SNR 模型。SNR 的像素→球面转换必须使用修正后的完整 WCS/SIP。

HISS 必须记录：format_version、source_file/hash、T1–T4 system_id、filter canonical/raw、Master 文件/hash、PlateSolve mode、WCS closure summary、photometry counts/scale/residual、SNR point count、nside/pixfrac、软件 commit/config hash。

正式银心输入必须全部 `has_snr=true`。若某帧不能形成 SNR，应显式失败或从科学数据集排除并说明，不得静默等权。
