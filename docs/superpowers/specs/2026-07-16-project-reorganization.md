# 项目根目录整理与文档同步 spec

> 创建日期: 2026-07-16
> 任务类型: 工程重构
> 范围: 根目录调试文件整理 + 系统文档与实施割裂问题记录

---

## 1. 背景与动机

经过长期开发迭代（PSF块扩展、photometric-sigma-residual、SNR模块设计、两段流水线架构重构、HEALPix浏览器Qt6重写等），项目根目录堆积大量一次性诊断脚本与调试输出文件，且系统文档（PROJECT_ARCHITECTURE.md / UI_ARCHITECTURE.md / memory.md）与实际实施存在割裂。

**主要问题**：
1. 根目录混入 24 个调试文件（diag_*.py / diag_*.txt / diag_*.png / healpix_*.png / qt_run_log.txt / run_healpix.bat / 03_fsyn.json），干扰项目结构识别
2. 现有架构文档反映历史阶段状态，与当前两段流水线 10 节点架构、step4 C++化、各模块独立仓库拆分等实施现状存在偏差
3. 缺少专门记录"设计与实施割裂"的文档，导致后续开发难以快速定位断层

---

## 2. 范围

### 2.1 本次整理范围（用户确认）

- **根目录调试文件**：24 个 diag_*/healpix_*/qt_run_log.txt/run_healpix.bat/03_fsyn.json
- **文档与实施割裂问题**：识别并记录架构文档与实际代码的偏差

### 2.2 不在本次范围

- `lib/` 各模块内部冗余文件（如 archive/、REPORT.md、debug_visual.py）—— 各模块独立维护
- `.trae/specs/` 下过时 spec —— spec 历史归档由 .trae 自身管理
- `output/` 目录（pipeline_debug 测试输出）—— 运行时产物
- `siril-1.4.3/`（第三方依赖目录）
- `GaiaDR3/` / `GaiaDR3SP/`（数据库文件）
- `testdata/`（测试数据）

---

## 3. 调试文件分类清单

### 3.1 高价值保留（移入对应模块 tools/ 目录）

| 文件 | 目标位置 | 保留理由 |
|------|----------|----------|
| `diag_gradient_report.py` | `lib/photometric_calib/tools/` | 通用 CSV 输入的梯度诊断报告生成器，可复用于任意 step4 输出 |
| `diag_projection_plot.py` | `lib/plate_solve/tools/` | WCS 投影精度可视化（Gaia 投影 vs 图像星点），含中文字体配置，可复用 |
| `run_healpix.bat` | `lib/healpix_db/healpix_browser_qt/` | Qt6 浏览器启动脚本（含 PATH/QT_PLUGIN_PATH 配置），日常使用 |

### 3.2 归档（分散归档至对应模块 `archive/debug_2026-07/`）

| 文件 | 目标位置 | 归档理由 |
|------|----------|----------|
| `diag_wcs.py` | `lib/plate_solve/archive/debug_2026-07/` | 一次性 WCS 基本信息诊断，硬编码 Galaxy_Center_T4 路径 |
| `diag_wcs_offset.py` | `lib/plate_solve/archive/debug_2026-07/` | WCS 边缘残差诊断，针对特定帧 |
| `diag_psf_edge.py` | `lib/dynamic_psf/archive/debug_2026-07/` | 边缘星 vs 中心星 PSF 对比，硬编码路径 |
| `diag_psf_root_cause.py` | `lib/dynamic_psf/archive/debug_2026-07/` | PSF 拟合根因诊断，针对特定帧边缘 100% 失败问题 |
| `diag_image_stats.py` | `lib/dynamic_psf/archive/debug_2026-07/` | 图像值域+背景快速诊断，硬编码路径 |
| `diag_distribution.py` | `lib/photometric_calib/archive/debug_2026-07/` | 星点空间分布诊断，依赖特定 03_fsyn.json |
| `diag_light.py` | `lib/photometric_calib/archive/debug_2026-07/` | 轻量星点分布诊断，一次性 |

### 3.3 直接删除（一次性输出产物）

