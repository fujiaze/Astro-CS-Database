# AstroCS Architecture

## 总览

AstroCS 是天文 CCD 图像校准与标准化数据库系统：

- 输入：单帧 FITS/XISF 亮场 + 校准母版（masterBias/Dark/Flat）；
- 模块链：I/O（astro_image_io）→ 校准（calibration）→ 星点检测
  （star_detector）→ PSF（dynamic_psf）→ plate solve（ipv）→ 测光定标
  （photometric_calib）→ 噪声/SNR（snr_estimator）→ 球面 Drizzle
  （healpix_drizzle）→ HiPS 产品（astro_image_io）→ Phase2 多帧统一光度
  模型（phase2: coverage→sampler→UPM→rejection→integration→HiPS）。
- 编排：orchestrator.exe（C++，DllLoader 动态加载各模块 DLL，stage1.json 驱动）。
- 浏览器：healpix_browser_qt（Qt6 独立查看器，可选构建）。
- 运行时：ACR（lib/acr）提供异构计算抽象（CPU/GPU），phase2 集成其 CPU
  reference 与 CUDA bridge。

## 分层

```text
orchestrator / stage2 CLI / browser        (应用层)
      └── phase2 (coverage/sampler/UPM/rejection/integration)
      └── 科学模块 DLL (calibration, star_detector, dynamic_psf, ipv,
                        photometric_calib, snr_estimator, healpix_drizzle)
      └── astro_image_io (FITS/XISF/HiPS/ahpx/compression, 唯一 I/O 层)
      └── common (healpix_core, crypto/sha256, compute 抽象)
```

## 关键原则

- lib/ 是唯一源码目录；run/ 是唯一运行输出目录；testdata/ 只读。
- 科学语义唯一实现（oracle/reference 并存，禁止重复 active science path）。
- I/O 唯一入口 astro_image_io（含 UPM 模型文件容器 aio_upm）。
- 正式运行入口只有 orchestrator.exe（Phase1）与 astrocs-stage2（Phase2）。

## 模块地图

见 docs/architecture/MODULE_MAP.md。
