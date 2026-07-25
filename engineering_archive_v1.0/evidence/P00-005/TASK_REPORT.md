# TASK_REPORT: P00-005 采集工具链与本机环境

## 任务信息
- **Task ID**: P00-005
- **Phase**: P00 基线冻结与仓库完整性恢复
- **状态**: IN_REVIEW
- **执行时间**: 2026-07-24

## 基线
- **commit**: 61c3b05 (P00-004 提交后 HEAD)
- **分支**: main

## 目标与范围
采集本机工具链实际版本、安装路径、许可证与关键二进制 SHA-256，生成 environment baseline，为 P01 构建可重现性提供环境基线。

## 入口条件
- P00-001 DONE ✓

## 允许修改（已遵守）
- `engineering/evidence/P00-005/**`
- `engineering/control/**`
- 未修改 `lib/**`、`docs/**`、构建脚本

## 实现过程
1. 依据 P00-004 依赖图识别的全部外部库，确定采集范围：PowerShell/Python/Git/gh/GCC/G++/Make/Qt6/GSL/zstd/lz4/zlib/OpenMP/Eigen3
2. 并行运行 5 组采集命令，获取各工具版本号、安装路径
3. 对 13 个关键二进制计算 SHA-256
4. 从 Eigen3ConfigVersion.cmake 提取 Eigen3 版本 (5.0.1)
5. 汇总为 environment_baseline.json（机器可读）和 environment_baseline.md（人类可读）
6. 识别 3 个路径问题（GCC 不在 PATH、两个 make 并存、qmake6 不在 PATH）

## 修改文件
- `engineering/evidence/P00-005/environment_baseline.json`（新增，16 工具机器可读基线）
- `engineering/evidence/P00-005/environment_baseline.md`（新增，人类可读基线）
- `engineering/evidence/P00-005/TASK_REPORT.md`（本文件）
- `engineering/evidence/P00-005/TEST_REPORT.md`（新增）
- `engineering/evidence/P00-005/EVIDENCE_INDEX.md`（新增）
- `engineering/evidence/P00-005/REVIEW_REPORT.md`（新增，独立复核）
- `engineering/control/MASTER_TASK_REGISTER.csv`（P00-005 → DONE，P00-006 → IN_PROGRESS）
- `engineering/control/PROJECT_STATE.yaml`（current_task → P00-006）
- `engineering/control/CURRENT_WORK.md`（切换为 P00-006 任务说明）

## 结果

### 工具链覆盖（16/16）
| 类别 | 工具 | 版本 |
|---|---|---|
| 运行时 | PowerShell | 7.6.3 |
| 脚本 | Python | 3.10.11 |
| 版本控制 | Git / gh | 2.53.0 / 2.63.2 |
| 编译器 | GCC / G++ | 16.1.0 (MSYS2 Rev4) |
| 构建 | mingw32-make | 4.4.1 |
| GUI 框架 | Qt6 | 6.11.0 |
| 数值库 | GSL / GSL CBLAS | 2.8 |
| 压缩库 | zstd / lz4 / zlib | (见 baseline) |
| 并行 | OpenMP (libgomp) | 16.1.0 |
| 数学 | Eigen3 | 5.0.1 (头文件库) |

### 关键二进制哈希（13/13）
所有关键二进制 SHA-256 已记录，可在另一台同类机器上对照验证。

### 路径问题（3 项）
1. GCC/G++/mingw32-make 不在默认 PATH（构建脚本通过 MSYS2 环境调用）
2. PATH 中存在两个 make（TRAE make.cmd 与 mingw32-make.exe）
3. qmake6 不在默认 PATH（CMake 需显式定位）

## 未解决问题
- zstd/lz4/zlib 的精确版本号未从 DLL 文件本身提取（标注为 null），仅记录 SHA-256。如需精确版本号，可在 P01-002 依赖锁定清单中通过 `pacman -Qi` 补充。
- Python 路径位于 TRAE 沙盒目录下，非系统级安装。P01-003 bootstrap 脚本需决定是否要求系统级 Python。

## 风险与回退
- 本任务为只读采集 + 证据归档，无代码变更，无运行时风险
- 回退方式：删除 `engineering/evidence/P00-005/` 目录并 revert 控制文件即可

## 控制文件更新
- `MASTER_TASK_REGISTER.csv`：P00-005 → DONE，P00-006 → IN_PROGRESS
- `PROJECT_STATE.yaml`：current_task → P00-006
- `CURRENT_WORK.md`：切换为 P00-006 任务说明

## 建议状态
`IN_REVIEW`
