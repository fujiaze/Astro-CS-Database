# EVIDENCE_INDEX: P00-004

## 证据目录
`engineering/evidence/P00-004/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| deps_group1.json | c68f7baa2b6621f20efa761f14624e37713498ae4051584cb89b2a29bd7c288f | 子 Agent 分析第一组（7 模块）依赖数据 |
| deps_group2.json | faca99c31c982cc01451815e51b663134a474b2c38a3a0f3bf1dc284beddab0c | 子 Agent 分析第二组（6 模块）依赖数据 |
| merge_deps.py | 0e934fda245d0be1574695309b7ef73c6110decff57c3f4aa72b3eda68299f03 | 合并脚本，生成统一依赖图 |
| dependency_graph.json | 6f2ce8f21063e971bcc77c40c335a4b3b3d9111febb4b3d8270f7751e22904b | 机器可读依赖图（13 模块 68 边 10 问题） |
| dependency_graph.md | aa149d5c64c7b15c06415a1bdef3cc626b000f226ed5aa4e25ed1ab0c7f876ef | 人类可读依赖图（模块清单+调用方向+分层架构+潜在问题） |
| TASK_REPORT.md | — | 任务执行报告 |
| TEST_REPORT.md | — | 可重复性测试报告 |
| EVIDENCE_INDEX.md | — | 本文件 |
| REVIEW_REPORT.md | — | 独立复核报告 |

## 关键事实证据

### F-001: 13 个模块全部覆盖
- 证据: dependency_graph.json `module_count` = 13
- 证据: dependency_graph.md 模块清单表 13 行
- 对照: P00-001 preflight 11 个主模块 + P00-002/003 新增 healpix_drizzle/healpix_stack + healpix_db 拆分 3 子模块 = 13

### F-002: 基础层 6 模块编译期独立
- 证据: dependency_graph.json 中 astro_image_io/calibration/dynamic_psf/gaia_xpsd_client/star_detector/snr_estimator 的 `includes_other_modules` 字段均为空数组
- 证据: dependency_graph.md "基础层（无跨模块依赖）" 列出 6 模块

### F-003: 中间层 4 模块依赖基础层头文件
- 证据: healpix_drizzle `includes_other_modules` 含 astro_image_io: aio_healpix_io.h / astro_image_io.h / aio_pipeline.h
- 证据: healpix_stack `includes_other_modules` 含 astro_image_io: 3 个头文件 + gaia_xpsd_client: gaia_client.h
- 证据: photometric_calib `includes_other_modules` 含 gaia_xpsd_client: gaia_client.h
- 证据: healpix_browser_qt `includes_other_modules` 含 astro_image_io: aio_healpix_io.h

### F-004: 顶层 2 模块运行时动态加载
- 证据: orchestrator `note` 字段: "各 DLL 模块通过 DllLoader 在运行时动态加载 (LoadLibraryExA)"
- 证据: plate_solve `includes_other_modules` 3 条均为"运行时句柄注入"或"运行时动态加载"

### F-005: 10 个潜在问题已登记
- 证据: dependency_graph.json `potential_issues` 数组长度 10
- 证据: dependency_graph.md "潜在问题" 列出 10 项
- 涵盖: Makefile 过时引用、构建系统分歧、源码重复、类型漂移风险、静态编译耦合

### F-006: 头文件依赖可追踪
- 证据: include 类型边均带具体头文件路径与源文件行号（如 "drizzle_engine.h:7, drizzle_engine.cpp:3, hp_drizzle_api.cpp:13"）
- 抽查 4 条均可在源码中定位（见 TEST_REPORT 测试 5）

## 命令日志
- `python merge_deps.py` — 退出码 0，产物 SHA-256 见上表
