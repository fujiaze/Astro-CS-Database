# AstroCS 模块依赖图

- 生成时间: 2026-07-24
- 模块数: 13
- 依赖边数: 68
- 潜在问题数: 10

## 模块清单

| 模块 | DLL 产出 | 外部库 | Python 绑定 | 数据依赖 |
|---|---|---|---|---|
| astro_image_io | astro_image_io.dll | -lm, -lzstd (按 build_config 启用), -llz4 (按 build_config 启用) | ['python/astro_image_io.py (ctypes.CDLL 加载 astro_image_io.dll)', 'python/aio_healpix_io.py', 'python/orchestrator.py'] | — |
| calibration | astro_calibration.dll (build.ps1 主构建) / cosmetic_corrector.dll (Makefile, 仅 cosmetic_corrector.cpp) | -lm, -fopenmp | ['python/astro_calibration.py (ctypes.CDLL 加载 astro_calibration.dll)', 'python/cosmetic_corrector.py (ctypes.CDLL 加载 cosmetic_corrector.dll)'] | testdata/calibration (校准帧数据, 见 orchestrator stage1_config.json) |
| data_pipeline | — | — | — | — |
| dynamic_psf | dynamic_psf.dll | -lm, -fopenmp | ['python/dynamic_psf.py (ctypes.CDLL 加载 dynamic_psf.dll)'] | — |
| gaia_xpsd_client | gaia_client.dll (make dll) / libgaia_client.a (静态库) | -lz, -fopenmp, -lm | ['python/verify_spectrum.py (ctypes.CDLL 加载 gaia_client.dll)', 'python/vector_match_v2.py (ctypes.CDLL 加载 gaia_client.dll)', 'python/verify_global_coverage.py (ctypes.CDLL 加载 gaia_client.dll)', 'python/test_multi_db.py (ctypes.CDLL 加载 gaia_client.dll)', 'python/verify_dr3.py'] | GaiaDR3 (项目根目录, 18 亿星, 16 个 .xpsd 文件, db_type=1), GaiaDR3SP (项目根目录, 2.2 亿星含光谱, 20 个 .xpsd 文件, db_type=2) |
| healpix_db | — | — | — | — |
| orchestrator | — | -lm, -static | ['python/orchestrator.py (不直接 ctypes 加载 DLL; 使用 ctypes.create_string_buffer)', 'python/pipeline_adapters/ (calibrate_adapter.py, drizzle_adapter.py, photometric_adapter.py, platesolve_adapter.py, psf_adapter.py, snr_adapter.py)'] | GaiaDR3SP (configs/stage1_config.json: gaia_data_dir, configs/galaxy_center_t4.json), testdata/calibration (configs/stage1_config.json: calibration_dir) |
| photometric_calib | photometric_calib.dll | -lm, -static (Makefile: 整体静态链接运行时), -fopenmp, gaia_client.dll (链接为输入, Makefile 通过 GAIA_DLL 变量直接传入; build.ps1 复制到本模块目录保证运行时加载) | ['python/photometric_calib.py (ctypes.CDLL 加载 ../cpp/photometric_calib.dll; 调用 pc_calibrate_simple / pc_calibrate_simple_with_gaia)', 'python/gaia_spectrum_client.py (ctypes.CDLL 加载 ../../gaia_xpsd_client/gaia_client.dll; 封装 gaia_client_cone_search_with_spectrum, 为本模块提供 BP/RP 光谱数据)', 'python/flux_calibrator.py (PhotometricCalib 包装类, 调用 photometric_calib.dll)'] | data/response_curves/filters.json (滤镜透过率曲线), data/response_curves/qe_curves.json (量子效率曲线), GaiaDR3SP (经 gaia_client.dll 提供 BP/RP 光谱, 336-1020nm 343 采样点) |
| plate_solve | ipv_solver.dll | -lkernel32 (Makefile 链接选项), -fopenmp, build.ps1 额外标志: -ffast-math -funroll-loops -march=native -mstackrealign -D__USE_MINGW_ANSI_STDIO=1 | ['python/ipv_solver.py (ctypes.CDLL 加载 ../cpp/ipv/ipv_solver.dll; 封装 ipv_solve_create/ipv_set_gaia_handle/ipv_set_detector_handle/ipv_solve)', 'python/pipeline_adapter.py (ctypes 加载 gaia_client.dll 调用 gaia_client_cone_search_for_solver, 配合 ipv_solver.dll / star_detector.dll / astro_image_io.dll)', 'python/visualize_reproject.py (依赖 ipv_solver.dll + gaia_client.dll + star_detector.dll + astro_image_io.dll)', 'python/debug_visual.py (依赖同上)'] | GaiaDR3SP (经 gaia_client.dll 句柄注入, 提供锥形搜索星表), FITS/XISF 图像 (经 astro_image_io.dll 动态加载读取) |
| snr_estimator | snr_estimator.dll | -fopenmp, -static (Makefile 整体静态链接) | ['python/snr_estimator.py (ctypes.CDLL 加载 ../cpp/snr_estimator.dll; 封装 snr_estimate C 接口)'] | — |
| star_detector | star_detector.dll | -lgsl (gsl_multifit_nlinear trust-region LM 求解器), -lgslcblas (GSL BLAS), -lm, -fopenmp, -static-libgcc, -static-libstdc++ | ['python/star_detector.py (ctypes.CDLL 加载 ../star_detector.dll; 封装星点检测 + PSF 拟合 API)', 'python/analyze_fit_performance.py (ctypes 调用 star_detector.dll 测量拟合性能)', 'python/test_dynamic_background.py (ctypes 调用 star_detector.dll 动态背景测试)'] | — |
| healpix_db/healpix_drizzle | healpix_drizzle.dll | -lastro_image_io (链接 ../../astro_image_io/astro_image_io.dll), -lm, -static-libgcc, -static-libstdc++, -fopenmp | ['healpix_drizzle.py (ctypes.CDLL 加载 healpix_drizzle.dll; 运行时依赖 healpix_io.dll + astro_image_io.dll)'] | FITS 投影图像 (含 SIP 畸变多项式, 输入), .hiss 格式输出 (经 astro_image_io.dll hiss_write) |
| healpix_db/healpix_stack | healpix_stack.dll | — | ['healpix_stack.py (ctypes.CDLL 加载 healpix_stack.dll; 预加载 healpix_io.dll + astro_image_io.dll 到进程地址空间)'] | GaiaDR3SP (gradient/gradient_sampler.cpp:322 gaia_client_create_ex(gaia_data_dir, GAIA_DB_DR3SP); 用于梯度采样阶段的星表查询), .hiss / .hcsd 格式文件 (经 astro_image_io.dll 读写) |

