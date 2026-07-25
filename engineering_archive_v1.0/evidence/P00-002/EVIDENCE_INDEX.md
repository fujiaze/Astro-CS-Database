# EVIDENCE_INDEX: P00-002

## 证据目录
`engineering/evidence/P00-002/`

## 证据清单
| 文件 | 说明 |
|---|---|
| SOURCE_RECORD.md | 来源记录（远端/commit/文件清单/清理清单） |
| TASK_REPORT.md | 任务执行报告 |
| TEST_REPORT.md | 验证测试报告 |
| EVIDENCE_INDEX.md | 本文件 |

## 关键事实证据

### F-001: 来源仓库可追溯
- 远端: https://github.com/fujiaze/Healpix-Drizzle-Cpp.git
- Commit: ecf8758affaf0caf0eb12faed7d2ed32623886e7
- 日期: 2026-07-16T12:27:28+08:00

### F-002: 源码未被修改
- 删除 .git 前后 SHA-256 对比：3 个抽查全部 MATCH
- drizzle_engine.cpp / hp_drizzle_api.h / wcs_sip.cpp

### F-003: 编译产物未入库
- healpix_drizzle.dll (1.27MB) 已删除
- _test_compile.exe (504KB) 已删除
- __pycache__/ (6 个 .pyc) 已删除
- .pytest_cache/ 已删除

### F-004: 18 个文件纳入主仓库
- 16 个源码/文档/配置文件 + 2 个测试文件
- git diff --cached 确认 18 个文件已暂存

### F-005: .gitignore 规则已更新
- lib/healpix_db/.gitignore 移除 healpix_drizzle/ 和 healpix_stack/ 忽略规则
- healpix_stack/ 将在 P00-003 中处理
