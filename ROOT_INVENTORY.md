# 根目录清单

> 更新时间: 2026-07-29
> 用途: 根目录结构索引，供 AI / 审计快速定位

## 目录结构

| 路径 | 大小 | 用途 | Git 追踪 |
|------|------|------|----------|
| `lib/` | 3.2 GB | 源代码（C++ DLL + Python 工具） | 是 (372 文件) |
| `工程控制/` | 26 MB | 工程控制包（P09-P17 任务文档/证据/控制文件） | 是 (552 文件) |
| `tools/` | 0.2 MB | 项目工具集（astro_toolkit.py） | 是 (24 文件) |
| `testdata/` | 71.7 GB | 测试数据（710 帧 FITS） | 否 (.gitignore) |
| `GaiaDR3SP/` | 64.7 GB | Gaia DR3 星表数据库 | 否 (.gitignore) |
| `GaiaDR3/` | 41.9 GB | Gaia DR3 数据（旧版） | 否 (.gitignore) |
| `siril-1.4.3/` | 55.2 MB | Siril 参考源码 | 否 (.gitignore) |
| `output/` | 500.8 MB | 运行时输出（.hiss/.hcsd） | 否 (.gitignore) |
| `.trae/` | 3.7 MB | IDE 配置 | 否 (.gitignore) |

## 根目录文件

| 文件 | 用途 |
|------|------|
| `.gitignore` / `.gitattributes` | Git 配置 |
| `README.md` | 项目说明 v1.3 |
| `memory.md` | 开发记忆（需求/进度/重大问题日志） |
| `ROOT_INVENTORY.md` | 本文件（目录索引） |

## 工程控制包结构 (`工程控制/`)

```
工程控制/
├── AUTONOMOUS_ENTRY.md     # 自治执行入口
├── AUDIT_PACK.md           # 审计入口主文档
├── control/                # 控制文件
│   ├── PROJECT_STATE.yaml  # 项目状态 (G12)
│   ├── MASTER_TASK_REGISTER.csv  # 50 任务注册表
│   └── CURRENT_TASK.md     # 当前任务
├── contracts/              # 契约文档 (FROZEN)
├── docs/                   # 设计与规格文档
├── evidence/               # 任务证据 (P09-P13)
├── tasks/                  # 任务定义
└── tools/                  # 工程工具
```
