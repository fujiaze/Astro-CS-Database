# 项目根目录整理 checklist

> 对应 spec: docs/superpowers/specs/2026-07-16-project-reorganization.md
> 任务类型: 工程重构

---

## Phase 0: 准备与回滚点

> 根目录非 git 仓库，各模块独立 git 仓库。回滚点 = 各模块当前 commit hash。

- [ ] 0.1 记录 plate_solve / dynamic_psf / photometric_calib / healpix_db 四个模块仓库当前 commit hash
- [ ] 0.2 确认四个模块仓库无未提交的重要改动

## Phase 1: 调试文件分类归档

### 1.1 创建目录结构
- [ ] 1.1.1 创建 `lib/plate_solve/archive/debug_2026-07/`
- [ ] 1.1.2 创建 `lib/dynamic_psf/archive/debug_2026-07/`
- [ ] 1.1.3 创建 `lib/photometric_calib/archive/debug_2026-07/`
- [ ] 1.1.4 创建 `lib/photometric_calib/tools/`
- [ ] 1.1.5 创建 `lib/plate_solve/tools/`

### 1.2 高价值脚本移入模块 tools/
- [ ] 1.2.1 `git mv diag_gradient_report.py lib/photometric_calib/tools/`
- [ ] 1.2.2 `git mv diag_projection_plot.py lib/plate_solve/tools/`
- [ ] 1.2.3 `git mv run_healpix.bat lib/healpix_db/healpix_browser_qt/`

### 1.3 一次性脚本分散归档至对应模块
- [ ] 1.3.1 `git mv diag_wcs.py lib/plate_solve/archive/debug_2026-07/`
- [ ] 1.3.2 `git mv diag_wcs_offset.py lib/plate_solve/archive/debug_2026-07/`
- [ ] 1.3.3 `git mv diag_psf_edge.py lib/dynamic_psf/archive/debug_2026-07/`
- [ ] 1.3.4 `git mv diag_psf_root_cause.py lib/dynamic_psf/archive/debug_2026-07/`
- [ ] 1.3.5 `git mv diag_image_stats.py lib/dynamic_psf/archive/debug_2026-07/`
- [ ] 1.3.6 `git mv diag_distribution.py lib/photometric_calib/archive/debug_2026-07/`
- [ ] 1.3.7 `git mv diag_light.py lib/photometric_calib/archive/debug_2026-07/`

### 1.4 输出文件删除
- [ ] 1.4.1 `git rm diag_light_output.txt`
- [ ] 1.4.2 `git rm diag_output.txt`
- [ ] 1.4.3 `git rm diag_plot_log.txt`
- [ ] 1.4.4 `git rm diag_resolve_log.txt`
- [ ] 1.4.5 `git rm diag_wcs_output.txt`
- [ ] 1.4.6 `git rm diag_gradient_report.png`
- [ ] 1.4.7 `git rm diag_projection.png`
- [ ] 1.4.8 `git rm diag_projection_after.png`
- [ ] 1.4.9 `git rm diag_projection_before.png`
- [ ] 1.4.10 `git rm diag_projection_current.png`
- [ ] 1.4.11 `git rm healpix_after_fix.png`
- [ ] 1.4.12 `git rm healpix_window.png`
- [ ] 1.4.13 `git rm qt_run_log.txt`
- [ ] 1.4.14 `git rm 03_fsyn.json`

### 1.5 归档目录 README
- [ ] 1.5.1 在 `lib/plate_solve/archive/debug_2026-07/` 创建 README.md
- [ ] 1.5.2 在 `lib/dynamic_psf/archive/debug_2026-07/` 创建 README.md
- [ ] 1.5.3 在 `lib/photometric_calib/archive/debug_2026-07/` 创建 README.md

## Phase 2: 文档归档与新建

### 2.1 当前文档归档
- [ ] 2.1.1 创建 `docs/archive/`
- [ ] 2.1.2 `git mv PROJECT_ARCHITECTURE.md docs/archive/PROJECT_ARCHITECTURE_2026-07-16.md`
- [ ] 2.1.3 `git mv UI_ARCHITECTURE.md docs/archive/UI_ARCHITECTURE_2026-07-16.md`

### 2.2 新增架构文档
- [ ] 2.2.1 创建 `docs/ARCHITECTURE.md`，归纳当前真实架构
  - [ ] 项目概述与核心设计原则
  - [ ] 模块清单（lib/ 实际模块 + 状态标注）
  - [ ] 两段流水线 10 节点架构
  - [ ] 数据流与格式体系（.hiss / .hcsd）
  - [ ] 模块依赖关系
  - [ ] 性能基线

### 2.3 新增割裂记录文档
- [ ] 2.3.1 创建 `docs/DESIGN_IMPL_GAP.md`
  - [ ] 架构文档 vs 实际代码偏差
  - [ ] 模块状态标注 vs 仓库拆分状态
  - [ ] spec 设计 vs 实施完成差距
  - [ ] 待修复断层清单

### 2.4 memory.md 更新
- [ ] 2.4.1 在 memory.md 追加本次整理记录
- [ ] 2.4.2 修正 memory.md 中与实际不符的条目

## Phase 3: 验证

### 3.1 目录结构核对
- [ ] 3.1.1 根目录无 diag_*/healpix_*.png/qt_run_log.txt/03_fsyn.json
- [ ] 3.1.2 `archive/debug_2026-07/` 含 7 个归档脚本 + README.md
- [ ] 3.1.3 `lib/photometric_calib/tools/diag_gradient_report.py` 存在
- [ ] 3.1.4 `lib/plate_solve/tools/diag_projection_plot.py` 存在
- [ ] 3.1.5 `lib/healpix_db/healpix_browser_qt/run_healpix.bat` 存在
- [ ] 3.1.6 `docs/ARCHITECTURE.md` 存在且内容完整
- [ ] 3.1.7 `docs/DESIGN_IMPL_GAP.md` 存在且内容完整
- [ ] 3.1.8 `docs/archive/` 含 2 个归档文档

### 3.2 git 提交历史清洁
- [ ] 3.2.1 `git status` 无未跟踪文件（除 .gitignore 忽略项）
- [ ] 3.2.2 所有移动/删除通过 git mv/git rm 保留历史
- [ ] 3.2.3 commit message 遵循项目规范

## Phase 4: 提交（各模块仓库独立提交）

- [ ] 4.1 plate_solve 仓库：git add tools/diag_projection_plot.py + archive/debug_2026-07/，commit
- [ ] 4.2 dynamic_psf 仓库：git add archive/debug_2026-07/，commit
- [ ] 4.3 photometric_calib 仓库：git add tools/diag_gradient_report.py + archive/debug_2026-07/，commit
- [ ] 4.4 healpix_db 仓库：git add healpix_browser_qt/run_healpix.bat，commit
- [ ] 4.5 记录各模块 commit hash
