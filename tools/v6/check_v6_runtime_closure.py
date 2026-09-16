#!/usr/bin/env python3
"""RUNTIME-CI-001 收口 CI 入口（V6 运行面）。

子命令:
  closure       独立 Oracle（全部规则）+ lib/infrastructure/scheduler/v6_budget.py selftest + 负向 mutation
  cli-mode      Oracle R1/R2/R3（CLI 模式路由 + 三 Phase 隔离）
  resource-gate Oracle R5/R6（§10.5 字段面 + SO-05 记录/裁决分离）+ budget selftest
  mutations     仅负向 mutation 驱动
  budget        lib/infrastructure/scheduler/v6_budget.py selftest

所有子命令输出 JSON 证据（--json-out），任一失败 rc != 0；无零用例 PASS。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

DEFAULT_REPO = pathlib.Path(__file__).resolve().parents[2]


def _run(argv, timeout=1800):
    p = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def _emit(path, doc):
    if not path:
        return
    p = pathlib.Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _oracle(repo, rules, out):
    argv = [sys.executable, str(repo / "tools/v6/v6_runtime_oracle.py"),
            "--repo", str(repo)]
    if rules:
        argv += ["--rules", rules]
    if out:
        argv += ["--json-out", str(out)]
    return _run(argv)


def _budget(repo, out):
    argv = [sys.executable, str(repo / "lib/infrastructure/scheduler/v6_budget.py"), "selftest"]
    return _run(argv)


def _mutations(repo, out):
    argv = [sys.executable, str(repo / "tools/v6/v6_runtime_mutation_driver.py"),
            "--repo", str(repo)]
    if out:
        argv += ["--json-out", str(out)]
    return _run(argv, timeout=1200)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["closure", "cli-mode", "resource-gate", "mutations", "budget"])
    ap.add_argument("--repo", default=str(DEFAULT_REPO))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--oracle-out", default="")
    ap.add_argument("--mutations-out", default="")
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo).resolve()
    if not (repo / "lib/infrastructure/cli/v6_runtime_contract.h").exists():
        print("check_v6_runtime_closure: repo invalid: %s" % repo, file=sys.stderr)
        return 2

    doc = {"schema": "astrocs.v6.runtime-ci/v1", "command": args.cmd, "steps": []}
    failed = False

    def record(name, rc, log):
        nonlocal failed
        doc["steps"].append({"step": name, "rc": rc, "tail": log[-600:]})
        print("[%s] %s rc=%d" % ("PASS" if rc == 0 else "FAIL", name, rc))
        if rc != 0:
            failed = True
            print(log[-1200:])

    if args.cmd == "budget":
        rc, log = _budget(repo, args.json_out)
        record("budget_selftest", rc, log)
    elif args.cmd == "mutations":
        rc, log = _mutations(repo, args.mutations_out or args.json_out)
        record("mutation_driver", rc, log)
    elif args.cmd == "cli-mode":
        rc, log = _oracle(repo, "R1,R2,R3", args.json_out)
        record("oracle_R1_R2_R3", rc, log)
    elif args.cmd == "resource-gate":
        rc, log = _oracle(repo, "R5,R6", args.oracle_out)
        record("oracle_R5_R6", rc, log)
        rc2, log2 = _budget(repo, "")
        record("budget_selftest", rc2, log2)
    else:  # closure
        rc, log = _oracle(repo, "", args.oracle_out)
        record("oracle_all_rules", rc, log)
        rc2, log2 = _budget(repo, "")
        record("budget_selftest", rc2, log2)
        rc3, log3 = _mutations(repo, args.mutations_out)
        record("mutation_driver", rc3, log3)

    doc["verdict"] = "FAIL" if failed else "PASS"
    _emit(args.json_out, doc)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
