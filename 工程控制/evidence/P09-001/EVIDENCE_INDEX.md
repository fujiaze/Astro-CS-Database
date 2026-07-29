# P09-001 证据索引

- **任务 ID**: P09-001
- **执行日期**: 2026-07-27
- **执行人**: 长期工程 Agent（GLM-5.2）

## 证据清单

### 1. 报告

| 文件 | 描述 | SHA-256 |
|---|---|---|
| `TASK_REPORT.md` | 任务执行报告，含 v1.1 仓库事实快照 + 6 项过度结论修正 | (生成时未算) |
| `TEST_REPORT.md` | 测试报告，12 项测试矩阵全部 PASS | (生成时未算) |
| `EVIDENCE_INDEX.md` | 本文件 | — |
| `REVIEW_REPORT.md` | 独立复核报告 | (生成时未算) |

### 2. 原始证据

| 文件 | 来源 | 内容 | 大小 |
|---|---|---|---|
| `git_log_oneline.txt` | `git log --oneline -n 10` | 最近 10 次提交（ed145a7 ~ 4ccb507） | 10 行 |
| `git_log_oneline.err` | 同上 stderr | 空 | 0 B |
| `git_porcelain.txt` | `git status --porcelain=v1` | 工作树状态（含 v1.2 新文件） | 5 行 |
| `git_porcelain.err` | 同上 stderr | 空 | 0 B |
| `untracked_engineering.txt` | `git ls-files --others --exclude-standard engineering/` | 空（engineering/ 下无未跟踪） | 0 B |
| `untracked_engineering.err` | 同上 stderr | 空 | 0 B |
| `git_status_result.json` | astro_toolkit 输出 | git_status step 结构化结果 | — |

### 3. 工具集扩展证据

| 文件 | 来源 | 内容 |
|---|---|---|
| `tools/astro_toolkit.py`（v1.2 扩展） | 本任务新增 5 个步骤处理器 | unzip / move_dir / run_python / file_exists / rmtree |
| `_toolkit_configs/install_v12.json` | v1.2 开发包安装配置 | 4 步：mkdir + unzip + file_exists + run_python |
| `_toolkit_configs/install_v12.log` | 上述配置运行结果 | 4/4 step PASS，validate_pack 输出 `tasks=50 prompt_chars=62` |
| `_toolkit_configs/p09_001_baseline.json` | 本任务基线收集配置 | 10 步：git_status + 4×run_orchestrator + 2×list_dir + 3×sha256 |
| `_toolkit_configs/p09_001_baseline.log` | 上述配置运行结果 | 9/10 step PASS（1 步 git rev-parse 参数顺序错误，已记录） |

### 4. v1.1 工程基线锁定

| 文件 | SHA-256 | 大小 |
|---|---|---|
| `engineering/control/PROJECT_STATE.yaml` | `55C6F9120B27BF83965B163D2F09E3502C49F269E06F9579EED3CF91395D0951` | 8869 B |
| `engineering/control/MASTER_TASK_REGISTER.csv` | `09E476835F216CE873FCCD0D904B40D43FB0DE81092AC8077B289137FC4AC6FD` | 2627 B |
| `engineering/control/CURRENT_TASK.md` | `CF87C7CD5AA60E06270C232581A3A94A13DF31633DD82ACBD5B0F657F878D7E8` | 3813 B |

### 5. v1.2 开发包完整性

| 项 | 值 |
|---|---|
| zip 文件 | `AstroCS_Next_Phase_Development_Pack_2026-07-27_v1.2.zip` |
| 解压目标 | `engineering_v1.2/` |
| 解压文件数 | 121 |
| `validate_pack.py` 输出 | `OK root=...engineering_v1.2 tasks=50 prompt_chars=62` |
| 任务数 | 50（P09-001 ~ P17-003） |
| Gate 数 | 8（G9 ~ G16） |

### 6. v1.1 过度结论修正记录

本任务在 `TASK_REPORT.md §4` 中显式记录 6 项 v1.1 过度结论的修正：

1. 测光链未完成（P05-002 + P06-002 引用过度）
2. SNR² 加权仅在合成数据上证明（P06-002 G-002 部分解决）
3. 梯度校正未在真实非零梯度数据上验证（P06-002 残留风险）
4. 三面板马赛克未在真实数据上验证（P06-003 引用过度）
5. 浏览器性能无 Gate（v1.1 引用过度）
6. PlateSolve 共享检测导出（保留，仅做命名修正）

后续任务 P09-002 ~ P17-003 不得引用上述结论为"已通过"。
