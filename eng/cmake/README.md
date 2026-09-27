# cmake

本目录是 CMake 模块与平台构建片段的集合，由仓库根的 CMake 入口引用。

## 职责边界

- 放：源清单与安装布局等 CMake 模块、平台工具链片段、Windows 兼容垫片、产品清单模板、覆盖率报告脚本与目录迁移清单文档。
- 不放：CMake 入口本体（根 CMakeLists.txt、CMakePresets.json）、构建脚本（eng/build/）、机器门注册表（eng/ci/）。

## 内容

- cfitsio_sources.cmake —— CFITSIO 源文件清单模块。
- install_layout.cmake —— 安装目录布局模块。
- astrocs.product.windows.json.in —— Windows 产品清单模板。
- toolchain/ —— 平台工具链 CMake 片段。
- win32_pthread_shim/ —— Windows pthread 兼容垫片。
- qa_coverage_report.sh —— 覆盖率报告脚本。
- ARCH-001-migration-manifest.md —— 目录迁移清单文档，登记路径对应关系、状态与同步义务。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（顶层结构·eng/cmake）；ENGINEERING_SPEC.md §1（语言、编译器与平台）、§7（目录规范）。