## 依赖关系（调用方向）

```
调用方 → 被调用方 (类型)
---
astro_image_io → calibration (link)
astro_image_io → dynamic_psf (link)
astro_image_io → gaia_xpsd_client (link)
astro_image_io → photometric_calib (link)
astro_image_io → snr_estimator (link)
calibration → astro_image_io (link)
calibration → dynamic_psf (link)
calibration → gaia_xpsd_client (link)
calibration → photometric_calib (link)
calibration → snr_estimator (link)
dynamic_psf → astro_image_io (link)
dynamic_psf → calibration (link)
dynamic_psf → gaia_xpsd_client (link)
dynamic_psf → photometric_calib (link)
dynamic_psf → snr_estimator (link)
gaia_xpsd_client → astro_image_io (link)
gaia_xpsd_client → calibration (link)
gaia_xpsd_client → dynamic_psf (link)
gaia_xpsd_client → healpix_db/healpix_drizzle (link)
gaia_xpsd_client → photometric_calib (link)
gaia_xpsd_client → snr_estimator (link)
healpix_db/healpix_drizzle → astro_image_io (link)
healpix_db/healpix_drizzle → astro_image_io: include/aio_healpix_io.h (drizzle_engine.h:7, drizzle_engine.cpp:3, hp_drizzle_api.cpp:13; 提供 hiss_write / HioSnrModel / HioSnrControlPoint, 向后兼容宏) (include)
healpix_db/healpix_drizzle → astro_image_io: include/aio_pipeline.h (hp_drizzle_api.h:15; PipelineFrame 定义) (include)
healpix_db/healpix_drizzle → astro_image_io: include/astro_image_io.h (hp_drizzle_api.cpp:11; 提供 aio_frame_get_block / aio_frame_kv_get) (include)
healpix_db/healpix_drizzle → calibration (link)
healpix_db/healpix_drizzle → dynamic_psf (link)
healpix_db/healpix_drizzle → gaia_xpsd_client (link)
healpix_db/healpix_drizzle → healpix_db/healpix_stack: gradient/snr_evaluator.h (hp_drizzle_api.cpp:12; 静态编译 snr_evaluator.cpp) (include)
healpix_db/healpix_drizzle → healpix_db/healpix_stack: healpix_core.h (drizzle_engine.cpp:2; 静态编译 healpix_core.cpp) (include)
healpix_db/healpix_drizzle → photometric_calib (link)
healpix_db/healpix_drizzle → snr_estimator (link)
healpix_db/healpix_stack → Eigen3: C:\msys64\mingw64\include\eigen3 (build.ps1 -I 引用, 用于 spherical_spline 球面样条) (include)
healpix_db/healpix_stack → astro_image_io: include/aio_healpix_io.h (hp_stack_api.cpp:14, hp_stack_hiss.cpp:14, gradient/gradient_sampler.cpp:17; 提供 hiss_read / hiss_read_snr_model / hcsd_write / hio_free, 向后兼容宏) (include)
healpix_db/healpix_stack → astro_image_io: include/aio_pipeline.h (hp_stack_api.h:13; PipelineFrame 定义) (include)
healpix_db/healpix_stack → astro_image_io: include/astro_image_io.h (ahps_reader.cpp:2, ahps_writer.cpp:2; 提供 aio_compress / aio_decompress) (include)
healpix_db/healpix_stack → gaia_xpsd_client: src/gaia_client.h (gradient/gradient_sampler.cpp:18 #include "gaia_client.h"; 调用 gaia_client_create_ex / gaia_client_cone_search_for_solver / gaia_client_destroy; build.ps1 静态编译 gaia_client.c 进本 DLL) (include)
healpix_db/healpix_stack → healpix_db/healpix_io: include/healpix_io.h (Makefile -I$(HIO_DIR)/include 引用, 但该目录已归档仅 ARCHIVED.md, 实际由 astro_image_io 兼容宏提供) (include)
orchestrator → astro_image_io (link)
orchestrator → astro_image_io: aio_pipeline.h (cpp/include/orchestrator.h) (include)
orchestrator → astro_image_io: astro_image_io.h (cpp/src/orchestrator.cpp) (include)
orchestrator → calibration (link)
orchestrator → dynamic_psf (link)
orchestrator → dynamic_psf: dynamic_psf.h (cpp/src/orchestrator.cpp) (include)
orchestrator → gaia_xpsd_client (link)
orchestrator → gaia_xpsd_client: gaia_client.h (cpp/src/orchestrator.cpp) (include)
orchestrator → healpix_db/healpix_drizzle: hp_drizzle_api.h (cpp/src/orchestrator.cpp) (include)
orchestrator → healpix_db/healpix_stack: hp_stack_api.h (cpp/src/orchestrator.cpp) (include)
orchestrator → photometric_calib (link)
orchestrator → photometric_calib: photometric_calib.h (cpp/src/orchestrator.cpp, 不在本次 7 模块范围) (include)
orchestrator → plate_solve: ipv_api.h (cpp/src/orchestrator.cpp, 不在本次 7 模块范围) (include)
orchestrator → snr_estimator (link)
orchestrator → snr_estimator: snr_estimator.h (cpp/src/orchestrator.cpp, 不在本次 7 模块范围) (include)
photometric_calib → astro_image_io (link)
photometric_calib → calibration (link)
photometric_calib → dynamic_psf (link)
photometric_calib → gaia_xpsd_client (link)
photometric_calib → gaia_xpsd_client: src/gaia_client.h (cpp/src/pc_api.cpp:14 #include "gaia_client.h"; Makefile -I../../gaia_xpsd_client/src; build.ps1 -I$gaiaIncDir; 链接 ../../gaia_xpsd_client/gaia_client.dll) (include)
photometric_calib → snr_estimator (link)
plate_solve → astro_image_io: 运行时动态加载 (ipv_select.cpp:122 LoadLibraryA("astro_image_io.dll"), GetProcAddress 解析 aio_read/aio_get_pixel_data/aio_get_width/aio_get_height/aio_free_image_data; 无 #include, 无链接) (include)
plate_solve → gaia_xpsd_client: 运行时句柄注入 (ipv_entry.cpp 实现 ipv_set_gaia_handle, 通过 get_gaia_client_handle() 全局访问器供 ipv_select 使用; 无 #include gaia_client.h) (include)
plate_solve → star_detector: 运行时句柄注入 (ipv_entry.cpp 实现 ipv_set_detector_handle, 通过 get_star_detector_handle() 全局访问器; 无 #include star_detector.h) (include)
star_detector → astro_image_io (link)
star_detector → calibration (link)
star_detector → dynamic_psf (link)
star_detector → gaia_xpsd_client (link)
star_detector → photometric_calib (link)
star_detector → snr_estimator (link)
```

