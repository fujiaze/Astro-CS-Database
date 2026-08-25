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
- common 权威：healpix_core 为 HEALPix NESTED 唯一实现（B4-01 去重为单源，drizzle 转依赖 lib/common/healpix，另一份 deprecated shim + 机器门禁，见 HEALPIX_MAPPING.md）、crypto/sha256 为 DATA-FRAME-ID-001 唯一实现。
- 正式运行入口只有 orchestrator.exe（Phase1）与 astrocs-stage2（Phase2）。

## 模块地图

见 docs/architecture/MODULE_MAP.md。

## 四条链（T300 生产接线契约）

1. **数据链**：原始帧 → Stage1 产品 (FITS/HiPS) → HiPS/AIO → Phase2 sampling/UPM/rejection/integration → 标准 HiPS (DATA_FLOW.md, PIPELINE.md)
2. **控制链**：CLI (`main` orchestrator.exe / astrocs-stage2) → config parser/schema → orchestrator (DllLoader) → module API → diagnostics/error (ERROR_MODEL.md)
3. **执行链**：串行控制面 → CPU并行数据面 (OpenMP per-pixel/tile) → 异步 I/O → ACR CPU/GPU Dispatcher (register_phase2_acr_kernels, weight_mode=ivar→cpu fallback ACR-IVAR-001) → 同步/归约 (THREADING_MODEL.md)
4. **生命周期链**：对象/缓冲区创建者、所有者、借用者、释放线程、失败/取消清理 (OWNERSHIP_AND_LIFETIME.md, IO_AND_ATOMICITY.md)

## 生产调用路径 (T300)

- Stage1: `docs/architecture/production_call_paths_stage1.csv` (7 路径, entry_symbol→source_symbol 全可达)
- Stage2: `docs/architecture/production_call_paths_stage2.csv` (10 路径, 含 ACR Dispatcher 接线, 仅库实现但入口不可达视为未接线)

仅由 CLI 入口可达视为接线，已验证每个路径的 source_symbol 在目标 target 的编译单元存在。
