# AstroCS 模块索引 (V19)

| 模块 | 目录 | 职责 | 入口 | 构建 |
|---|---|---|---|---|
| AIO | lib/astro_image_io | 唯一 I/O + HiPS 产品 | aio_hips_* / aio_pipeline | make |
| calibration | lib/calibration | 校准 | ac_calibrate_frame | make |
| plate_solve | lib/plate_solve/cpp/ipv | WCS 求解 | ipv_solve_from_memory | make |
| dynamic_psf | lib/dynamic_psf | PSF 拟合 | dpsf_fit_batch | make |
| photometric_calib | lib/photometric_calib/cpp | 测光定标 | pc_calibrate_simple | make |
| snr_estimator | lib/snr_estimator | V19 噪声三层模型 | snr_noise_model_v1 | make |
| healpix_drizzle | lib/healpix_db/healpix_drizzle | Drizzle+方差 | hp_drizzle_run_hips | make |
| phase2 | lib/phase2 | UPM/排异/叠加 | astrocs-stage2 | cmake |
| orchestrator | lib/orchestrator/cpp | 编排 | orchestrator.exe | make |
| acr | lib/acr | dormant 加速基座 | (未进入科学路径) | cmake |
| common | lib/common | HEALPix core / crypto | - | header |

每个模块维护独立 `memory.md` (决策/进度/未完成项)。
