# ACR 禁止修改路径清单

**生成日期**: 2026-08-02
**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**规则**: path guard 每次提交前检查 `git diff --name-only`，越界文件立即回退

## 1. 算法实现目录（绝对禁止修改）

以下 `lib/` 子目录是现有算法实现，ACR 分支**只读**：

| 目录 | 用途 |
|---|---|
| `lib/astro_image_io/` | FITS/XISF/HISS 读写、pipeline |
| `lib/calibration/` | 校准（bias/dark/flat/photometry apply） |
| `lib/data_pipeline/` | 数据管道 |
| `lib/dynamic_psf/` | 动态 PSF |
| `lib/gaia_xpsd_client/` | Gaia 客户端 |
| `lib/healpix_db/` | HEALPix drizzle/stack/browser |
| `lib/orchestrator/` | 调度器 |
| `lib/photometric_calib/` | 测光定标 |
| `lib/plate_solve/` | plate solving |
| `lib/snr_estimator/` | SNR 估计 |
| `lib/star_detector/` | 星检测 |

## 2. 顶层文件（禁止修改）

| 文件/目录 | 用途 |
|---|---|
| `AGENTS.md` | AI 代理操作指南 |
| `AstroCS.wiki/` | Wiki 仓库本地副本 |
| `README.md` | 仓库说明 |
| `.gitignore` / `.gitattributes` | git 配置（如需调整须单独授权） |
| `tools/astro_toolkit.py` | 主工具 |
| `tools/gen_audit_pack.py` | 审查包生成器 |
| `tools/vq-commit.ps1` | 备用提交脚本 |
| `testdata/` | 只读测试数据 |
| `工程控制/`（除 `tasks/acr/`、`evidence/acr/` 外的现有内容） | 工程控制包 |

## 3. 允许修改/新增

| 路径 | 用途 |
|---|---|
| `lib/acr/` | ACR 全部源码、测试、工具、文档、CMake |
| `工程控制/tasks/acr/` | spec 三件套 |
| `工程控制/evidence/acr/` | 实验证据 |

## 4. path guard 脚本

`lib/acr/ci/path_guard.ps1` 检查 `git diff --name-only HEAD`：
- 允许前缀：`lib/acr/`、`工程控制/tasks/acr/`、`工程控制/evidence/acr/`
- 任何不在允许前缀内的文件 = 越界 → 非零退出 + 立即回退

## 5. 例外（需单独授权）

- 顶层 `.gitignore` 如需追加 ACR 相关忽略项（如 `lib/acr/build/`），须用户授权
- 本 spec 已确定不修改顶层 `.gitignore`（`run/*` 和 `build/` 已被主仓库 .gitignore 覆盖）