## 分层架构

```
基础层（无跨模块依赖）:
  astro_image_io    — FITS/XISF/.ahpx I/O + Pipeline 引擎
  calibration       — CCD 校准（OpenMP）
  dynamic_psf       — PSF 拟合（GSL, OpenMP）
  gaia_xpsd_client  — Gaia 星表客户端（mmap, OpenMP）
  star_detector     — 星点检测（GSL, OpenMP）
  snr_estimator     — SNR 估算（OpenMP）

中间层（依赖基础层）:
  healpix_drizzle   → astro_image_io (link+include)
  healpix_stack     → astro_image_io (link+include)
  photometric_calib → gaia_xpsd_client (link+include)
  healpix_browser_qt → astro_image_io (link+include, Qt6/OpenGL)

顶层（运行时动态加载）:
  orchestrator      → 运行时 LoadLibrary 加载所有 DLL
  plate_solve       → 运行时 LoadLibrary 加载 astro_image_io/gaia_client/star_detector
```

## 潜在问题

1. healpix_db/healpix_stack Makefile 仍引用 -lhealpix_io 与 ../healpix_io/include, 但 healpix_io 目录已归档 (仅 ARCHIVED.md), 实际功能已合并入 astro_image_io.dll
2. calibration 模块存在两套构建: Makefile 仅产出 cosmetic_corrector.dll, build.ps1 产出 astro_calibration.dll (主构建), 二者源文件范围不一致
3. data_pipeline 模块无独立构建文件, 其源文件与 astro_image_io/src 下同名文件重复, 存在维护一致性问题
4. orchestrator Makefile -I 包含 star_detector/include, 但 orchestrator.cpp 未 #include star_detector.h (通过 DllLoader 运行时加载)
5. healpix_db/healpix_stack Makefile 与 build.ps1 源文件列表严重分歧: Makefile 仅编译 7 个根目录 .cpp, 缺少 gradient/ 子目录 5 个 .cpp 与 gaia_client.c; 使用 make 构建会导致 gradient 模块符号缺失
6. healpix_db/healpix_stack Makefile 仍引用 -lhealpix_io 与 ../healpix_io/include, 但 healpix_io 已合并入 astro_image_io (2026-07-16 spec G1), 目录仅剩 ARCHIVED.md, make 构建会失败
7. plate_solve 运行时依赖 astro_image_io.dll / gaia_client.dll / star_detector.dll, 但编译期无任何 -I 或 -l 声明, 构建独立但运行需保证 DLL 在 PATH 或同目录
8. plate_solve ipv_select.cpp 自声明 AIOImageData* 类型 (typedef) 而非 #include astro_image_io.h, 存在结构体定义漂移风险
9. photometric_calib 运行时需 gaia_client.dll 与 photometric_calib.dll 同目录, build.ps1 自动复制但 Makefile 依赖手动复制 (copy /Y)
10. healpix_db/healpix_drizzle 静态编译 healpix_stack 的 healpix_core.cpp + snr_evaluator.cpp, 若 healpix_stack 升级需同步重新编译 drizzle
