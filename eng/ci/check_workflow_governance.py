#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_workflow_governance.py — GitHub workflow 面的治理门（GATE-SOLID-01）。

依据（独立审查节点一页纸 S2-A「注册与承载缺失」）：
  * .github/workflows/fatduck-admin.yml 是四个 workflow 里**唯一没有检查项覆盖**的
    一个，而它是权限最重的执行通道（workflow_dispatch 输入在 self-hosted fatduck
    节点上以 pwsh 执行任意命令）⇒ 必须补载体，或如实登记为未覆盖并说明原因；
  * 判据：每个在册 workflow 都必须有注册承载（由 eng/ci/checks.json 的 inputs /
    optional_inputs / command 读它），且该 workflow 必须满足最小权限与超时治理；
  * 扫描面为空（0 个 workflow 文件）⇒ 判红（空面恒真通过是本轮要堵的根因）。

判据（fail-closed，任一违规 exit 1）：
  W1 扫描面：workflow 目录存在且至少 1 个 *.yml；
  W2 覆盖：每个 workflow 文件都被某注册项的 inputs/optional_inputs/command 命中
     （changed_paths 不算载体——那是增量触发面，不是"谁读它"）；
  W3 最小权限：每个 workflow 必须有顶层 permissions 段，且不得出现任何 : write；
  W4 超时：每个 job 必须有 timeout-minutes；
  W5 fatduck-admin 专属：只允许 workflow_dispatch 触发、runs-on 必须是
     self-hosted + fatduck-realdata、cmd 输入必须 required。

用法:
  python3 eng/ci/check_workflow_governance.py                    # 全量判据
  python3 eng/ci/check_workflow_governance.py --json-out run/... # 另落证据 JSON
  python3 eng/ci/check_workflow_governance.py --selftest         # 负例面（内存 fixture）

只读；仅 stdlib（文本级解析，不依赖第三方 yaml 包）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
WORKFLOW_DIR = ".github/workflows"
REGISTRY = "eng/ci/checks.json"
ADMIN_WORKFLOW = "fatduck-admin.yml"


