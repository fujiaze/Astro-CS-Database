# 当前唯一工作

## Task ID

`P00-005` — 采集工具链与本机环境

## 目标

采集本机工具链实际版本、安装路径、许可证与关键二进制哈希，生成 environment baseline，为 P01 构建可重现性提供环境基线。

采集范围（依据 P00 任务定义与 P00-004 依赖图识别的外部库）：
- PowerShell / Python
- GCC / MinGW-w64 / Make
- Qt6（Core/Gui/Widgets/OpenGLWidgets）
- GSL（gsl_multifit_nlinear）
- zstd / lz4 / zlib
- OpenMP（随编译器）
- Eigen3（healpix_stack build.ps1 引用）
- Git / gh CLI

## 入口条件

- P00-001 DONE ✓（基线预检完成，已知无根级构建入口）

## 允许修改

- `engineering/evidence/P00-005/**`
- `engineering/control/**`
- `engineering/tools/`（如需新增采集脚本）

## 禁止修改

- `lib/**`
- `docs/**`
- 构建脚本与算法配置

## 执行计划

1. 编写采集脚本（或直接采集），记录各工具版本号、安装路径、许可证
2. 对关键二进制（gcc.exe、python.exe、qmake6.exe、gsl 库等）计算 SHA-256
3. 汇总为 environment_baseline.json（机器可读）和 environment_baseline.md（人类可读）
4. 生成报告、复核、提交

## 完成标准

- 工具链版本、路径、许可证齐全
- 关键二进制哈希已记录
- 可在另一台同类机器上对照采集
