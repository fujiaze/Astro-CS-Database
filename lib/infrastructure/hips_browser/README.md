# hips_browser

本目录是 HiPS 浏览 GUI 组件的落位目录，组件不进产品清单。

## 职责边界

- 放：Qt 浏览组件实现（healpix_browser_qt/）与本目录的迁移登记文件。
- 不放：生产可执行路径上的代码（产品清单不含本组件）、科学算法（lib/algorithms/）、I/O 边界（lib/infrastructure/aio/）。

## 内容

- healpix_browser_qt/ —— Qt 浏览器组件：app/、core/、widgets/、include/、tests/、tools/，以及构建与部署脚本（CMakeLists.txt、Makefile、deploy.ps1、run_healpix.bat）。
- PENDING.md —— 本目录内容与目标位置的迁移登记文件。

## 上游

上游：docs/ACSD_DESIGN.md §8.4（infrastructure/hips_browser：未来 GUI 组件，不进产品清单）。