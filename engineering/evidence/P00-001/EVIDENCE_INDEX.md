# EVIDENCE_INDEX: P00-001

## 证据目录
`engineering/evidence/P00-001/`

## 证据清单

| 文件 | SHA-256 | 说明 |
|---|---|---|
| preflight.json | 012958bdb78fefa866eab1c371c5c649e07d6763efa9d3d2487165d917b34432 | 机器可读预检报告（Git 状态、路径检查、构建/测试/CI 文件清单） |
| preflight.md | 730c17c087b3896ea85b2b6d9a07c094740abd14c8125b5face1db1758270928 | 人类可读预检报告摘要 |
| artifacts.sha256 | — | 上述文件的 SHA-256 校验和 |
| TASK_REPORT.md | — | 任务执行报告（发现、状态、变更清单） |
| TEST_REPORT.md | — | 可重复性测试报告 |
| EVIDENCE_INDEX.md | — | 本文件 |

## 关键事实证据

### F-001: 11 个主模块全部受控
- 证据: preflight.json `expected_paths` 中 11 个 `lib/<module>` 路径全部为 true
- git ls-files 确认每个模块有跟踪文件（合计 311）

### F-002: healpix_drizzle/healpix_stack 本地存在但未受控
- 证据: preflight.json `expected_paths` 显示路径存在（true）
- 反证: `git ls-files lib/healpix_db/healpix_drizzle` 返回 0 文件
- 反证: `git ls-files lib/healpix_db/healpix_stack` 返回 0 文件
- 原因: 两目录含独立 .git，Git 拒绝跟踪嵌套仓库内部文件

### F-003: 无根级构建入口
- 证据: preflight.json `root_cmake` = false
- 证据: preflight.json `root_requirements` = []

### F-004: 无主仓库 CI
- 证据: preflight.json `ci_files` 中 9 个文件全部位于 siril-1.4.3/ 或 third_party/ 下

### F-005: 无 baseline tag
- 证据: preflight.json `commands.tags.stdout` = ""（空）

### F-006: HEAD 与基线 commit 差异
- 控制包基线: 9f10c72
- 当前 HEAD: eb44f65
- 差异: 1 个文档提交（memory.md 封存记录），不影响代码基线

## 命令日志
所有命令通过 repo_preflight.py 执行，内置 30s 超时，退出码 0，无超时。
