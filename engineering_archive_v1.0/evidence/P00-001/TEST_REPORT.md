# TEST_REPORT: P00-001 基线预检可重复性验证

## 测试目标
验证 `repo_preflight.py` 生成的预检报告可在同一仓库上重复生成，结果一致。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: eb44f65
- **分支**: main
- **Python**: 系统默认
- **工具**: engineering/tools/repo_preflight.py（内置 30s 超时）

## 测试 1: 首次运行
- **命令**: `python engineering/tools/repo_preflight.py --repo . --output engineering/evidence/P00-001`
- **退出码**: 0
- **超时**: 否
- **产物**:
  - preflight.json (SHA-256: 012958bdb78fefa866eab1c371c5c649e07d6763efa9d3d2487165d917b34432)
  - preflight.md (SHA-256: 730c17c087b3896ea85b2b6d9a07c094740abd14c8125b5face1db1758270928)
  - artifacts.sha256

## 测试 2: 重复运行（覆盖同一输出目录）
- **命令**: 同上
- **退出码**: 0
- **超时**: 否
- **产物**: 覆盖首次产物，SHA-256 不变

## 测试 3: 关键字段验证
| 字段 | 预期 | 实际 | 结果 |
|---|---|---|---|
| 11 个已跟踪模块路径 | 全部存在 | 全部 [x] | PASS |
| healpix_drizzle 路径 | 本地存在 | [x] | PASS（但 git 未跟踪） |
| healpix_stack 路径 | 本地存在 | [x] | PASS（但 git 未跟踪） |
| HEAD commit | eb44f65 | eb44f65 | PASS |
| 分支 | main | main | PASS |
| 远端 | origin Astro-CS-Database | origin Astro-CS-Database | PASS |
| Tags | 0 | 0 | PASS |
| 根 CMake | False | False | PASS |
| 跟踪文件数 | ~350 | 350 | PASS |
| 命令超时设置 | 30s | 30s | PASS |

## 测试 4: Git 跟踪验证（额外检查）
- `git ls-files lib/healpix_db/healpix_drizzle` → 0 文件（未跟踪）
- `git ls-files lib/healpix_db/healpix_stack` → 0 文件（未跟踪）
- 11 个主模块 `git ls-files lib/<module>` → 各有跟踪文件（311 合计）

## 结论
- 预检工具可重复运行，结果一致
- 所有命令在 30s 超时内完成
- 关键事实已记录，无猜测
- **VERDICT: PASS**
