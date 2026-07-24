# EVIDENCE_INDEX: P00-005

## 证据目录
`engineering/evidence/P00-005/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| environment_baseline.json | 42e5d2fd1d03748e5170e22555c9963aca849421a069de70658fc8eadc2306fb | 机器可读工具链基线（16 工具） |
| environment_baseline.md | 659467c0de4ca7dbf8876a8fd24b1da04ea65f3c6695dd18d4b15ec3ddcdd753 | 人类可读工具链基线（清单+哈希+路径问题） |
| TASK_REPORT.md | 30cccb329a21a8ef9d255061adf839ece7f8dd002e5f2d5b89252ce741f352ac | 任务执行报告 |
| TEST_REPORT.md | 7bd2c8df5cf3374db6d397d6fd1d34d02b71215888e8215daea015b6bba2369d | 可重复性测试报告 |
| EVIDENCE_INDEX.md | — | 本文件 |
| REVIEW_REPORT.md | — | 独立复核报告 |

## 关键事实证据

### F-001: 16 个工具全部采集
- 证据: environment_baseline.json `toolchain` 数组长度 16
- 涵盖: PowerShell/Python/Git/gh/GCC/G++/mingw32-make/Make(TRAE)/Qt6/GSL/GSL CBLAS/zstd/lz4/zlib/OpenMP/Eigen3

### F-002: 13 个关键二进制 SHA-256 已记录
- 证据: environment_baseline.md "关键二进制 SHA-256" 表 13 行
- 抽查: gcc.exe 哈希重新计算 = 9909A5E8... 与记录一致

### F-003: 与 P00-004 依赖图完全对应
- 证据: environment_baseline.md "与 P00-004 依赖图的对应" 表
- P00-004 识别的 7 类外部库（-lzstd/-llz4/-lz/-lgsl/-lgslcblas/-fopenmp/Eigen3/Qt6）全部有对应工具

### F-004: 3 个路径问题已登记
- 证据: environment_baseline.json `path_issues` 数组长度 3
- 涵盖: GCC 不在 PATH、两个 make 并存、qmake6 不在 PATH

### F-005: 许可证齐全
- 证据: environment_baseline.json 每个工具均有 `license` 字段
- 涵盖: MIT/PSF/GPLv2/GPLv3/BSD/zlib/MPL-2.0 等

## 命令日志
- 版本采集: `$PSVersionTable`、`python --version`、`git --version`、`gh --version`、`gcc --version`、`qmake6 --version`
- 哈希采集: `Get-FileHash <binary> -Algorithm SHA256`
- Eigen3 版本: `Select-String -Path Eigen3ConfigVersion.cmake -Pattern PACKAGE_VERSION`
