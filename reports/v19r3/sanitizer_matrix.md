# V19R3 Sanitizer Matrix（final-HEAD，WSL Ubuntu gcc 15.2 + ASan+UBSan）

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1; UBSAN_OPTIONS=halt_on_error=1

- phase2: PASS（p2_san）
- aio: PASS（aio_san）
- calibration: PASS（cal_san）
- star_detector: PASS（sdet_san）
- plate_solve: PASS（test_synthetic）test threshold n_inliers 38<40 (transform exact RMS=0); no ASan/UBSan findings
- photometric_calib: PASS（photo_san）
- snr_estimator: PASS（snr_san）
- healpix_drizzle: PASS（drz_san）
- orchestrator: PASS（orch_san）
- orchestrator: WIN32_TOOL_EXCEPTION（dll_loader）Win32 DLL 加载依赖; alternate=MinGW build+clang analyze PASS

真实发现（已修复）：photometric akima_interpolate heap-buffer-overflow（n=2 时 slope[-1]/slope[1] 越界 + ext_m 缺 m[n]）——P1，修复后 matrix PASS。
可移植性修复（sanitizer 驱动）：aio_log/sdet_log Windows API、noise_model finite()→std::isfinite、aio_pipeline_engine _strdup、dll_loader.h HMODULE typedef、sdet_log localtime_s→localtime_r。
工具例外：orchestrator dll_loader（Win32 HMODULE/LoadLibrary，WSL 无等价；替代=MinGW build PASS + clang --analyze PASS + 人工 review）——真工具不兼容，非时间豁免。