| 文件 | 删除理由 |
|------|----------|
| `diag_light_output.txt` | diag_light.py 输出 |
| `diag_output.txt` | 诊断输出 |
| `diag_plot_log.txt` | 绘图日志 |
| `diag_resolve_log.txt` | 解析日志 |
| `diag_wcs_output.txt` | diag_wcs.py 输出 |
| `diag_gradient_report.png` | diag_gradient_report.py 输出（脚本可重新生成） |
| `diag_projection.png` | 投影诊断输出 |
| `diag_projection_after.png` | 投影诊断输出 |
| `diag_projection_before.png` | 投影诊断输出 |
| `diag_projection_current.png` | 投影诊断输出 |
| `healpix_after_fix.png` | healpix 修复后截图 |
| `healpix_window.png` | healpix 窗口截图 |
| `qt_run_log.txt` | Qt 运行日志 |
| `03_fsyn.json` | 特定帧的流量同步数据（测试数据产物） |

---

## 4. 文档处理策略

### 4.1 当前文档归档

将以下文档移入 `docs/archive/`：
- `PROJECT_ARCHITECTURE.md` → `docs/archive/PROJECT_ARCHITECTURE_2026-07-16.md`
- `UI_ARCHITECTURE.md` → `docs/archive/UI_ARCHITECTURE_2026-07-16.md`

`memory.md` 保留在根目录（项目记忆持续更新，不归档）。

### 4.2 新增架构文档

创建 `docs/ARCHITECTURE.md`：归纳当前真实架构，包含：
- 项目概述与核心设计原则（C++核心 + Python调试层、PipelineFrame、两段流水线、分模块独立仓库）
- 模块清单（lib/ 下实际存在的模块，标注状态：活跃/已废弃/已归档）
- 两段流水线 10 节点架构（stage1: 0-7, stage2: 8-9）
- 数据流与格式体系（.hiss / .hcsd）
- 模块依赖关系
- 性能基线

### 4.3 新增割裂记录文档

创建 `docs/DESIGN_IMPL_GAP.md`：记录设计与实施割裂问题，包含：
- 架构文档描述 vs 实际代码的偏差
- 模块状态标注 vs 实际仓库拆分状态
- spec 设计 vs 实施完成的差距
- 待修复的断层清单（drizzle落盘/hiss格式/Python绑定/stack加权等）

### 4.4 memory.md 更新

在 memory.md 中追加本次整理记录，并修正与实际不符的条目。

---

## 5. 验证标准

> **重要前提**：根目录不是 git 仓库，各模块（lib/plate_solve 等）是独立 git 仓库。
> 因此 git 提交仅适用于移入模块的文件；根目录文档与调试文件用普通文件操作。

### 5.1 git 提交历史清洁
- 移入模块的文件（3 个 tools/ + run_healpix.bat + 7 个归档脚本）在对应模块仓库 git add + commit
- commit message 遵循项目规范
- 根目录文档（docs/、archive/）非 git 跟踪，无需提交

### 5.2 目录结构清单核对
- 根目录无调试文件（diag_*/healpix_*.png/qt_run_log.txt 等）
- `lib/plate_solve/archive/debug_2026-07/` 含 2 个归档脚本 + README.md
- `lib/dynamic_psf/archive/debug_2026-07/` 含 3 个归档脚本 + README.md
- `lib/photometric_calib/archive/debug_2026-07/` 含 2 个归档脚本 + README.md
- `lib/photometric_calib/tools/diag_gradient_report.py` 存在
- `lib/plate_solve/tools/diag_projection_plot.py` 存在
- `lib/healpix_db/healpix_browser_qt/run_healpix.bat` 存在
- `docs/ARCHITECTURE.md`、`docs/DESIGN_IMPL_GAP.md` 存在
- `docs/archive/` 含 2 个归档文档
- `memory.md` 已更新整理记录

---

## 6. 回滚策略

- 根目录文件移动前记录文件清单（已在 spec §3 列出）
- 根目录非 git 仓库，无法 git reset；移动/删除前确认文件清单可恢复（归档脚本可通过移回根目录恢复，删除文件为一次性输出可重新生成）
- 移入模块的文件若需回滚：在对应模块仓库 `git reset HEAD~1` 恢复
- 若验证失败：将归档脚本移回根目录，恢复 docs/archive/ 中的旧文档
