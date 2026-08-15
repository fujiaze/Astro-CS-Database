# Data Flow

## Phase1（单帧管线）

```text
FITS/XISF 亮场 + 母版
  → aio read (astro_image_io)
  → calibration (bias/dark/flat + cosmetic)
  → star_detector (星点表)
  → dynamic_psf (PSF 质量)
  → ipv plate solve (WCS/astrometry)
  → photometric_calib (flux 定标)
  → snr_estimator (SNR/ivar)
  → healpix_drizzle (球面投影/方差传播)
  → HiPS 写 (signal/support/variance/ivar)
  → orchestrator stage 编排 + 日志/诊断
```

## Phase2（多帧统一模型）

```text
多帧 Phase1 HiPS
  → coverage (MOC union, target_order)
  → sampler (control cell + patch estimator + SNR catalogue)
  → UPM build (Huber IRLS + ivar 权重 + 弱零锚 + 连通分量)
  → UPM persist (sparse JSON via aio_upm; dense cache 可选)
  → block plan/calibrate (每帧 frame_id → C(frame, leaf))
  → rejection (7 种: None/Sigma/Winsorized/AveragedSigma/LinearFit/ESD/RCR)
  → integrate (加权均值 + support reducer)
  → HiPS 写 + verify
```

## 数据契约

见 docs/contracts/DATA_SEMANTICS.md 与 docs/TRACEABILITY.csv DATA-* 行。
