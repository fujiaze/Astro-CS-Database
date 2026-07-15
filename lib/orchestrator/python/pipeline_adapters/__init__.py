"""
Pipeline Adapters - 5个管线阶段适配器

每个适配器将一个 C++ 模块封装为 PipelineStageHandlerC:
- calibrate_adapter:    STAGE_CALIBRATE   (calibration DLL)
- platesolve_adapter:   STAGE_PLATESOLVE  (plate_solve DLL + Gaia DB)
- psf_adapter:          PSF_FIT           (dynamic_psf DLL)
- photometric_adapter:  STAGE_PHOTOMETRIC (photometric_calib DLL)
- drizzle_adapter:      STAGE_DRIZZLE     (healpix_drizzle DLL)
"""
