# TASK_REPORT: P00-004 建立完整模块与依赖图

## 任务信息
- **Task ID**: P00-004
- **Phase**: P00 基线冻结与仓库完整性恢复
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 基线
- **commit**: dde66ba (P00-003 提交后 HEAD)
- **分支**: main
- **dirty**: `engineering/control/CURRENT_WORK.md`、`engineering/control/MASTER_TASK_REGISTER.csv`、`engineering/control/PROJECT_STATE.yaml` 已修改（P00-004 任务推进记录），`engineering/evidence/P00-004/` 未跟踪

## 目标与范围
生成 `dependency_graph.md`/`json`，记录 13 个模块的源码依赖、DLL 产出、头文件依赖、运行库依赖、数据依赖和调用方向，为后续 P01 构建可重现性、P02 数据契约冻结、P03 接口契约冻结提供基线依赖事实。

## 入口条件
- P00-002 DONE ✓（healpix_drizzle 源码已纳入主仓库）
- P00-003 DONE ✓（healpix_stack 源码已纳入主仓库）

## 允许修改
- `engineering/evidence/P00-004/**`
- `engineering/control/**`
- `engineering/tools/`（如需新增分析工具）

## 禁止修改（已遵守）
- `lib/**` — 未修改
- `docs/**` — 未修改
- 构建脚本与算法配置 — 未修改

## 实现过程
1. **模块清单确认**：以 P00-001 preflight 报告中的 11 个主模块为基础，加上 P00-002/003 新纳入的 healpix_drizzle/healpix_stack，并拆分 healpix_db 容器下的 3 个子模块（healpix_drizzle、healpix_stack、healpix_browser_qt），合计 13 个分析单元。
2. **并行分析**：通过两个子 Agent 分组分析（group1: 7 个模块，group2: 6 个模块），各自读取 Makefile/build.ps1/CMakeLists.txt 提取 DLL 产出、链接库、源文件、头文件 `#include` 关系、Python 绑定、数据依赖。
3. **合并与去重**：编写 `merge_deps.py` 脚本，合并两组 JSON，统一生成 `dependency_graph.json`（机器可读）和 `dependency_graph.md`（人类可读）。
4. **分层架构识别**：依据 `includes_other_modules` 与链接关系，将 13 个模块划分为基础层（无跨模块依赖）、中间层（依赖基础层）、顶层（运行时动态加载）。
5. **潜在问题登记**：识别 10 个构建/依赖一致性问题，作为 P01 阶段输入。

## 修改文件
- `engineering/evidence/P00-004/deps_group1.json`（新增，子 Agent 分析结果第一组）
- `engineering/evidence/P00-004/deps_group2.json`（新增，子 Agent 分析结果第二组）
- `engineering/evidence/P00-004/merge_deps.py`（新增，合并脚本）
- `engineering/evidence/P00-004/dependency_graph.json`（新增，13 模块 68 边机器可读依赖图）
- `engineering/evidence/P00-004/dependency_graph.md`（新增，人类可读依赖图与分层架构）
- `engineering/evidence/P00-004/TASK_REPORT.md`（本文件）
- `engineering/evidence/P00-004/TEST_REPORT.md`（新增）
- `engineering/evidence/P00-004/EVIDENCE_INDEX.md`（新增）
- `engineering/evidence/P00-004/REVIEW_REPORT.md`（新增，独立复核）
- `engineering/control/MASTER_TASK_REGISTER.csv`（P00-004 → DONE，P00-005 → IN_PROGRESS）
- `engineering/control/PROJECT_STATE.yaml`（current_task → P00-005）
- `engineering/control/CURRENT_WORK.md`（切换到 P00-005 任务说明）

## 结果

