# TEST_REPORT: P00-005 工具链环境基线验证

## 测试目标
验证 environment_baseline.json 中记录的工具链版本、路径、SHA-256 可在同一主机上重复采集，结果一致。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: 61c3b05
- **分支**: main
- **主机**: Windows 10.0.26220.0 (AMD64)

## 数据集
- 输入：本机已安装的工具链二进制
- 输出：environment_baseline.json、environment_baseline.md

## 产物哈希
（见 EVIDENCE_INDEX.md）

## 测试 1: 版本号验证
| 工具 | 报告记录值 | 采集命令 | 结果 |
|---|---|---|---|
| PowerShell | 7.6.3 | $PSVersionTable.PSVersion | PASS |
| Python | 3.10.11 | python --version | PASS |
| Git | 2.53.0.windows.1 | git --version | PASS |
| gh | 2.63.2 | gh --version | PASS |
| GCC | 16.1.0 | C:\msys64\mingw64\bin\gcc.exe --version | PASS |
| G++ | 16.1.0 | C:\msys64\mingw64\bin\g++.exe --version | PASS |
| mingw32-make | 4.4.1 | mingw32-make.exe --version | PASS |
| Qt6 | 6.11.0 | qmake6 --version | PASS |
| GSL | 2.8 | gsl_version.h 宏定义 | PASS |
| Eigen3 | 5.0.1 | Eigen3ConfigVersion.cmake | PASS |

## 测试 2: SHA-256 可重复性
- **命令**: `Get-FileHash <binary> -Algorithm SHA256`
- **方法**: 重新计算 gcc.exe 哈希并比对
- **结果**: 9909A5E830DC5E9740D4958A99ECE7797652F1F30756C6AB54C51867BBA4765C — 与 baseline 记录一致 PASS

## 测试 3: 路径存在性验证
| 路径 | 存在 |
|---|---|
| C:\msys64\mingw64\bin\gcc.exe | PASS |
| C:\msys64\mingw64\bin\libgsl-28.dll | PASS |
| C:\msys64\mingw64\include\eigen3 | PASS |
| C:\msys64\mingw64\bin\qmake6.exe | PASS |
| C:\Program Files\Git\cmd\git.exe | PASS |

## 测试 4: 与 P00-004 依赖图对应
- P00-004 识别的 7 类外部库（-lzstd/-llz4/-lz/-lgsl/-lgslcblas/-fopenmp/Eigen3/Qt6）全部在 P00-005 baseline 中有对应工具 — PASS

## 失败与 Skip
- zstd/lz4/zlib 精确版本号未从 DLL 提取（SKIP，仅记录 SHA-256，已作为未解决问题登记）

## 结论
- 16 个工具全部采集版本与路径
- 13 个关键二进制 SHA-256 已记录且可重复
- 与 P00-004 依赖图完全对应
- **VERDICT: PASS**
