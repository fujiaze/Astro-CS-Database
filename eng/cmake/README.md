# cmake

本目录是 CMake 模块与平台构建片段的集合，由仓库根的 CMake 入口引用。

## 职责边界

- 放：源清单与安装布局等 CMake 模块、平台工具链片段、Windows 兼容垫片、产品清单模板与目录迁移清单文档。
- 不放：CMake 入口本体（根 CMakeLists.txt、CMakePresets.json）、构建脚本（eng/build/）。

## 内容

- cfitsio_platform.cmake —— CFITSIO 平台探测模块。
- cfitsio_sources.cmake —— CFITSIO 源文件清单模块。
- install_layout.cmake —— 安装目录布局模块。
- acsd.product.windows.json.in —— Windows 产品清单模板。
- win32_pthread_shim/ —— Windows pthread 兼容垫片。
- ARCH-001-migration-manifest.md —— 目录迁移清单文档，登记路径对应关系、状态与同步义务。

## 上游

上游：docs/ACSD_DESIGN.md §8.4（顶层结构·eng/cmake）。