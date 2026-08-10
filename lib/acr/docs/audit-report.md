# ACR 仓库审计报告

**审计日期**: 2026-08-02
**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`

## 1. 仓库基线

- **主仓库路径**: `F:\Astro dev\Astro CS Normalization Database`
- **远端**: `origin https://github.com/fujiaze/Astro-CS-Database.git`
- **主分支**: `main`
- **base commit**: `8f5051946e9ea824ceefa6a90a071de7cad31a98` (origin/main HEAD)
- **当前 worktree 分支**: `feature/astrocompute-runtime`（从 origin/main 创建）
- **worktree 路径**: `run/worktrees/acr/`（沙箱内隔离，`run/*` 已 gitignored）

## 2. 构建系统现状

- **顶层 CMakeLists.txt**: 无（仓库根无顶层 CMake）
- **现有模块构建**: 各 `lib/<module>/Makefile` + `build.ps1`
- **编译器**: MSYS2 MinGW64 g++ 16.1.0（C++17，`-fopenmp`）
- **C++ 标准**: 现有模块用 C++17；ACR 用 C++20（alpaka 1.2.0 要求）
- **OpenMP**: 现有模块大量使用 `-fopenmp`；ACR 用 oneTBB，不删不改现有 OpenMP

## 3. 平台与工具链

| 工具 | 版本 | 路径 |
|---|---|---|
| g++ | 16.1.0 (Rev4, MSYS2) | `C:\msys64\mingw64\bin\g++.exe` |
| CUDA toolkit | 11.8 | `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8` |
| GPU | NVIDIA GeForce RTX 3060 Ti | 驱动 595.79 |
| Python | 系统默认 | 用于 astro_toolkit.py |
| gh CLI | 已装 | `C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe` |
| vcpkg/Conan | 无 | 全部用 CMake FetchContent |
| MSYS2 pacman | 可用 | 备用依赖源 |

## 4. 现有第三方库

**仓库内无任何第三方 C++ 库**（alpaka/oneTBB/hwloc/cpu_features/Catch2/gtest/fmt/spdlog/json/cli11 全无）。
MSYS2 mingw64/include 下也无这些库的头文件。
→ ACR 全部依赖通过 CMake FetchContent 拉取固定 tag。

## 5. 测试框架现状

- 现有 `lib/*/tests/test_*.cpp` 为**自写测试**，未用任何框架（无 catch2/gtest include）
- ADR-006 选择 GoogleTest 1.15.2（控制包 ADR 授权）

## 6. astro_toolkit.py 自检

```
python tools/astro_toolkit.py tools/_acr_selfcheck.json --log run/logs/toolkit_acr_selfcheck.log
→ ok: true
→ git_status + list_dir step 通过
```
路径: `tools/astro_toolkit.py`（主仓库），后续 commit 编排通过此工具。

## 7. 现有 lib/ 模块（禁止修改区）

11 个模块：astro_image_io、calibration、data_pipeline、dynamic_psf、gaia_xpsd_client、healpix_db、orchestrator、photometric_calib、plate_solve、snr_estimator、star_detector。

详见 `forbidden-paths.md`。

## 8. CMake 接入点

- **不引入顶层 CMakeLists.txt**（避免与未来 main 顶层 CMake 冲突）
- ACR 子目录 `lib/acr/CMakeLists.txt` 独立构建
- CPU-only 默认 ON，CUDA backend 默认 OFF（`ACR_BUILD_CUDA`）

## 9. worktree 隔离策略

- 主仓库留在 `experiment/fast-drizzle-r06`，工作区未提交改动**不动**
- worktree `run/worktrees/acr/` 从 `origin/main` 创建 `feature/astrocompute-runtime`
- 共享主仓库 `.git`，push 时用 PowerShell
- 开发期间定期 `git fetch origin && git merge --no-ff origin/main` 同步
- 合并前再 sync 最新 main + path guard 终检

## 10. 风险与缓解

| 风险 | 缓解 |
|---|---|
| main 前进导致 feature 落后 | 定期 sync，ACR 只新增 lib/acr/ 不与算法改动重叠 |
| 顶层 .gitignore 冲突 | ACR 不改 .gitignore（run/* 和 build/ 已覆盖） |
| 第三方 ABI 冲突 | namespaced CMake target，dependency-lock |
| alpaka 工具链差异 | feature gate，CPU-only 永远可用 |
