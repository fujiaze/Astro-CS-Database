#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/wf_step.py — workflow 声明步派发器（CI-001B 目标 1/2 的运行期执行面）。

用法:
    python3 ci/wf_step.py --step <STEP_ID> [--dry-run] [--json]

设计（与 ci/validate_workflow_binding.py 静态面配对）:
- workflow 的 run: 体不再承载任何业务命令，只允许出现本派发调用；
- 「这一步做什么」只声明在 ci/workflow_binding.json，命令本体在 ci/ 内；
- 声明与 ci/checks.json 交叉核死：binds_check / serves_checks 必须在注册表内且
  waivable=false；require_outputs 必须是该检查 outputs 的子集（契约漂移即 FAIL）；
- fail-closed：登记的必需产物缺失时直接非零退出并打 ::error::，绝不 warning 跳过
  （与 WIN-PACKAGE-CANDIDATE 不可豁免联动，STD-F8 残留项收口）；
- 所有外部命令带 timeout（manifest timeout_seconds），超时 124。

退出码: 0 成功；1 fail-closed 违约（必需产物缺失/命令非零回传）；2 用法或未知 step；
        3 声明非法（role/exec 目标缺失）；4 绑定检查可豁免（漂移）；
        5 outputs 契约漂移；124 超时。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = Path(__file__).resolve().parent / "workflow_binding.json"
DEFAULT_REGISTRY = Path(__file__).resolve().parent / "checks.json"
DEFAULT_OUT_DIR = "run/ci/wf_step"

EXIT_OK = 0
EXIT_FAILCLOSED = 1
EXIT_USAGE = 2
EXIT_DECLARATION = 3
EXIT_BOUND_WAIVABLE = 4
EXIT_CONTRACT_DRIFT = 5
EXIT_TIMEOUT = 124

EXECUTABLE_ROLES = ("registry-bound-step", "infra-step", "check-body")


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load_json(path: Path) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def find_step(manifest: dict, step_id: str) -> dict | None:
    for step in manifest.get("steps", []):
        if step.get("step_id") == step_id:
            return step
    return None


def registry_index(registry: dict) -> dict:
    return {c["id"]: c for c in registry.get("checks", [])}


def check_declaration(step: dict, checks: dict, repo: Path) -> list[dict]:
    """声明与注册表的交叉核验；返回 problems 列表（空 = 通过）。"""
    problems: list[dict] = []
    role = step.get("role")
    if role not in EXECUTABLE_ROLES:
        problems.append({"code": "role_not_executable", "detail": role})
        return problems
    if role == "check-body":
        # 注册表侧门：声明必须自证 fail-closed（缺产物即 FAIL，不得跳过）
        if step.get("fail_closed") is not True:
            problems.append({"code": "check_body_not_fail_closed", "detail": step.get("step_id")})
        cid = step.get("check_id")
        if not cid:
            problems.append({"code": "check_body_unbound", "detail": step.get("step_id")})

    bound = step.get("binds_check")
    if bound:
        entry = checks.get(bound)
        if entry is None:
            problems.append({"code": "bound_check_not_registered", "detail": bound})
        elif entry.get("waivable") is not False:
            problems.append({"code": "bound_check_waivable", "detail": bound})
        else:
            declared = list(entry.get("outputs") or [])
            for rel in step.get("require_outputs") or []:
                if rel not in declared:
                    problems.append({"code": "output_contract_drift",
                                     "detail": rel + " not in " + bound + ".outputs"})
    for sid in step.get("serves_checks") or []:
        entry = checks.get(sid)
        if entry is None:
            problems.append({"code": "serves_check_not_registered", "detail": sid})
        elif entry.get("waivable") is not False:
            problems.append({"code": "serves_check_waivable", "detail": sid})

    exec_argv = step.get("exec")
    if not isinstance(exec_argv, list) or not exec_argv or not all(
            isinstance(x, str) and x for x in exec_argv):
        problems.append({"code": "exec_missing", "detail": exec_argv})
        return problems
    for tok in exec_argv[1:]:
        if tok.startswith("-"):
            continue
        if "/" in tok or tok.endswith((".py", ".sh")):
            target = Path(tok)
            if not (repo / target).is_file():
                problems.append({"code": "exec_target_missing", "detail": tok})
            break
    return problems


