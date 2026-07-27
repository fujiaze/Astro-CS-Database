#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AstroCS 工程工具集 - Python 脚本 + JSON 配置驱动的批量操作工具

设计目标：
  - 将"git 提交/推送 / 运行 orchestrator / 算 hash / 建目录 / 写文件"等常用操作
    封装为一条命令，由 JSON 配置文件编排，避免子 Agent 频繁触发沙箱确认。
  - 每次调用只产生一次 RunCommand，子 Agent 可在其中批量执行多步操作。
  - 所有结果以 JSON 写到 stdout，便于 Agent 解析；详细日志写到文件。

用法：
  python tools/astro_toolkit.py <config.json> [--log <logfile>]
  python tools/astro_toolkit.py --example            # 打印示例配置

配置文件结构（JSON 数组，每个元素为一个 step）：
  [
    { "type": "git_status",   "params": { "repo": "." } },
    { "type": "git_add",      "params": { "repo": ".", "files": ["a.md","b.md"] } },
    { "type": "git_commit",   "params": { "repo": ".", "message_file": "COMMIT.txt" } },
    { "type": "git_push",     "params": { "repo": ".", "remote": "origin", "branch": "main" } },
    { "type": "git_log",     "params": { "repo": ".", "count": 5 } },
    { "type": "run_orchestrator",
      "params": {
          "exe": "build/artifacts/orchestrator.exe",
          "args": ["stage1","--frame","x.fits","--output","y.hiss","--config","c.json"],
          "timeout_sec": 120,
          "stdout_file": "out.jsonl",
          "stderr_file": "err.log"
      } },
    { "type": "sha256",      "params": { "path": "output.hiss" } },
    { "type": "mkdir",       "params": { "path": "evidence/P06-002/checks" } },
    { "type": "write_file",  "params": { "path": "x.txt", "content": "hello", "encoding": "utf-8" } },
    { "type": "copy_file",   "params": { "src": "a.bin", "dst": "b.bin" } },
    { "type": "delete_file", "params": { "paths": ["tmp1.txt","tmp2.txt"] } },
    { "type": "list_dir",    "params": { "path": ".", "pattern": "*.md" } }
  ]

每步结果（输出到 stdout 的 JSON 数组元素）：
  { "step": 0, "type": "git_status", "ok": true, "exit_code": 0,
    "stdout": "...", "stderr": "...", "extra": {...} }

约定：
  - 任一步失败默认不中断（继续执行后续步），最终汇总 exit_code：
      全部成功 → 0；任一失败 → 1
  - 若需"失败即停"，在 step 上加 "stop_on_error": true。
  - 所有文件路径相对当前工作目录（可用 --cwd 切换）。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


# ---------------------------------------------------------------------------
# 通用工具
# ---------------------------------------------------------------------------

def _run(cmd: list[str], cwd: str, timeout: int | None,
         stdout_file: str | None, stderr_file: str | None) -> dict[str, Any]:
    """执行子进程并返回结果字典。"""
    t0 = time.time()
    try:
        proc = subprocess.run(
            cmd, cwd=cwd, capture_output=True, text=True,
            timeout=timeout, encoding="utf-8", errors="replace",
        )
        rc = proc.returncode
        out = proc.stdout or ""
        err = proc.stderr or ""
    except subprocess.TimeoutExpired as e:
        rc = -1
        out = e.stdout or "" if isinstance(e.stdout, str) else ""
        err = (e.stderr or "" if isinstance(e.stderr, str) else "") + \
              f"\n[astro_toolkit] TIMEOUT after {timeout}s"
    except FileNotFoundError as e:
        rc = -2
        out = ""
        err = f"[astro_toolkit] FILE_NOT_FOUND: {e}"
    elapsed = round(time.time() - t0, 3)

    # 落盘 stdout/stderr
    if stdout_file:
        Path(stdout_file).parent.mkdir(parents=True, exist_ok=True)
        Path(stdout_file).write_text(out, encoding="utf-8")
    if stderr_file:
        Path(stderr_file).parent.mkdir(parents=True, exist_ok=True)
        Path(stderr_file).write_text(err, encoding="utf-8")

    return {
        "ok": rc == 0,
        "exit_code": rc,
        "stdout": out,
        "stderr": err,
        "elapsed_sec": elapsed,
        "cmd": cmd,
    }


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


# ---------------------------------------------------------------------------
# 步骤处理器
# ---------------------------------------------------------------------------

def step_git_status(params: dict, ctx: dict) -> dict:
    repo = params.get("repo", ".")
    r = _run(["git", "status", "--porcelain=v1", "-b"], repo, 30, None, None)
    r["extra"] = {"porcelain": r["stdout"]}
    return r


