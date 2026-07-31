# 仓库就地审计报告

- 审计日期: 2026-07-31
- 审计 Agent: TRAE AI Agent (GLM-5.2)
- 任务包: AstroCS_Stage1_HISS_Agent_Package_2026-07-31
- 审计性质: Phase 0 就地审计，不新建仓库

## 1. 仓库基准

| 项 | 值 |
|---|---|
| 仓库根 | `f:\Astro dev\Astro CS Normalization Database` |
| 远端 | https://github.com/fujiaze/Astro-CS-Database.git |
| 默认分支 | main |
| Wiki 启用 | true |
| 审计基准 commit | 183558ad6907d1e13a56a01c33c708913d7bbdc3 |
| 基准 commit 信息 | feat: 按Wiki规范修改K计算和CLI事件名 (2026-07-30 21:48:40 +0800) |
| 工作区状态 | 干净（已 push 到 origin/main） |

## 2. 现有 Stage1 / HISS 入口

### 2.1 CLI 入口

- 主可执行: `lib/orchestrator/cpp/src/main.cpp`
- CLI 实现: `lib/orchestrator/cpp/src/cli_command.cpp`
- 支持子命令: `run`, `run-batch`, `stage1`, `stage2`, `inspect`, `capabilities`
- stage1 入口: `CliCommand::cmd_stage1`（已实现，调用 `Orchestrator::run_stage1`）
- stage2 入口: `CliCommand::cmd_stage2`（已实现，本任务不得修改或触发）
- 事件流: 已输出 `job_started`/`stage_started`/`stage_progress`/`stage_completed`/`warning`/`error`/`job_completed` JSONL 事件

### 2.2 HISS 读写入口

- C++ HISS 读写器: `lib/astro_image_io/src/ahpx/`（含 `aio_ahpx_writer.cpp`, `aio_ahpx_reader.cpp`, `aio_ahpx_api.cpp`，DEPRECATED.md 已标注旧 .ahpx 格式）
- C++ Healpix I/O: `lib/astro_image_io/src/healpix/aio_healpix_io.cpp`
- Python HISS v2 参考实现: `lib/astro_image_io/python/hiss_v2.py`（基于已冻结 HISS_FORMAT_V2.md，含 footer/checksum/分块索引）
- Python Inspector/Visualizer: `lib/astro_image_io/python/hiss_v2_inspector.py`, `hiss_v2_visualizer.py`

### 2.3 Drizzle 入口

- C++ Drizzle 引擎: `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` + `drizzle_engine.h`
- 关键模块: `wcs_sip.cpp`（WCS/SIP 变换）、`poly_clip.cpp`（多边形裁剪）、`fits_reader.cpp`
- 已实现: `DrizzleEngine::drizzle()` 和 `DrizzleEngine::writeHis()`
- 默认配置: `nside=32768`, `nested=true`, `pixfrac=1.0`（任务包要求保留标准 pixfrac 语义）

### 2.4 浏览器入口

- Qt6+OpenGL 浏览器: `lib/healpix_db/healpix_browser_qt/`
- 入口: `app/main.cpp`
- core 层（无 Qt 依赖）: `core/`
- widgets 层: `app/stf_panel.h`（SingleFrameView 已归档至 widgets/archive/）
- 编译: `cmake --build build`（依赖 Qt6 + astro_image_io.dll）

### 2.5 Wiki 状态

- GitHub Wiki 已启用（远端），本地工作目录 `_wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/wiki/` 存在最新冻结 Wiki 内容
- 本地 Wiki 文件: 8 个页面（Home、_Sidebar、HISS-格式规范、Stage1-CLI接口、Stage1-Drizzle规范、Stage1-范围与架构、Stage1-校准规范、Stage1-验收与性能分析、Stage1-浏览器检查、Stage1-开发治理、Stage1-待确认事项、Wiki-本地推送说明）

## 3. 旧 Python 原型与冲突实现

### 3.1 HISS v2 Python 参考实现（保留，迁移参考）

- `lib/astro_image_io/python/hiss_v2.py` — Python 参考实现，基于已冻结 HISS_FORMAT_V2.md
- **状态**: 保留为参考实现，不作为正式 C++ 路径
- **风险**: 该实现使用 `footer 48B + MAGIC_TRAILER` 结构，而 02_FROZEN_STAGE1_HISS_SPEC.md §14 明确禁止 Footer/Checkpoint
- **处理**: 在 Wiki 标注"仅历史参考，不是规范"，正式实现以 C++ 为准

### 3.2 旧 .ahpx 格式（已归档）

- `lib/astro_image_io/src/ahpx/DEPRECATED.md` 已标注
- `aio_ahpx_api.cpp`, `aio_ahpx_reader.cpp`, `aio_ahpx_writer.cpp` — 旧 .ahpx 格式实现
- **处理**: 保持归档状态，不删除（被 healpix_drizzle 的 `writeHis()` 调用），新 HISS 实现使用独立路径

### 3.3 Python 校准/检测原型

