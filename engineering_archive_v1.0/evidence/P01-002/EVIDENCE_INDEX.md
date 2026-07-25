# EVIDENCE_INDEX: P01-002

## 证据目录
`engineering/evidence/P01-002/`

## 证据清单

| 文件 | 说明 |
|---|---|
| module_build_configs.json | 13 模块构建配置（子 Agent 采集） |
| module_build_configs.md | 人类可读模块配置摘要 |
| generate_lock.py | 锁定清单生成脚本 |
| dependencies.lock.json | 机器可读依赖锁定清单 |
| dependencies.lock.md | 人类可读锁定清单报告 |
| TASK_REPORT.md | 任务执行报告 |
| TEST_REPORT.md | 可重复性测试报告 |
| EVIDENCE_INDEX.md | 本文件 |
| REVIEW_REPORT.md | 独立复核报告 |

## 关键事实证据

### F-001: 16 工具链锁定
- 证据: dependencies.lock.json `toolchain` 数组长度 16
- 含版本、路径、许可证、SHA-256

### F-002: 8 外部库锁定
- 证据: dependencies.lock.json `external_libs` 数组长度 8
- GSL/GSL CBLAS/zstd/lz4/zlib/OpenMP/Eigen3/Qt6
- 每库标注 used_by 模块

### F-003: 13 模块构建配置锁定
- 证据: dependencies.lock.json `modules` 数组长度 13
- 每模块含 authority/sources/cflags/ldlibs/output/includes/defines

### F-004: 3 层构建顺序
- 基础层 6: astro_image_io, calibration, dynamic_psf, gaia_xpsd_client, star_detector, snr_estimator
- 中间层 4: healpix_drizzle, healpix_stack, photometric_calib, healpix_browser_qt
- 顶层 2: orchestrator, plate_solve_ipv
- 无构建 1: data_pipeline

### F-005: 3 个 Makefile 过时
- calibration: Makefile 输出名不同 + 源文件不一致
- healpix_stack: 引用已归档 healpix_io + 缺 gradient 源
- astro_image_io: 缺 healpix 源 + 缺条件定义

### F-006: 锁定完整性
- 3 个来源文件 SHA-256 记录在 lock_integrity
- environment_baseline / module_build_configs / dependency_graph 均可追溯