def step_git_add(params: dict, ctx: dict) -> dict:
    repo = params.get("repo", ".")
    files = params.get("files", [])
    all_flag = params.get("all", False)
    if all_flag:
        cmd = ["git", "add", "-A"]
    else:
        cmd = ["git", "add"] + list(files)
    return _run(cmd, repo, 60, None, None)


def step_git_commit(params: dict, ctx: dict) -> dict:
    repo = params.get("repo", ".")
    msg_file = params["message_file"]
    if not os.path.isabs(msg_file):
        msg_file = os.path.join(repo, msg_file)
    if not os.path.exists(msg_file):
        return {"ok": False, "exit_code": -3, "stdout": "",
                "stderr": f"message file not found: {msg_file}",
                "elapsed_sec": 0, "cmd": ["git", "commit", "-F", msg_file]}
    return _run(["git", "commit", "-F", msg_file], repo, 60, None, None)


def step_git_push(params: dict, ctx: dict) -> dict:
    repo = params.get("repo", ".")
    remote = params.get("remote", "origin")
    branch = params.get("branch", "")
    cmd = ["git", "push", remote]
    if branch:
        cmd.append(branch)
    return _run(cmd, repo, params.get("timeout_sec", 180), None, None)


def step_git_log(params: dict, ctx: dict) -> dict:
    repo = params.get("repo", ".")
    count = params.get("count", 5)
    return _run(["git", "log", "--oneline", "-n", str(count)], repo, 30, None, None)


def step_run_orchestrator(params: dict, ctx: dict) -> dict:
    exe = params["exe"]
    if not os.path.isabs(exe):
        exe = os.path.join(ctx.get("cwd", "."), exe)
    args = [exe] + list(params.get("args", []))
    cwd = ctx.get("cwd", ".")
    # 注入 DLL 路径（Windows）
    env = os.environ.copy()
    arts = os.path.join(cwd, "build", "artifacts")
    env["PATH"] = arts + os.pathsep + r"C:\msys64\mingw64\bin" + os.pathsep + env.get("PATH", "")
    t0 = time.time()
    try:
        proc = subprocess.run(args, cwd=cwd, capture_output=True, text=True,
                              timeout=params.get("timeout_sec", 120),
                              encoding="utf-8", errors="replace", env=env)
        rc, out, err = proc.returncode, proc.stdout or "", proc.stderr or ""
    except subprocess.TimeoutExpired as e:
        rc, out, err = -1, "", f"TIMEOUT after {params.get('timeout_sec',120)}s"
    except FileNotFoundError as e:
        rc, out, err = -2, "", f"FILE_NOT_FOUND: {e}"
    elapsed = round(time.time() - t0, 3)
    if params.get("stdout_file"):
        p = Path(params["stdout_file"]); p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(out, encoding="utf-8")
    if params.get("stderr_file"):
        p = Path(params["stderr_file"]); p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(err, encoding="utf-8")
    return {"ok": rc == 0, "exit_code": rc, "stdout": out, "stderr": err,
            "elapsed_sec": elapsed, "cmd": args}


def step_sha256(params: dict, ctx: dict) -> dict:
    path = params["path"]
    if not os.path.isabs(path):
        path = os.path.join(ctx.get("cwd", "."), path)
    if not os.path.exists(path):
        return {"ok": False, "exit_code": -4, "stdout": "", "stderr": f"not found: {path}",
                "elapsed_sec": 0, "cmd": [path], "extra": {}}
    h = sha256_file(path)
    return {"ok": True, "exit_code": 0, "stdout": h, "stderr": "",
            "elapsed_sec": 0, "cmd": [path], "extra": {"sha256": h, "size": os.path.getsize(path)}}


def step_mkdir(params: dict, ctx: dict) -> dict:
    path = params["path"]
    if not os.path.isabs(path):
        path = os.path.join(ctx.get("cwd", "."), path)
    Path(path).mkdir(parents=True, exist_ok=True)
    return {"ok": True, "exit_code": 0, "stdout": path, "stderr": "",
            "elapsed_sec": 0, "cmd": [], "extra": {"path": path}}


def step_write_file(params: dict, ctx: dict) -> dict:
    path = params["path"]
    content = params.get("content", "")
    encoding = params.get("encoding", "utf-8")
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(content, encoding=encoding)
    return {"ok": True, "exit_code": 0, "stdout": path, "stderr": "",
            "elapsed_sec": 0, "cmd": [], "extra": {"path": path, "bytes": len(content.encode(encoding))}}


def step_copy_file(params: dict, ctx: dict) -> dict:
    src, dst = params["src"], params["dst"]
    Path(dst).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return {"ok": True, "exit_code": 0, "stdout": dst, "stderr": "",
            "elapsed_sec": 0, "cmd": [src, dst], "extra": {"src": src, "dst": dst}}


