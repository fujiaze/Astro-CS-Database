> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/ARCHITECTURE.md、docs/architecture/

# AstroCS 架构 (V19)

> 详细模块级文档见各 `lib/<module>/memory.md` 与 `docs/architecture/`。

## 总览

```text
Stage1 (单帧 → HiPS):
  READ_FITS → CALIBRATE → PLATESOLVE → PSF → PHOTOMETRIC → SNR →
  NSIDE → DRIZZLE → HIPS_WRITE → HIPS_VERIFY

Stage2 (多帧 → UPM → 马赛克):
  DISCOVER → COVERAGE_UNION → CONTROL_SAMPLE → UPM_FIT → BLOCK_PLAN →
  BLOCK_CALIBRATE → REJECT_INTEGRATE → HIPS_WRITE → HIPS_VERIFY
```

## 模块图

```text
astro_image_io       唯一 I/O: FITS/XISF/HiPS 读写、PipelineFrame、variance 产品
calibration          校准 (bias/dark/flat/cosmetic)
plate_solve          WCS+TAN/SIP 求解
dynamic_psf          Moffat4 PSF 拟合 (psf_fit_quality 语义)
photometric_calib    测光定标 (sigma_residual dex 单位)
snr_estimator        V19 三层模型: PhotometricCalibrationQuality /
                     PsfFitQuality / NoiseWeightModelV1
healpix_drizzle      球面 Drizzle + 方差传播 + 操作计数
phase2               UPM/rejection/integration (ivar 权重)
orchestrator         stage1/stage2 编排 (唯一正式入口 orchestrator.exe)
acr                  dormant 加速基座 (V19 未进入科学路径)
```

## 依赖图

```text
orchestrator → astro_image_io, calibration, plate_solve, dynamic_psf,
               photometric_calib, snr_estimator, healpix_drizzle
phase2       → astro_image_io, common/healpix, acr (kernel registry)
healpix_drizzle → astro_image_io (AIO HiPS + HEALPix I/O)
snr_estimator → 无 DLL 依赖 (纯 C++ 计算)
```

## 关键设计决策

1. **唯一 I/O**: 所有文件读写经 astro_image_io.dll
2. **HiPS 生产链**: Drizzle → AIO 直写; HISS deprecated
3. **科学权重**: 噪声/权重唯一来源为 NoiseWeightModelV1 (空背景噪声),
   星亮度派生量只作 QA
4. **CPU oracle 权威**: 任何 fast path/GPU 不得改变科学语义 (false negative=0)
5. **配置单源**: configs/ 模板 + schema, stage1/stage2 参数追踪 (P03-002)
