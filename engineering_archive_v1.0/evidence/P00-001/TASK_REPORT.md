# TASK_REPORT: P00-001 冻结并复核主仓库基线

## 任务信息
- **Task ID**: P00-001
- **Phase**: P00 基线冻结与仓库完整性恢复
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 开始前状态
- **Commit**: eb44f655bbbbfbc5c8c1629fef5d4762c52f0628 (eb44f65)
- **基线 Commit** (控制包导出时): 9f10c72ceb635b726fa995c72c85e4635b36fabf (9f10c72)
- **分支**: main
- **Dirty**: `.astrocs_agent_bootstrap/`、`AstroCS_Autonomous_Agent_Pack_2026-07-24_v2.zip`、`engineering/` 未跟踪
- **远端**: origin https://github.com/fujiaze/Astro-CS-Database.git
- **Tags**: 0

## 执行步骤
1. 运行 `python engineering/tools/repo_preflight.py --repo . --output engineering/evidence/P00-001`
2. 复核 preflight.json 和 preflight.md
3. 验证 healpix_drizzle/healpix_stack 的 git 跟踪状态
4. 统计各模块跟踪文件数
5. 识别 CI 文件来源
6. 二次运行预检验证可重复性

## 关键发现

### 1. 已跟踪模块（11 个，全部存在）
| 模块 | 跟踪文件数 |
|---|---|
| astro_image_io | 59 |
| calibration | 31 |
| data_pipeline | 8 |
| dynamic_psf | 12 |
| gaia_xpsd_client | 12 |
| healpix_db | 35 |
| orchestrator | 37 |
| photometric_calib | 32 |
| plate_solve | 55 |
| snr_estimator | 8 |
| star_detector | 22 |
| **合计** | **311** |

总跟踪文件：350（含 docs/、根文件、.gitattributes、.gitignore、memory.md 等）

### 2. 未受控源码（2 个模块）
- `lib/healpix_db/healpix_drizzle` — 本地存在完整源码（含独立 .git），但主仓库 git ls-files 返回 0 文件
- `lib/healpix_db/healpix_stack` — 本地存在完整源码（含独立 .git），但主仓库 git ls-files 返回 0 文件

原因：这两个模块此前被拆分为独立 GitHub 仓库（Healpix-Drizzle-Cpp / Healpix-Mosaic-Cpp），本地保留 clone 但未纳入主仓库版本控制。Git 因检测到嵌套 .git 目录而拒绝跟踪其内部文件。

### 3. 构建入口
- **根级 CMakeLists.txt**: 不存在（无统一构建入口）
- **根级 requirements.txt**: 不存在
- 各模块有独立 Makefile（11 个模块各 1 个）+ build.ps1（24 个 .ps1 脚本）

### 4. CI/CD
- 主仓库自身无 CI 配置
- 9 个 CI 文件全部来自第三方源码（siril-1.4.3、eigen-3.4.0、nanoflann-master），均为第三方项目自带

### 5. 测试文件
- 1175 个测试文件（含 test_/tests/ 路径下的文件）
- 无统一测试入口

### 6. Git 状态
- 分支: main，跟踪 origin/main
- Tags: 0（无 baseline tag）
- Submodules: 无
- 远端: origin https://github.com/fujiaze/Astro-CS-Database.git

### 7. HEAD 与基线差异
- 控制包导出基线: 9f10c72
- 当前 HEAD: eb44f65（比基线多 1 个文档提交：memory.md 补充封存记录）
- 差异性质: 仅文档更新，不影响代码基线完整性

## 阻塞项
- 无硬阻塞。healpix_drizzle/healpix_stack 源码在本地存在，P00-002/003 可从本地恢复。

## 允许范围遵守
- 仅修改了 `engineering/evidence/P00-001/**` 和 `engineering/control/**`
- 未修改 `lib/**`、`docs/**`、构建脚本与算法配置
- 未修改业务代码

## 变更文件清单
- `engineering/evidence/P00-001/preflight.json`（新增）
- `engineering/evidence/P00-001/preflight.md`（新增）
- `engineering/evidence/P00-001/artifacts.sha256`（新增）
- `engineering/evidence/P00-001/TASK_REPORT.md`（新增）
- `engineering/evidence/P00-001/TEST_REPORT.md`（新增）
- `engineering/evidence/P00-001/EVIDENCE_INDEX.md`（新增）

## 下一任务
P00-002: 恢复并固定 healpix_drizzle 源码
