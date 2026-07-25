# AstroCS 依赖锁定清单 — P01-002
- **生成时间**: 2026-07-24T11:58:16.817746+00:00
- **基线 Tag**: `astrocs-baseline-p00`
- **Schema**: `dependencies.lock/v1`

## 1. 工具链锁定

| # | 工具 | 版本 | 路径 | SHA-256 |
|---|---|---|---|---|
| 1 | PowerShell | 7.6.3 | `C:\Users\fujia\AppData\Local\Programs\PowerShell\7\pwsh.exe` | — |
| 2 | Python | 3.10.11 | `C:\Users\fujia\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\vm\tools\python\python.exe` | 77177A53D091575C... |
| 3 | Git | 2.53.0.windows.1 | `C:\Program Files\Git\cmd\git.exe` | DA240FE9BC24895B... |
| 4 | GitHub CLI (gh) | 2.63.2 (2024-12-05) | `C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe` | C9D45316C3EE3270... |
| 5 | GCC | 16.1.0 (Rev4, Built by MSYS2 project) | `C:\msys64\mingw64\bin\gcc.exe` | 9909A5E830DC5E97... |
| 6 | G++ | 16.1.0 (Rev4, Built by MSYS2 project) | `C:\msys64\mingw64\bin\g++.exe` | 805AEB690FAD8CB3... |
| 7 | mingw32-make | 4.4.1 | `C:\msys64\mingw64\bin\mingw32-make.exe` | C7D3BE056FEB5EE8... |
| 8 | Make (TRAE bundled) | 4.4.1 | `C:\Users\fujia\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\vm\tools\bin\make.cmd` | — |
| 9 | Qt6 | 6.11.0 | `C:\msys64\mingw64\bin\qmake6.exe` | 36BE79B55084BD5C... |
| 10 | GSL (GNU Scientific Library) | 2.8 | `C:\msys64\mingw64\bin\libgsl-28.dll` | F133DD12A4F5DA84... |
| 11 | GSL CBLAS | 2.8 | `C:\msys64\mingw64\bin\libgslcblas-0.dll` | 0FE783C4F188F5D8... |
| 12 | zstd | None | `C:\msys64\mingw64\bin\libzstd.dll` | B95C223A9548A9EC... |
| 13 | lz4 | None | `C:\msys64\mingw64\bin\liblz4.dll` | 35F917274BCA8F19... |
| 14 | zlib | None | `C:\msys64\mingw64\bin\zlib1.dll` | 93E9243A44C29200... |
| 15 | OpenMP (libgomp) | 16.1.0 (随 GCC) | `C:\msys64\mingw64\bin\libgomp-1.dll` | 53ADE6D7001F8E8E... |
| 16 | Eigen3 | 5.0.1 | `C:\msys64\mingw64\include\eigen3` | — |

## 2. 外部库锁定

| 库 | 版本 | DLL | 使用模块 |
|---|---|---|---|
| GSL (GNU Scientific Library) | 2.8 | libgsl-28.dll | star_detector |
| GSL CBLAS | 2.8 | libgslcblas-0.dll | star_detector |
| zstd | None | libzstd.dll | astro_image_io |
| lz4 | None | liblz4.dll | astro_image_io |
| zlib | None | zlib1.dll | gaia_xpsd_client, healpix_stack |
| OpenMP (libgomp) | 16.1.0 (随 GCC) | libgomp-1.dll | calibration, dynamic_psf, gaia_xpsd_client, star_detector, snr_estimator, photometric_calib, plate_solve_ipv, healpix_drizzle, astro_image_io |
| Qt6 | 6.11.0 | (header-only/framework) | healpix_browser_qt |
| Eigen3 | 5.0.1 | (header-only/framework) | healpix_stack |

## 3. 模块构建锁定

| 模块 | 权威构建 | 输出 | 源文件数 | Makefile 过时 |
|---|---|---|---|---|
| astro_image_io | build.ps1 | astro_image_io.dll | 11 | ⚠ 是 |
| calibration | build.ps1 | astro_calibration.dll | 4 | ⚠ 是 |
| dynamic_psf | makefile | dynamic_psf.dll | 3 | 否 |
| gaia_xpsd_client | makefile | gaia_client.dll | 1 | 否 |
| star_detector | makefile | star_detector.dll | 5 | 否 |
| snr_estimator | build.ps1 | snr_estimator.dll | 1 | 否 |
| photometric_calib | build.ps1 | photometric_calib.dll | 5 | 否 |
| plate_solve_ipv | build.ps1 | ipv_solver.dll | 13 | 否 |
| healpix_drizzle | makefile | healpix_drizzle.dll | 5 | 否 |
| healpix_stack | build.ps1 | healpix_stack.dll | 13 | ⚠ 是 |
| healpix_browser_qt | cmake | — | 0 | 否 |
| orchestrator | makefile | orchestrator.exe | 7 | 否 |
| data_pipeline | none | — | 0 | 否 |

## 4. 构建顺序

- **layer_1_base**: astro_image_io, calibration, dynamic_psf, gaia_xpsd_client, star_detector, snr_estimator
- **layer_2_middle**: healpix_drizzle, healpix_stack, photometric_calib, healpix_browser_qt
- **layer_3_top**: orchestrator, plate_solve_ipv
- **no_build**: data_pipeline

> 按 P00-004 依赖图分层: 基础层(无跨模块依赖) -> 中间层(依赖基础层) -> 顶层(运行时动态加载)

## 5. 路径要求

- MSYS2 MinGW64 bin: `C:\msys64\mingw64\bin`
- 必须在 PATH: True
> GCC/mingw32-make/qmake6 不在默认 PATH, 根级 build.ps1 需注入此路径

## 6. 锁定完整性

| 来源文件 | SHA-256 |
|---|---|
| environment_baseline_sha256 | 42e5d2fd1d03748e5170e22555c9963a... |
| module_build_configs_sha256 | d6a61a55985fc127ae62a451ae14a85a... |
| dependency_graph_sha256 | 6f2ce8f21063e971bcc77c40c335a4b3... |
