# Module Map

| 模块 | 路径 | 产物 | 职责 |
| --- | --- | --- | --- |
| common | lib/common | header-only/静态 | HEALPix core、SHA-256、compute traits |
| astro_image_io | lib/astro_image_io | astro_image_io.dll | FITS/XISF/HiPS/ahpx/压缩/UPM 容器 |
| calibration | lib/calibration | astro_calibration.dll, cosmetic_corrector.dll | 主帧生成/图像校准/坏点修复 |
| dynamic_psf | lib/dynamic_psf | dynamic_psf.dll | 动态 PSF 建模/拟合 |
| star_detector | lib/star_detector | star_detector.dll | 星点检测/质心 |
| ipv (plate_solve) | lib/plate_solve/cpp/ipv | ipv_solver.dll | 星表匹配/plate solve/WCS |
| photometric_calib | lib/photometric_calib | photometric_calib.dll | 测光定标/流量校准 |
| snr_estimator | lib/snr_estimator | snr_estimator.dll | 三层噪声模型/ivar |
| gaia_xpsd_client | lib/gaia_xpsd_client | gaia_client.dll | Gaia DR3 查询/缓存 |
| healpix_drizzle | lib/healpix_db/healpix_drizzle | healpix_drizzle.dll | 球面 Drizzle/方差传播（active） |
| healpix_browser_qt | lib/healpix_db/healpix_browser_qt | healpix_browser_qt.exe（tool 分类，非发布） | HiPS 浏览器（optional） |
| healpix_stack | lib/healpix_db/archive/legacy/healpix_stack | —（归档，不重建） | 稀疏堆栈/梯度（archived，不重建） |
| phase2 | lib/phase2 | phase2.a（静态库；V5 无独立 exe） | coverage/sampler/UPM/rejection/integration |
| orchestrator | lib/orchestrator/cpp | （V5 迁移：编排并入 astrocs CLI pipeline driver，无独立 exe） | Phase1 编排（历史） |
| acr | lib/acr | （V5 不接入；源码保留为配置守卫参考） | 异构计算抽象（dormant） |
| tools | tools/（仓根 tools/，非 lib/） | 脚本 | 工程批处理、诊断、一致性检查 |

## 每个 shipping module 的详细文档

docs/modules/<module>.md（L5 模板）。
