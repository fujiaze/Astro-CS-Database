// healpix_browser_core.h - HEALPix 浏览器 core 层统一头文件 (healpix_browser_qt)
// 功能: 统一引入 core/ 层所有公开头文件，供 widgets/ 和 app/ 层使用
// 用途: #include "healpix_browser_core.h" 即可获得 BrowserBackend + STFEngine +
// HealpixMath + GLRenderer 全部接口
// 依赖: 无 Qt（core/ 层纯 C++17 + OpenGL 3.3 Core）
// 编译: g++ -Icore -Iinclude -I../../astro_image_io/include

#ifndef HEALPIX_BROWSER_CORE_H
#define HEALPIX_BROWSER_CORE_H

#include "browser_backend.h"
#include "stf_engine.h"
#include "healpix_math.h"
#include "gl_renderer.h"

#endif
