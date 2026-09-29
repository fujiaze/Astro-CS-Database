# Module: healpix_browser_qt

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

## 职责

HiPS/HEALPix 球面浏览器（Qt6 + OpenGL，STF 显示，可选构建）。

## 非职责

不参与科学处理链。

## Public API

`lib/infrastructure/hips_browser/healpix_browser_qt/include/healpix_browser_core.h`（core 面）+ `core/{stf_engine.h,hips_sky_view.h,gl_renderer.h,browser_backend.h}` + Qt 层 `widgets/{abstract_view,hips_view,sphere_view}.{h,cpp}`、`app/{main_window,stf_bar,stf_panel}.{h,cpp}`；关键符号 `STFEngine::get_preset(name,data_min,data_max)`/`STFEngine::mtf(x,m)`。

## Data contract

HiPS tiles 读（astro_image_io 读面）；显示变换状态 `DisplayTransformState{black,white,midtones,curve,compression,generation}` 与预设模式 `STFMode`（参数结构 `STFParams{shadows,highlights,midtones,compression}`）。

## Ownership

Qt parent-child；renderer 只读共享。

## Thread safety

Qt 主线程 + 后台 tile I/O；GL 单线程。

## Errors

IO 失败 → 状态显示。

## Science IDs

无（展示层）；依赖 DATA-HIPS-*。

## 性能特征

tile cache + async I/O；STF 黑/白点取 0.5%/99.5% 分位（定义 = `STFEngine::get_preset` 由数据 min/max 与分位裁剪点构造显示变换，`normalize()` 归一到 [0,1]；MTF 曲线 `mtf(x,m)`；复杂度 O(n)，用 `std::nth_element` 选分位、不整排序）。

## Tests

STF engine 单测（`lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_stf_engine.cpp`）；视觉验收按 `ACCEPTANCE_SPEC.md` 的 L4 清单执行。

## Source files

lib/infrastructure/hips_browser/healpix_browser_qt/。