def evaluate(step: dict, checks: dict, repo: Path) -> tuple[int, dict]:
    """按声明核验前置条件，返回 (exit_code, record)；exit_code=0 表示可执行。"""
    required = list(step.get("require_outputs") or [])
    missing = [rel for rel in required if not (repo / rel).exists()]
    record = {
        "step_id": step.get("step_id"),
        "role": step.get("role"),
        "binds_check": step.get("binds_check"),
        "serves_checks": step.get("serves_checks") or [],
        "require_outputs": required,
        "missing_outputs": missing,
        "fail_closed": bool(step.get("fail_closed")),
    }
    if missing and step.get("fail_closed"):
        record["verdict"] = "FAIL_MISSING_OUTPUT"
        return EXIT_FAILCLOSED, record
    record["verdict"] = "READY"
    return EXIT_OK, record


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="ci/wf_step.py", description="workflow 声明步派发器")
    ap.add_argument("--step", required=True, help="ci/workflow_binding.json 的 step_id")
    ap.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    ap.add_argument("--registry", default=str(DEFAULT_REGISTRY))
    ap.add_argument("--repo-root", default=None, help="仓库根覆盖（单测注入 fixture）")
    ap.add_argument("--out-dir", default=DEFAULT_OUT_DIR)
    ap.add_argument("--dry-run", action="store_true", help="只打印解析结果，不执行")
    ap.add_argument("--json", action="store_true", help="stdout 单行 JSON")
    args = ap.parse_args(argv)

    repo = Path(args.repo_root).resolve() if args.repo_root else REPO
    manifest = load_json(Path(args.manifest))
    registry = load_json(Path(args.registry))
    checks = registry_index(registry)

    step = find_step(manifest, args.step)
    if step is None:
        print("::error::wf_step unknown step_id: " + args.step, file=sys.stderr)
        return EXIT_USAGE

    problems = check_declaration(step, checks, repo)
    if problems:
        for p in problems:
            print("::error::wf_step declaration: " + p["code"] + " " + str(p["detail"]),
                  file=sys.stderr)
        code = EXIT_DECLARATION
        if any(p["code"] == "bound_check_waivable" for p in problems):
            code = EXIT_BOUND_WAIVABLE
        elif any(p["code"] == "output_contract_drift" for p in problems):
            code = EXIT_CONTRACT_DRIFT
        return code

    exit_code, record = evaluate(step, checks, repo)
    record.update({"schema_version": 1, "task": "CI-001B", "repo": str(repo),
                   "started_utc": utc_now(), "dry_run": bool(args.dry_run),
                   "timeout_seconds": step.get("timeout_seconds"),
                   "exec": step.get("exec")})
    if exit_code != EXIT_OK:
        record["finished_utc"] = utc_now()
        record["exit_code"] = exit_code
        for rel in record["missing_outputs"]:
            print("::error::wf_step fail-closed: 登记必需产物缺失（绑定检查 "
                  + str(step.get("binds_check")) + " 不可豁免）: " + rel, file=sys.stderr)
        _write_record(repo, args.out_dir, args.step, record)
        print(json.dumps(record, ensure_ascii=False, sort_keys=True))
        return exit_code

    if args.dry_run:
        record["exit_code"] = 0
        record["finished_utc"] = utc_now()
        _write_record(repo, args.out_dir, args.step, record)
        print(json.dumps(record, ensure_ascii=False, sort_keys=True))
        return EXIT_OK

    exec_argv = list(step["exec"])
    if exec_argv and exec_argv[0] in ("python", "python3"):
        # 平台解释器名解析：hosted Linux 只有 python3、Windows 只有 python 的部分
        # 镜像下 argv[0] 直调会 FileNotFoundError；统一走当前解释器（同语义、跨平台）。
        exec_argv[0] = sys.executable
    timeout = float(step.get("timeout_seconds") or 600)
    t0 = time.monotonic()
    timed_out = False
    rc = None
    env = dict(os.environ)
    env.setdefault("PYTHONIOENCODING", "utf-8")
    env.setdefault("PYTHONUTF8", "1")
    try:
        proc = subprocess.run(exec_argv, cwd=str(repo), env=env, timeout=timeout)
        rc = proc.returncode
    except subprocess.TimeoutExpired:
        timed_out = True
    except FileNotFoundError as exc:
        record["exec_error"] = repr(exc)
        record["exit_code"] = EXIT_DECLARATION
        record["finished_utc"] = utc_now()
        _write_record(repo, args.out_dir, args.step, record)
        print(json.dumps(record, ensure_ascii=False, sort_keys=True))
        return EXIT_DECLARATION
    duration = round(time.monotonic() - t0, 3)
    if timed_out:
        rc = EXIT_TIMEOUT
        record["timed_out"] = True
    record.update({"exit_code": rc, "duration_seconds": duration,
                   "finished_utc": utc_now()})
    if rc != 0:
        record["verdict"] = "FAIL_EXEC" if not timed_out else "FAIL_TIMEOUT"
        print("::error::wf_step " + args.step + " 命令非零退出 rc=" + str(rc), file=sys.stderr)
    _write_record(repo, args.out_dir, args.step, record)
    if args.json:
        print(json.dumps(record, ensure_ascii=False, sort_keys=True))
    else:
        print("wf_step " + args.step + " rc=" + str(rc) + " duration=" + str(duration) + "s")
    return int(rc)


def _write_record(repo: Path, out_dir: str, step_id: str, record: dict) -> None:
    path = Path(out_dir)
    if not path.is_absolute():
        path = repo / path
    path.mkdir(parents=True, exist_ok=True)
    dest = path / (step_id + ".json")
    tmp = dest.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(record, ensure_ascii=False, indent=2, sort_keys=True),
                   encoding="utf-8")
    tmp.replace(dest)


if __name__ == "__main__":
    raise SystemExit(main())
