# modules

本目录是模块说明层：MODULE_MAP 映射表加逐模块说明卡，记录每个代码模块的职责、落位与测试对应关系。

## 职责边界

- 放：模块一致性映射表与逐模块说明（职责、源码落位、公开头、测试）。
- 不放：模块工作细节与设计（在 docs/plugins/）；架构规则（在 docs/architecture/）；registry 生成件（在 registry/ 子目录）。

## 内容

- `MODULE_MAP.yaml` —— 模块一致性映射表：插件规范与代码落位的机器映射。
- `common.md` —— 共享基础库（HEALPix 核心、哈希、精度抽象）。
- `astro_image_io.md` —— 唯一 I/O 层：FITS/ahpx 读写、压缩、HiPS 读写。
- `calibration.md` —— master 产物生成与单帧图像校准。
- `dynamic_psf.md` —— 动态 PSF 建模与拟合质量代理。
- `star_detector.md` —— 单帧全图盲检测。
- `plate_solve.md` —— 星表匹配与 plate solve 解出 WCS。
- `photometric_calib.md` —— 测光定标与质量指标。
- `snr_estimator.md` —— 三层噪声模型与 SNR 估计。
- `healpix_drizzle.md` —— 球面 Drizzle 重投影与通量守恒累加。
- `healpix_browser_qt.md` —— HiPS/HEALPix 球面浏览器（可选构建）。
- `gaia_xpsd_client.md` —— XPSD 格式本地星表解析（离线）。
- `acr.md` —— 异构计算运行时（kernel registry、调度、执行器）。
- `orchestrator.md` —— normalize 阶段编排。
- `phase1_session.md` —— normalize 装配与进程内执行段。
- `phase2.md`、`phase2_int.md`、`phase2_rej.md`、`phase2_samp.md`、`phase2_upm.md` —— mosaic 阶段的统一模型、积分、排异、采样与 UPM 模块。
- `hips_p2.md` —— 输入哈希链与 manifest 规则。
- `phase3_proj.md`、`phase3_rsmp.md`、`phase3_fits.md` —— export 阶段的投影、重采样与 FITS 写出模块。
- `registry/` —— registry 模块的机读生成件与基线。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（模块与 ABI）。