def step_delete_file(params: dict, ctx: dict) -> dict:
    paths = params.get("paths", [])
    deleted = []
    for p in paths:
        try:
            os.remove(p)
            deleted.append(p)
        except FileNotFoundError:
            pass
    return {"ok": True, "exit_code": 0, "stdout": json.dumps(deleted), "stderr": "",
            "elapsed_sec": 0, "cmd": [], "extra": {"deleted": deleted}}


def step_list_dir(params: dict, ctx: dict) -> dict:
    path = params.get("path", ".")
    pattern = params.get("pattern", "*")
    base = Path(path)
    if not base.is_absolute():
        base = Path(ctx.get("cwd", ".")) / base
    items = sorted([str(p.relative_to(base)) for p in base.glob(pattern) if p.is_file()])
    return {"ok": True, "exit_code": 0, "stdout": json.dumps(items), "stderr": "",
            "elapsed_sec": 0, "cmd": [], "extra": {"items": items}}


HANDLERS = {
    "git_status": step_git_status,
    "git_add": step_git_add,
    "git_commit": step_git_commit,
    "git_push": step_git_push,
    "git_log": step_git_log,
    "run_orchestrator": step_run_orchestrator,
    "sha256": step_sha256,
    "mkdir": step_mkdir,
    "write_file": step_write_file,
    "copy_file": step_copy_file,
    "delete_file": step_delete_file,
    "list_dir": step_list_dir,
}


EXAMPLE_CONFIG = [
    {"type": "git_status", "params": {"repo": "."}},
    {"type": "git_add", "params": {"repo": ".", "files": ["evidence/P06-002/TASK_REPORT.md"]}},
    {"type": "git_commit", "params": {"repo": ".", "message_file": "COMMIT_MSG.txt"}},
    {"type": "git_push", "params": {"repo": ".", "remote": "origin", "branch": "main", "timeout_sec": 180}},
    {"type": "git_log", "params": {"repo": ".", "count": 3}},
    {"type": "run_orchestrator", "params": {
        "exe": "build/artifacts/orchestrator.exe",
        "args": ["stage2", "--frames", "hiss_dir", "--output", "out.hcsd", "--config", "cfg.json"],
        "timeout_sec": 180,
        "stdout_file": "logs/stage2.jsonl",
        "stderr_file": "logs/stage2.err"
    }},
    {"type": "sha256", "params": {"path": "out.hcsd"}},
    {"type": "mkdir", "params": {"path": "evidence/P06-002/checks"}},
    {"type": "write_file", "params": {"path": "tmp.txt", "content": "hello"}},
    {"type": "list_dir", "params": {"path": "evidence/P06-002", "pattern": "*.md"}}
]


def main():
    ap = argparse.ArgumentParser(description="AstroCS 工程工具集")
    ap.add_argument("config", nargs="?", help="JSON 配置文件路径")
    ap.add_argument("--log", default=None, help="日志输出文件（默认 stderr）")
    ap.add_argument("--cwd", default=None, help="工作目录（默认当前目录）")
    ap.add_argument("--example", action="store_true", help="打印示例配置并退出")
    args = ap.parse_args()

    if args.example:
        print(json.dumps(EXAMPLE_CONFIG, ensure_ascii=False, indent=2))
        return 0

    if not args.config:
        ap.error("需要提供 config 或 --example")

    cfg_path = args.config
    with open(cfg_path, "r", encoding="utf-8") as f:
        steps = json.load(f)
    if isinstance(steps, dict):
        steps = [steps]

    ctx = {"cwd": args.cwd or os.getcwd()}
    results = []
    overall_ok = True

    for i, step in enumerate(steps):
        stype = step.get("type")
        params = step.get("params", {})
        stop_on_error = step.get("stop_on_error", False)
        handler = HANDLERS.get(stype)
        if not handler:
            r = {"step": i, "type": stype, "ok": False, "exit_code": -5,
                 "stdout": "", "stderr": f"unknown step type: {stype}",
                 "elapsed_sec": 0, "cmd": []}
        else:
            try:
                r = handler(params, ctx)
                r["step"] = i
                r["type"] = stype
            except Exception as e:
                r = {"step": i, "type": stype, "ok": False, "exit_code": -6,
                     "stdout": "", "stderr": f"{type(e).__name__}: {e}",
                     "elapsed_sec": 0, "cmd": []}
        results.append(r)
        if not r["ok"]:
            overall_ok = False
            if stop_on_error:
                break

    output = {"ok": overall_ok, "results": results}
    text = json.dumps(output, ensure_ascii=False, indent=2)
    print(text)
    if args.log:
        Path(args.log).parent.mkdir(parents=True, exist_ok=True)
        Path(args.log).write_text(text, encoding="utf-8")

    return 0 if overall_ok else 1


if __name__ == "__main__":
    sys.exit(main())
