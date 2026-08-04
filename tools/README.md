# AstroCS 工程工具集（astro_toolkit）

Python 脚本 + JSON 配置驱动的批量操作工具，用于减少子 Agent 频繁触发沙箱确认。

## 快速开始

```powershell
# 打印示例配置
python tools/astro_toolkit.py --example

# 执行配置文件
python tools/astro_toolkit.py tools/my_task.json --log tools/my_task.log
```

## 配置文件格式

JSON 数组，每个元素为一个 step：

```json
[
  { "type": "git_status",  "params": {"repo": "."} },
  { "type": "git_add",     "params": {"repo": ".", "files": ["a.md"]} },
  { "type": "git_commit",  "params": {"repo": ".", "message_file": "COMMIT.txt"} },
  { "type": "git_push",    "params": {"repo": ".", "branch": "main", "timeout_sec": 180} },
  { "type": "git_log",     "params": {"repo": ".", "count": 5} },
  { "type": "run_orchestrator", "params": {
      "exe": "build/artifacts/orchestrator.exe",
      "args": ["stage1","--frame","x.fits","--output","y.hiss","--config","c.json"],
      "timeout_sec": 120,
      "stdout_file": "logs/out.jsonl",
      "stderr_file": "logs/err.log"
  }},
  { "type": "sha256",      "params": {"path": "out.hiss"} },
  { "type": "mkdir",       "params": {"path": "evidence/P06-002/checks"} },
  { "type": "write_file",  "params": {"path": "x.txt", "content": "hello"} },
  { "type": "copy_file",   "params": {"src": "a.bin", "dst": "b.bin"} },
  { "type": "delete_file", "params": {"paths": ["tmp1.txt"]} },
  { "type": "list_dir",    "params": {"path": "evidence", "pattern": "*.md"} }
]
```

可选字段：`"stop_on_error": true` 使任一步失败即停止后续。

## 输出格式（stdout JSON）

```json
{
  "ok": true,
  "results": [
    {"step": 0, "type": "git_status", "ok": true, "exit_code": 0,
     "stdout": "...", "stderr": "...", "elapsed_sec": 0.06, "extra": {...}}
  ]
}
```

## 支持的 step 类型

| type | 说明 | 关键参数 |
|---|---|---|
| `git_status` | 查看工作树状态 | `repo` |
| `git_add` | 暂存文件 | `repo`, `files[]` 或 `all: true` |
| `git_commit` | 用 `-F` 从文件读取 message 提交 | `repo`, `message_file` |
| `git_push` | 推送远端 | `repo`, `remote`, `branch`, `timeout_sec` |
| `git_log` | 查看最近提交 | `repo`, `count` |
| `run_orchestrator` | 运行 orchestrator.exe | `exe`, `args[]`, `timeout_sec`, `stdout_file`, `stderr_file` |
| `sha256` | 计算文件 SHA-256 | `path` |
| `mkdir` | 递归创建目录 | `path` |
| `write_file` | 写文本文件 | `path`, `content`, `encoding` |
| `copy_file` | 复制文件 | `src`, `dst` |
| `delete_file` | 删除多个文件 | `paths[]` |
| `list_dir` | 列出目录文件 | `path`, `pattern` |

## 设计要点

- **一次调用完成多步操作**：把 git add → commit → push → log 打包成一个 JSON，只需一次 RunCommand。
- **DLL 路径自动注入**：`run_orchestrator` 自动将 `build/artifacts` 和 `C:\msys64\mingw64\bin` 加入 PATH。
- **超时保护**：每个进程都有 `timeout_sec`，避免阻塞。
- **结果结构化**：JSON 输出便于 Agent 解析；stdout/stderr 同时落盘到指定文件。
- **退出码**：全部成功 → 0；任一失败 → 1（不中断后续，除非 `stop_on_error`）。

## 与 vq-commit.ps1 的关系

`astro_toolkit` 的 `git_commit` 步骤与 `vq-commit.ps1` 等价（都用 `-F` 读取 message 文件），但可与其他步骤打包执行。两者可并存。
