# AstroCS 工程工具集（astro_toolkit）

Python 脚本 + JSON 配置驱动的批量操作工具，用于减少子 Agent 频繁触发沙箱确认。

## 快速开始

```powershell
# 打印示例配置
python eng/tools/astro_toolkit.py --example

# 执行配置文件
python eng/tools/astro_toolkit.py eng/tools/my_task.json --log eng/tools/my_task.log
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

---

## 附：根目录 `scripts/` 的退役（2026-09-21 ROOT-CONSOLIDATION）

**依据**：负责人 2026-09-21 直接指令（根目录整合）；`ENGINEERING_SPEC.md §7`「新产物落位到对应目录，
不散落根目录」与 §9「仓库只保留最新生产代码、自解释文档集、合同与测试」。

`scripts/` 曾是 §7「其他固定目录」之一。ROOT-008 之后该目录**已无可执行脚本**，
只剩一份 `README.md`（退役登记）。本次整合把该登记并入本文件，**删除根条目 `scripts/`**，
并同步 §7 与 `eng/ci/root_manifest.json`。

### 已退役脚本（ROOT-008，2026-09-16；登记逐字保留）

| 旧路径 | 世代 | 能力去向 / 退役依据 |
|---|---|---|
| `scripts/package_audit.py` | REL-003 审核包线 | 白名单打包器（源码快照 + L0-L2 文档 + 证据 + SHA 清单 tar.gz）。其 WHITELIST 指向 `docs/refactor`、`evidence/refactor`、`lib/phase1`、`lib/core`、`lib/io`、`cli` 等**本世代已不存在的路径**，已无打包对象；同职能工具 `eng/tools/pack_audit_package.py` / `eng/tools/assemble_audit.py` / `eng/tools/make_capsule.py` 已按 RETIRE-001 退役（`assemble_audit.py` 打印 `ASSEMBLE_AUDIT_RETIRED` 并 exit 2）。 |
| `scripts/validate_audit.py` | REL-003 审核包线 | 审核包校验器（解包重验 SHA + 禁止项）。与上面的打包器成对，无包可验；能力由 `eng/packaging/verify_install_tree.py`（安装树校验，在役）承接安装面校验。 |

两者在 `eng/ci/checks.json` 的注册 step 面引用数为 0（V21-N-07 复核），不参与任何构建/测试/CI 门。
ROOT-008 任务卡判定为「与已退役的审核包线重复 ⇒ 退役（删除）」。

### 恢复方式（走 git 历史，不做副本）

```bash
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/package_audit.py
git show e6d65dcdf9727899d508b46c0c33d200e835088f:scripts/validate_audit.py
# 退役登记原文：git show <ROOT-CONSOLIDATION 前一提交>:scripts/README.md
```

（`e6d65dcd` = ROOT-008 执行前的 main；两条路径的最后修改提交为 `b840ed64`。）