- `lib/calibration/python/calibrator.py` — 已按 Wiki 规范修改（K = t_light/t_dark，硬错误回退）
- `lib/calibration/python/optimize_dark_scale` 函数仍保留在文件中但不再被调用
- `lib/plate_solve/python/` — 多个调试/可视化脚本，保留为辅助工具
- `lib/healpix_db/healpix_drizzle/healpix_drizzle.py` — Python Drizzle 包装层

### 3.4 已归档的旧浏览器

- `lib/healpix_db/archive/legacy/healpix_browser_python/` — 旧 Python+vispy 浏览器（已归档）
- `lib/healpix_db/archive/legacy/healpix_lod/` — 旧 LOD 实现（已归档）
- `lib/healpix_db/healpix_io/archive/` — 旧 healpix_io（已归档，API 并入 astro_image_io）

## 4. 冲突点与处理计划

### 4.1 冲突 Wiki 内容

`_wiki_freeze/.../wiki/` 下 2 个文件命中冲突关键词:
- `HISS-格式规范.md` — 含 v2/footer 相关描述
- `Stage1-范围与架构.md` — 含固定 4096 或 surface brightness 相关描述

**处理**: Phase 1 按任务包 03_WIKI_UPDATE_REQUIREMENTS.md 重写或加 SUPERSEDED 标识。

### 4.2 Drizzle 默认 nside

- 现有 `drizzle_engine.h` 默认 `nside=32768`，`pixfrac=1.0`
- 任务包要求: 自动 NSIDE 计算 + 标准 pixfrac drop 语义
- **处理**: 保留 C++ 实现作为基础，新增自动 NSIDE 计算函数；pixfrac 语义按 02_FROZEN §9 校验

### 4.3 Stage2 自动触发路径

- `cli_command.cpp` 第 508-562 行支持 `stage2` 子命令
- `cli_command.cpp` 第 778-815 行实现 `cmd_stage2`
- **处理**: 不修改 stage2 代码，但确保 stage1 路径不自动调用 stage2；CLI capabilities 中保留 stage2 声明（用户显式调用允许）

### 4.4 710 回归触发路径

- `cli_command.cpp` 第 390-426 行支持 `run-batch` 子命令
- 无自动启动 710 帧的代码路径，仅用户显式 `orchestrator run-batch <dir>` 触发
- **处理**: 保持现状，不在本任务中触发

## 5. 计划保留/删除/替换清单

### 5.1 保留（不修改）

- `lib/orchestrator/` — CLI 和编排器主体
- `lib/calibration/src/calibrator.cpp` — 已按 Wiki 修改
- `lib/plate_solve/cpp/ipv/` — PlateSolve C++ 实现
- `lib/star_detector/` — 星点检测
- `lib/gaia_xpsd_client/` — Gaia 客户端
- `lib/healpix_db/healpix_browser_qt/` — Qt 浏览器
- `lib/healpix_db/healpix_drizzle/` — Drizzle 引擎（基础保留，按规范增量修改）
- `lib/astro_image_io/` — 图像 I/O 与 HISS v2 Python 参考
- `lib/healpix_db/archive/` — 已归档代码

### 5.2 替换（重写或更新）

- `_wiki_freeze/AstroCS_Stage1_Wiki_Freeze_2026-07-30/wiki/` — 按 03_WIKI_UPDATE_REQUIREMENTS.md 重写为 8 个标准页面
- `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` — 按规范增量修改（自动 NSIDE、标准 pixfrac、float64 几何、support 计算）
- `lib/astro_image_io/src/ahpx/` — 新增 C++ HISS Writer/Reader 路径（不破坏旧 .ahpx 兼容）

### 5.3 删除（无价值或重复）

- 暂不删除任何文件。Python 原型保留为迁移参考，已归档代码保持归档状态。
- DELETE_LIST.txt 将在 Phase 6 最终交付时列出确需删除的文件（预计为空或极少）。

## 6. 未决项（不阻塞继续工作）

以下事项不阻塞 Phase 1-3 的已冻结内容实现，将写入 DECISION_QUEUE.md 等待 Phase 4 C++ 实验后汇报:

- DQ-001: signal 默认 codec/transform
- DQ-002: support 默认 codec
- DQ-003: BITMAP 默认 codec
- DQ-004: SPARSE_LIST 编码与 codec
- DQ-005: FULL/BITMAP/SPARSE 切换阈值
- DQ-006: checksum 算法
- DQ-007: 子块对齐

## 7. 审计结论

仓库处于可执行状态，无阻断性问题。现有 C++ 实现基础扎实，Drizzle/校准/PlateSolve/Gaia/Browser 均有可用实现。主要工作集中在:

1. 按任务包 02_FROZEN_STAGE1_HISS_SPEC.md 重写 Wiki，消除旧冲突
2. 按规范增量修改 Drizzle 引擎（自动 NSIDE、标准 pixfrac、support、float64 几何）
3. 实现 C++ HISS Writer/Reader（XISF 式 Header+attachments，无 Footer）
4. C++ 实验 codec/阈值/checksum/对齐
5. 正确性测试与性能剖析
6. 精简交付

继续 Phase 1。
