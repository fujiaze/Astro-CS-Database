# Module: healpix_browser_qt

## 职责

HiPS/HEALPix 球面浏览器（Qt6 + OpenGL，STF 显示，可选构建）。

## 非职责

不参与科学处理链。

## Public API

healpix_browser_core.h；Qt widgets 层。

## Data contract

HiPS tiles 读（astro_image_io）；STF 参数。

## Ownership

Qt parent-child；renderer 只读共享。

## Thread safety

Qt 主线程 + 后台 tile I/O；GL 单线程。

## Errors

IO 失败 → 状态显示。

## Science IDs

无（展示层）；依赖 DATA-HIPS-*。

## 性能特征

tile cache + async I/O；STF 0.5%/99.5% 分位。

## Tests

STF engine 单测；视觉验收（工程控制 16 spec）。

## Source files

lib/healpix_db/healpix_browser_qt/。