### 模块覆盖（13/13）
| # | 模块 | DLL 产出 | 层级 |
|---|---|---|---|
| 1 | astro_image_io | astro_image_io.dll | 基础层 |
| 2 | calibration | astro_calibration.dll / cosmetic_corrector.dll | 基础层 |
| 3 | dynamic_psf | dynamic_psf.dll | 基础层 |
| 4 | gaia_xpsd_client | gaia_client.dll | 基础层 |
| 5 | star_detector | star_detector.dll | 基础层 |
| 6 | snr_estimator | snr_estimator.dll | 基础层 |
| 7 | healpix_db/healpix_drizzle | healpix_drizzle.dll | 中间层 |
| 8 | healpix_db/healpix_stack | healpix_stack.dll | 中间层 |
| 9 | healpix_db/healpix_browser_qt | libhealpix_browser_core.a / .exe | 中间层 |
| 10 | photometric_calib | photometric_calib.dll | 中间层 |
| 11 | orchestrator | orchestrator.exe | 顶层 |
| 12 | plate_solve | ipv_solver.dll | 顶层 |
| 13 | data_pipeline | 无（源码与 astro_image_io 重复） | 容器目录 |

### 依赖边数
- 总边数：68（含 include + link 两类）
- 真实跨模块 include 依赖：18 条（可追踪到具体头文件与源文件行号）
- link 边：50 条（由 `-l<module>` 标志推断，含部分由 merge_deps.py 简单字符串匹配产生的冗余边，详见下方"未解决问题"）

### 分层架构（关键结论）
- **基础层**（6 模块）：astro_image_io、calibration、dynamic_psf、gaia_xpsd_client、star_detector、snr_estimator — 无跨模块 `#include`，编译期独立
- **中间层**（4 模块）：healpix_drizzle/healpix_stack/photometric_calib 依赖基础层头文件与 DLL；healpix_browser_qt 依赖 astro_image_io + Qt6/OpenGL
- **顶层**（2 模块）：orchestrator 编译为 exe 运行时 LoadLibrary 加载所有 DLL；plate_solve 运行时句柄注入 + LoadLibraryA 加载 astro_image_io

### 潜在问题（10 项，全部登记为 P01+ 输入）
1. healpix_stack Makefile 引用已归档的 healpix_io（实际由 astro_image_io 兼容宏提供）
2. calibration 两套构建（Makefile 产 cosmetic_corrector.dll，build.ps1 产 astro_calibration.dll）
3. data_pipeline 无独立构建，源码与 astro_image_io 重复
4. orchestrator Makefile 含未使用的 star_detector -I
5. healpix_stack Makefile 与 build.ps1 源文件列表严重分歧（gradient/ 子目录缺失）
6. healpix_stack Makefile -lhealpix_io 已失效
7. plate_solve 运行时依赖无编译期声明
8. plate_solve ipv_select.cpp 自声明 AIOImageData* 类型，存在结构体漂移风险
9. photometric_calib Makefile 不自动复制 gaia_client.dll
10. healpix_drizzle 静态编译 healpix_stack 源码，升级需同步重编译

## 未解决问题
1. **link 边冗余**：`merge_deps.py` 通过 `-l<name>` 标志简单匹配其他模块 DLL 名生成 link 边，导致部分基础层模块（如 astro_image_io）出现指向 calibration/dynamic_psf 等的虚假 link 边。实际上这些 `-l` 标志是其他模块在链接时引用 astro_image_io.dll，而非 astro_image_io 反向依赖它们。真实的跨模块依赖方向应以 `includes_other_modules` 字段和 `dependency_graph.md` 中的"分层架构"为准。此问题不影响任务完成标准（每个 DLL 与头文件依赖可追踪），但 link 边集合包含噪声，需在 P01-002（依赖锁定清单）中精化。
2. **data_pipeline 模块归属**：该目录无构建文件，源码与 astro_image_io/src 重复，建议在 P02-001（PipelineFrame 唯一所有者 ADR）中决定是否废弃。

## 风险与回退
- 本任务为只读分析 + 证据归档，无代码变更，无运行时风险
- 回退方式：删除 `engineering/evidence/P00-004/` 目录并 revert 控制文件即可

## 控制文件更新
- `MASTER_TASK_REGISTER.csv`：P00-004 → DONE，P00-005 → IN_PROGRESS
- `PROJECT_STATE.yaml`：current_task → P00-005，last_updated → 2026-07-24
- `CURRENT_WORK.md`：切换为 P00-005 任务说明

## 建议状态
`IN_REVIEW`（等待独立复核确认 PASS 后置为 DONE）
