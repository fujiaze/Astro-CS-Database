# TASK_REPORT: P01-002 建立依赖锁定清单

## 任务信息
- **Task ID**: P01-002
- **Phase**: P01
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 基线
- **commit**: bfc1bb1（P01-001 提交后 HEAD）
- **分支**: main

## 目标与范围
基于 P00-005 环境基线 + P01-002 模块构建配置 + P00-004 依赖图，生成 dependencies.lock.json，锁定工具链版本、外部库、模块构建配置与构建顺序。

## 入口条件
- P01-001 DONE ✓（ADR-004 构建策略决策完成）

## 实现过程
1. **模块配置采集**：通过子 Agent 读取 13 个活跃模块的 build.ps1/Makefile/CMakeLists.txt，提取源文件、编译标志、链接库、输出名、include 路径、预处理宏
2. **依赖锁定**：编写 generate_lock.py 脚本，整合 environment_baseline.json（16 工具链）+ module_build_configs.json（13 模块）+ dependency_graph.json（68 依赖边）
3. **构建顺序生成**：按依赖图分层（基础层 6 模块 → 中间层 4 模块 → 顶层 2 模块 + 无构建 1 模块）
4. **完整性校验**：记录来源文件 SHA-256，确保锁定清单可追溯

## 修改文件
- `engineering/evidence/P01-002/module_build_configs.json`（子 Agent 采集的 13 模块构建配置）
- `engineering/evidence/P01-002/module_build_configs.md`（人类可读摘要）
- `engineering/evidence/P01-002/generate_lock.py`（锁定清单生成脚本）
- `engineering/evidence/P01-002/dependencies.lock.json`（机器可读依赖锁定清单）
- `engineering/evidence/P01-002/dependencies.lock.md`（人类可读锁定清单报告）
- `engineering/evidence/P01-002/TASK_REPORT.md`（本文件）
- `engineering/evidence/P01-002/TEST_REPORT.md`
- `engineering/evidence/P01-002/EVIDENCE_INDEX.md`
- `engineering/evidence/P01-002/REVIEW_REPORT.md`
- `engineering/control/MASTER_TASK_REGISTER.csv`（P01-002 → DONE）
- `engineering/control/PROJECT_STATE.yaml`（current_task → P01-003）
- `engineering/control/CURRENT_WORK.md`（切换为 P01-003）

## 结果

### 锁定内容统计
| 项目 | 数量 |
|---|---|
| 工具链 | 16（PowerShell/Python/Git/GitHub CLI/GCC/G++/mingw32-make/Make/Qt6/GSL/GSL CBLAS/zstd/lz4/zlib/OpenMP/Eigen3） |
| 外部库 | 8（GSL/GSL CBLAS/zstd/lz4/zlib/OpenMP/Eigen3/Qt6） |
| 模块 | 13（11 DLL + 1 EXE + 1 静态库 + 1 无构建） |
| 构建顺序 | 3 层（基础层 6 + 中间层 4 + 顶层 2 + 无构建 1） |

### 模块构建系统分布
| 构建系统 | 模块数 | 模块 |
|---|---|---|
| build.ps1（权威） | 6 | astro_image_io, calibration, healpix_stack, snr_estimator, photometric_calib, plate_solve_ipv |
| Makefile（权威） | 5 | dynamic_psf, gaia_xpsd_client, star_detector, healpix_drizzle, orchestrator |
| CMake（权威） | 1 | healpix_browser_qt |
| 无构建 | 1 | data_pipeline |

### 关键发现
1. **3 个 Makefile 过时**：calibration（源文件不一致）、healpix_stack（引用已归档 healpix_io）、astro_image_io（缺 healpix 源）
2. **healpix_browser_qt 用 CMake + Qt6**（非 qmake），需在根级 build.ps1 中特殊处理
3. **data_pipeline 无构建文件**，源文件编译进 astro_image_io（待 ADR-002 决策）
4. **外部库依赖**：GSL→star_detector，zlib→gaia_xpsd_client/healpix_stack，zstd/lz4→astro_image_io，Eigen3→healpix_stack，Qt6→healpix_browser_qt，OpenMP→9 模块

### 构建顺序
- **基础层（无跨模块依赖）**: astro_image_io, calibration, dynamic_psf, gaia_xpsd_client, star_detector, snr_estimator
- **中间层（依赖基础层）**: healpix_drizzle, healpix_stack, photometric_calib, healpix_browser_qt
- **顶层（运行时动态加载）**: orchestrator, plate_solve_ipv
- **无构建**: data_pipeline

## 未解决问题
- 3 个过时 Makefile 需在 P01-005 clean build 时规避（优先 build.ps1）
- data_pipeline 归属待 ADR-002 决策

## 风险与回退
- 本任务为只读采集 + 锁定清单生成，无代码变更
- 回退方式：删除 dependencies.lock.json 并 revert 控制文件

## 建议状态
`IN_REVIEW`
