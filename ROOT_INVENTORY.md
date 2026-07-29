# 根目录清单与归档状态

> 更新时间: 2026-07-29
> 用途: 明确根目录每个目录/文件的用途、状态和归档建议

## 活跃目录（当前开发使用）

| 路径 | 用途 | 状态 |
|------|------|------|
| `lib/` | 源代码（C++ DLL + Python 工具） | 活跃 |
| `testdata/` | 测试数据（710 帧 FITS） | 活跃 |
| `GaiaDR3SP/` | Gaia DR3 星表数据库 | 活跃 |
| `output/` | 运行时输出（.hiss/.hcsd） | 活跃 |
| `tools/` | 项目工具集（astro_toolkit.py） | 活跃 |
| `engineering_v1.3/` | 当前工程包（P09-P17 任务） | 活跃 |
| `.gitignore` / `.gitattributes` | Git 配置 | 活跃 |
| `README.md` | 项目说明（需更新到 v1.3） | 需更新 |
| `memory.md` | 项目记忆（开发日志） | 活跃 |

## 历史归档目录（可安全归档/删除）

| 路径 | 用途 | 状态 | 建议 |
|------|------|------|------|
| `engineering/` | v1.0 工程包（G0-G8 已完成） | 归档 | 保留参考 |
| `engineering_archive_v1.0/` | v1.0 归档副本 | 归档 | 可删除（与 engineering/ 重复） |
| `engineering_v1.2/` | v1.2 工程包（P09-P17 任务定义） | 归档 | 保留参考（v1.3 继承任务定义） |
| `docs/` | v1.0 文档 | 归档 | 可删除（已被 engineering_v1.3/docs/ 替代） |
| `audit/` | v1.1 审计包 | 归档 | 可删除 |
| `dist/` | v1.0 分发包 | 归档 | 可删除（已过时） |

## 临时文件（可清理）

| 路径 | 用途 | 建议 |
|------|------|------|
| `COMMIT_MSG.txt` | vq-commit 临时消息文件 | 清理 |
| `_commit_msg_p02.txt` | P02 时期临时文件 | 清理 |
| `_toolkit_configs/` | 旧工具配置 | 清理 |
| `bootstrap.ps1` | v1.0 引导脚本 | 保留参考 |
| `build.ps1` | v1.0 构建脚本 | 保留参考 |

## 根目录清理建议

```powershell
# 可选清理（需用户确认）
# Remove-Item "COMMIT_MSG.txt" -Force
# Remove-Item "_commit_msg_p02.txt" -Force
# Remove-Item "_toolkit_configs" -Recurse -Force
# Remove-Item "audit" -Recurse -Force
# Remove-Item "dist" -Recurse -Force
```

**注意**: 历史目录（engineering/, engineering_archive_v1.0/, engineering_v1.2/）保留作为参考，不删除。活跃开发只在 `engineering_v1.3/` 进行。