def read(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def load_registry(repo: pathlib.Path, rel: str) -> dict:
    path = repo / rel
    if not path.is_file():
        raise SystemExit("WORKFLOW_GOV_INPUT_MISSING: 注册表不存在: %s" % path)
    return json.loads(read(path))


def coverage_carriers(registry: dict, wf_rel: str) -> list:
    """返回"读该 workflow"的注册项 id 列表（只看 inputs/optional_inputs/command）。"""
    base = pathlib.PurePosixPath(wf_rel).name
    hits: list = []
    for check in registry.get("checks", []):
        texts: list = []
        for step in [check] + list(check.get("steps") or []):
            for field in ("inputs", "optional_inputs", "command"):
                val = step.get(field)
                if isinstance(val, list):
                    texts.extend(str(x) for x in val)
                elif isinstance(val, str):
                    texts.append(val)
        blob = " ".join(texts)
        if wf_rel in blob or base in blob:
            hits.append(check["id"])
    return sorted(set(hits))


TRIGGER_RE = re.compile(r"^(on|true)\s*:")
PERM_WRITE_RE = re.compile(r"^\s+[A-Za-z-]+:\s*write\b")
TIMEOUT_RE = re.compile(r"^\s+timeout-minutes:\s*\d+\s*$")


def parse_workflow(text: str) -> dict:
    """文本级解析 workflow 的关键治理面（不依赖 yaml 包，判据面窄而明确）。"""
    lines = text.splitlines()
    triggers: list = []
    in_on = False
    permissions_present = False
    top_write: list = []
    job_write: list = []
    in_top_perms = False
    timeout_jobs = 0
    job_count = 0
    in_jobs = False
    for line in lines:
        stripped = line.strip()
        if line.startswith("on:") or stripped in ("on:",):
            in_on = True
            in_jobs = False
            if stripped != "on:":
                triggers.append(stripped.split(":", 1)[1].strip())
            continue
        if line.startswith("permissions:"):
            permissions_present = True
            in_on = False
            in_jobs = False
            in_top_perms = True
            continue
        if line.startswith("jobs:"):
            in_on = False
            in_jobs = True
            continue
        if not line.startswith(" ") and stripped and not stripped.startswith("#"):
            in_on = False
            in_jobs = False
        if in_on and re.match(r"^\s{2}[A-Za-z_]+:", line):
            triggers.append(stripped.split(":", 1)[0])
        if PERM_WRITE_RE.match(line):
            # 顶层 permissions 段内的写权限 = 全局扩权（红）；job 级写权限 = 记录
            # （如 fatduck.yml 的 issues: write 用于失败开 issue，属正当用途）。
            (top_write if in_top_perms else job_write).append(stripped)
        if in_top_perms and not PERM_WRITE_RE.match(line) and re.match(r"^\s+\S", line):
            if not re.match(r"^\s+[A-Za-z-]+:\s*$", line):
                in_top_perms = False
        if in_jobs and re.match(r"^\s{2}[A-Za-z0-9_.-]+:\s*$", line):
            job_count += 1
        if in_jobs and TIMEOUT_RE.match(line):
            timeout_jobs += 1
    return {"triggers": sorted(set(triggers)), "permissions_present": permissions_present,
            "top_write_permissions": top_write, "job_write_permissions": job_write,
            "jobs": job_count, "jobs_with_timeout": timeout_jobs, "text": text}


def check_workflows(repo: pathlib.Path, registry: dict, wf_dir: str) -> dict:
    root = repo / wf_dir
    files = sorted(set(list(root.glob("*.yml")) + list(root.glob("*.yaml")))) if root.is_dir() else []
    files = [f for f in files if f.is_file()]
    problems: list = []
    per_file: list = []
    if not files:
        problems.append("W1 WORKFLOW_SCAN_EMPTY: %s 不存在或没有任何 *.yml"
                        "（扫描面为空 = 门没看对象，fail-closed 判红）" % root)
        return {"workflow_dir": str(root), "files": [], "per_file": [],
                "problems": problems}
    for f in files:
        rel = f.relative_to(repo).as_posix()
        info = parse_workflow(read(f))
        carriers = coverage_carriers(registry, rel)
        is_admin = pathlib.PurePosixPath(rel).name == ADMIN_WORKFLOW
        entry = {"workflow": rel, "triggers": info["triggers"],
                 "permissions_present": info["permissions_present"],
                 "top_write_permissions": info["top_write_permissions"],
                 "job_write_permissions": info["job_write_permissions"],
                 "jobs": info["jobs"], "jobs_with_timeout": info["jobs_with_timeout"],
                 "carriers": carriers}
        per_file.append(entry)
        if not carriers:
            problems.append("W2 WORKFLOW_UNCOVERED: %s 没有任何注册项读它"
                            "（补载体，或在覆盖登记里写明为何不覆盖）" % rel)
        if not info["permissions_present"]:
            problems.append("W3 WORKFLOW_PERMISSIONS_MISSING: %s 缺顶层 permissions 段"
                            "（最小权限必须显式声明）" % rel)
        if info["top_write_permissions"]:
            problems.append("W3 WORKFLOW_TOP_WRITE_PERMISSION: %s 顶层 permissions 段含写"
                            "权限 %s（全局扩权；写权限只能落在具体 job 上并说明用途）"
                            % (rel, info["top_write_permissions"]))
        if is_admin and info["job_write_permissions"]:
            problems.append("W3 FATDUCK_ADMIN_WRITE_PERMISSION: %s 是权限最重的执行通道"
                            "（pwsh 执行任意输入），不得声明任何写权限，实际含 %s"
                            % (rel, info["job_write_permissions"]))
        if info["jobs"] and info["jobs_with_timeout"] < info["jobs"]:
            problems.append("W4 WORKFLOW_JOB_TIMEOUT_MISSING: %s 有 %d 个 job 但只有 %d 个"
                            "带 timeout-minutes" % (rel, info["jobs"], info["jobs_with_timeout"]))
        if is_admin:
            entry["admin_policy"] = True
            extra = [t for t in info["triggers"] if t != "workflow_dispatch"]
            if extra:
                problems.append("W5 FATDUCK_ADMIN_TRIGGER: %s 只允许 workflow_dispatch，"
                                "实际还含 %s" % (rel, extra))
            if "workflow_dispatch" not in info["triggers"]:
                problems.append("W5 FATDUCK_ADMIN_TRIGGER: %s 缺 workflow_dispatch" % rel)
            if "self-hosted" not in info["text"] or "fatduck-realdata" not in info["text"]:
                problems.append("W5 FATDUCK_ADMIN_RUNNER: %s 的 runs-on 必须是"
                                " self-hosted + fatduck-realdata" % rel)
            if not re.search(r"cmd:\s*\n(?:\s+.*\n)*?\s+required:\s*true", info["text"]):
                problems.append("W5 FATDUCK_ADMIN_INPUT: %s 的 cmd 输入必须 required: true"
                                % rel)
    return {"workflow_dir": str(root),
            "files": [f.relative_to(repo).as_posix() for f in files],
            "per_file": per_file, "problems": problems}


def selftest() -> int:
    """负例面：未覆盖 / 写权限 / 缺超时 / fatduck-admin 触发面扩大 都必须判红。"""
    cases: list = []
    registry = {"checks": [{"id": "CHK-X", "inputs": [".github/workflows/a.yml"],
                           "command": ["python3", "x.py"]}]}
    good = ("name: a\non:\n  workflow_dispatch:\npermissions:\n  contents: read\n"
            "jobs:\n  j:\n    timeout-minutes: 5\n    steps:\n      - run: echo hi\n")
    admin_good = ("name: fatduck-admin\non:\n  workflow_dispatch:\n    inputs:\n"
                  "      cmd:\n        required: true\n        type: string\n"
                  "permissions:\n  contents: read\njobs:\n  admin:\n"
                  "    runs-on: [self-hosted, fatduck-realdata]\n    timeout-minutes: 30\n"
                  "    steps:\n      - run: echo hi\n")

    def run(name, files: dict, reg, expect_problems):
        with tempfile.TemporaryDirectory() as tmp:
            repo = pathlib.Path(tmp)
            wf = repo / WORKFLOW_DIR
            wf.mkdir(parents=True)
            for fname, body in files.items():
                (wf / fname).write_text(body, encoding="utf-8")
            rep = check_workflows(repo, reg, WORKFLOW_DIR)
            got = bool(rep["problems"])
            cases.append({"case": name, "ok": got == expect_problems,
                          "problems": rep["problems"][:2]})

    run("P1_covered_minimal_green", {"a.yml": good}, registry, False)
    run("P2_admin_policy_green", {ADMIN_WORKFLOW: admin_good},
        {"checks": [{"id": "CHK-X", "inputs": [".github/workflows/" + ADMIN_WORKFLOW],
                     "command": ["python3", "x.py"]}]}, False)
    run("N1_uncovered_is_red", {"b.yml": good}, registry, True)
    run("N2_write_permission_is_red",
        {"a.yml": good.replace("  contents: read", "  contents: write")}, registry, True)
    run("N3_missing_timeout_is_red",
        {"a.yml": good.replace("    timeout-minutes: 5\n", "")}, registry, True)
    run("N4_admin_push_trigger_is_red",
        {ADMIN_WORKFLOW: admin_good.replace("on:\n  workflow_dispatch:",
                                            "on:\n  push:\n    branches: [main]\n  workflow_dispatch:")},
        {"checks": [{"id": "CHK-X", "inputs": [".github/workflows/" + ADMIN_WORKFLOW],
                     "command": ["python3", "x.py"]}]}, True)
    run("N5_empty_scan_is_red", {}, registry, True)
    ok = all(c["ok"] for c in cases)
    for c in cases:
        print("SELFTEST %s %s%s" % ("PASS" if c["ok"] else "FAIL", c["case"],
                                    "" if c["ok"] else "  " + repr(c["problems"])))
    print("WORKFLOW_GOV_SELFTEST_%s: %d/%d"
          % ("PASS" if ok else "FAIL", sum(1 for c in cases if c["ok"]), len(cases)))
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="workflow 面治理门（覆盖 + 最小权限 + 超时）")
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--registry", default=REGISTRY)
    ap.add_argument("--workflow-dir", default=WORKFLOW_DIR)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    repo = pathlib.Path(args.repo).resolve()
    try:
        registry = load_registry(repo, args.registry)
    except SystemExit as exc:
        print(str(exc), file=sys.stderr)
        return 2
    report = check_workflows(repo, registry, args.workflow_dir)
    report["verdict"] = "PASS" if not report["problems"] else "FAIL"
    if args.json_out:
        p = pathlib.Path(args.json_out)
        if not p.is_absolute():
            p = repo / p
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=1) + "\n",
                     encoding="utf-8")
    if report["problems"]:
        for item in report["problems"]:
            print("WORKFLOW_GOV_FAIL: %s" % item, file=sys.stderr)
        print("WORKFLOW_GOV_FAIL: %d 项违规（判词见上，逐条带 文件）" % len(report["problems"]))
        return 1
    if not args.quiet:
        print("WORKFLOW_GOV_PASS: %d 个 workflow 全部有注册载体且满足最小权限/超时治理"
              % len(report["files"]))
        for e in report["per_file"]:
            print("  %s carriers=%s triggers=%s timeout=%d/%d"
                  % (e["workflow"], e["carriers"], e["triggers"],
                     e["jobs_with_timeout"], e["jobs"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
