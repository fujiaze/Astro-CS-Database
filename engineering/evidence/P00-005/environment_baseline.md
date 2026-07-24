# AstroCS 工具链环境基线

- **Task ID**: P00-005
- **生成时间**: 2026-07-24
- **主机**: Windows 10.0.26220.0 (AMD64)

## 工具链清单

| # | 工具 | 版本 | 路径 | 许可证 |
|---|---|---|---|---|
| 1 | PowerShell | 7.6.3 | C:\Users\fujia\AppData\Local\Programs\PowerShell\7\pwsh.exe | MIT |
| 2 | Python | 3.10.11 | C:\Users\fujia\AppData\Roaming\TRAE SOLO CN\...\python.exe | PSF |
| 3 | Git | 2.53.0.windows.1 | C:\Program Files\Git\cmd\git.exe | GPLv2 |
| 4 | GitHub CLI | 2.63.2 | C:\Users\fujia\AppData\Local\Temp\gh-cli-install\bin\gh.exe | MIT |
| 5 | GCC | 16.1.0 (MSYS2 Rev4) | C:\msys64\mingw64\bin\gcc.exe | GPLv3+runtime exception |
| 6 | G++ | 16.1.0 (MSYS2 Rev4) | C:\msys64\mingw64\bin\g++.exe | GPLv3+runtime exception |
| 7 | mingw32-make | 4.4.1 | C:\msys64\mingw64\bin\mingw32-make.exe | GPLv3 |
| 8 | Make (TRAE) | 4.4.1 | C:\Users\fujia\AppData\Roaming\TRAE SOLO CN\...\make.cmd | GPLv3 |
| 9 | Qt6 | 6.11.0 | C:\msys64\mingw64\bin\qmake6.exe | GPLv3/LGPLv3/Commercial |
| 10 | GSL | 2.8 | C:\msys64\mingw64\bin\libgsl-28.dll | GPLv3 |
| 11 | GSL CBLAS | 2.8 | C:\msys64\mingw64\bin\libgslcblas-0.dll | GPLv3 |
| 12 | zstd | — | C:\msys64\mingw64\bin\libzstd.dll | BSD-3-Clause |
| 13 | lz4 | — | C:\msys64\mingw64\bin\liblz4.dll | BSD-2-Clause |
| 14 | zlib | — | C:\msys64\mingw64\bin\zlib1.dll | zlib License |
| 15 | OpenMP (libgomp) | 16.1.0 | C:\msys64\mingw64\bin\libgomp-1.dll | GPLv3+runtime exception |
| 16 | Eigen3 | 5.0.1 | C:\msys64\mingw64\include\eigen3 (头文件库) | MPL-2.0 |

## 关键二进制 SHA-256

| 文件 | SHA-256 |
|---|---|
| gcc.exe | 9909A5E830DC5E9740D4958A99ECE7797652F1F30756C6AB54C51867BBA4765C |
| g++.exe | 805AEB690FAD8CB3DDFE8065998B2337B2CBF8C5F07B2ABCE5DCAFF5420535D0 |
| mingw32-make.exe | C7D3BE056FEB5EE8C1F236114CA97D3589C644963DE88D53F5E1D623F7EC7844 |
| qmake6.exe | 36BE79B55084BD5C7BD91517D88E2B99A88CEEF20E313834FD1AB48AD91211D2 |
| libgsl-28.dll | F133DD12A4F5DA84981D5AE8165A7FF83409579B99BB0FF7F1FE39FE7FEC7F4E |
| libgslcblas-0.dll | 0FE783C4F188F5D891602898A5284EBAEB8859E089E4E9922C8F2FF8CF6CD843 |
| libzstd.dll | B95C223A9548A9ECF51377C962E0BC8F0C51EB0C6F67A296DBC885996F0DD40D |
| liblz4.dll | 35F917274BCA8F19677BA66F1B3CC3C83568C3249FC43215FC16E76439C8E856 |
| zlib1.dll | 93E9243A44C29200EEACAF9658EFE2558581770E4B11CA4B500E18E424A6E3B5 |
| libgomp-1.dll | 53ADE6D7001F8E8E02F22CA266A25C0A7BA61B5231EA1DC7E6713DFEB865C9D0 |
| python.exe | 77177A53D091575C514D5BD19DF3EE40665A26459FB92D71319F139291440D98 |
| git.exe | DA240FE9BC24895B3E04150A4990B8A6FF329ECABCD8F19684C2CC310DA5EF3F |
| gh.exe | C9D45316C3EE3270CCA8A713C93F3FB48855EF8CFFB90A80D356AD265A9C2275 |

## 路径问题

1. **GCC/G++/mingw32-make 不在默认 PATH** — 直接运行 `make`/`g++` 会失败；构建脚本（build.ps1）需显式调用 `C:\msys64\mingw64\bin` 或将其加入 PATH。当前各模块 build.ps1 通过 MSYS2 环境调用。
2. **PATH 中存在两个 make** — TRAE 自带 make.cmd (4.4.1) 与 mingw32-make.exe (4.4.1) 并存。命令行直接 `make` 可能调用 TRAE 版本而非 MinGW 版本。构建时应显式使用 `mingw32-make` 或通过 build.ps1。
3. **qmake6 不在默认 PATH** — healpix_browser_qt 构建需显式指定 Qt6 路径。CMake 通过 CMAKE_PREFIX_PATH 或 qmake6.exe 全路径定位。

## 与 P00-004 依赖图的对应

| P00-004 识别的外部库 | P00-005 采集的版本 |
|---|---|
| astro_image_io: -lzstd/-llz4 | zstd/libzstd.dll, lz4/liblz4.dll |
| calibration: -fopenmp | libgomp-1.dll (GCC 16.1.0) |
| dynamic_psf: -fopenmp | libgomp-1.dll |
| gaia_xpsd_client: -lz | zlib/zlib1.dll |
| star_detector: -lgsl/-lgslcblas | GSL 2.8 (libgsl-28.dll, libgslcblas-0.dll) |
| healpix_stack: Eigen3 | Eigen3 5.0.1 |
| healpix_browser_qt: Qt6/OpenGL | Qt6 6.11.0 |
| 所有模块: GCC/G++ | GCC/G++ 16.1.0 (MSYS2) |

## 可复现步骤

1. 在 Windows 10/11 (AMD64) 主机上安装 MSYS2
2. 通过 `pacman` 安装 mingw-w64-x86_64-gcc / qt6 / gsl / zstd / lz4 / zlib / eigen3
3. 安装 PowerShell 7、Python 3.10+、Git、GitHub CLI
4. 对照本表验证版本与 SHA-256
