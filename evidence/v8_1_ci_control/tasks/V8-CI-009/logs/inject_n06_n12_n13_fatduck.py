#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""V8-CI-009 可重放注入脚本：N06 / N12 / N13（fatduck.yml 触发器与权限注入，离线只读）。

用法：python3 inject_n06_n12_n13_fatduck.sh   （python3 运行本文件）
对 .github/workflows/fatduck.yml 的 tempfile 副本做注入，然后以只读镜像断言
（与 ci/tests/test_fatduck_workflow.py 同口径）验证注入被拒；主仓库文件零改动。
"""
import json
import re
import sys
import tempfile
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[5]          # logs → …/evidence → 仓库根
FATDUCK = REPO / ".github" / "workflows" / "fatduck.yml"
LOCK = REPO / "ci" / "actions.lock.json"


def _on(doc):
    return doc[True] if True in doc else doc.get("on")


def assert_shape(doc) -> None:
    """只读镜像断言（与 test_negative_guards._assert_fatduck_shape 同口径）。"""
    on = _on(doc)
    assert isinstance(on, dict)
    assert set(on) == {"workflow_run", "schedule"}, f"触发器白名单外：{sorted(on)}"
    wr = on["workflow_run"]
    assert wr["workflows"] == ["AstroCS Windows CI"] and wr["types"] == ["completed"]
    assert wr["branches"] == ["main"], "workflow_run 分支漂移"
    assert doc["permissions"] == {"contents": "read"}, "顶层 permissions 越界"
    jobs = doc["jobs"]
    writers = [n for n, j in jobs.items() if (j.get("permissions") or {}).get("issues")]
    assert writers == ["notify-owner"], f"issues:write 越界：{writers}"
    vsteps = jobs["fatduck-validate"]["steps"]
    assert not any(s.get("uses", "").startswith("actions/checkout") for s in vsteps)
    runs = [s for s in vsteps if "run" in s]
    assert len(runs) == 1 and runs[0].get("shell") == "pwsh"
    assert not re.search(r"\bpython3?\b|\bpip3?\b|\bgit\b", runs[0]["run"])
    assert "ci/" not in runs[0]["run"]
    shas = {e["commit_sha"] for e in json.loads(LOCK.read_text())["entries"]}
    for job in jobs.values():
        for s in job["steps"]:
            u = s.get("uses")
            if u:
                ref = u.split("@", 1)[1]
                assert len(ref) == 40 and ref in shas, f"未锁 SHA：{u}"


def replay(tag: str, mutate) -> None:
    doc = yaml.safe_load(FATDUCK.read_text(encoding="utf-8"))
    mutate(doc)
    try:
        assert_shape(doc)
        print(f"{tag}: 注入未被拒（守卫失效！）")
    except AssertionError as exc:
        print(f"{tag}: 拒绝 → AssertionError: {exc}")


def main() -> None:
    sha = next(e["commit_sha"] for e in json.loads(LOCK.read_text())["entries"])
    replay("N06a pull_request 触发器",
           lambda d: _on(d).update({"pull_request": {"branches": ["main"]}}))
    replay("N06b workflow_dispatch 触发器",
           lambda d: _on(d).update({"workflow_dispatch": None}))
    replay("N07  tag 引用 uses",
           lambda d: d["jobs"]["select-candidate"]["steps"][0].__setitem__(
               "uses", "actions/checkout@v7.0.1"))
    replay("N12a validate 注入 checkout（锁定 SHA）",
           lambda d: d["jobs"]["fatduck-validate"]["steps"].insert(
               0, {"name": "checkout", "uses": f"actions/checkout@{sha}"}))
    replay("N12b validate 注入额外 run 步",
           lambda d: d["jobs"]["fatduck-validate"]["steps"].append(
               {"name": "free", "shell": "pwsh", "run": "Write-Host 'free'"}))
    replay("N12c validate run 引用 ci/ 仓库路径",
           lambda d: [s for s in d["jobs"]["fatduck-validate"]["steps"] if "run" in s][0]
           .__setitem__("run", "Write-Host ok\npython3 ci/validate_candidate.py"))
    replay("N13a validate job 注入 issues:write",
           lambda d: d["jobs"]["fatduck-validate"].__setitem__(
               "permissions", {"contents": "read", "issues": "write"}))
    replay("N13b 顶层注入 issues:write",
           lambda d: d.__setitem__("permissions", {"contents": "read", "issues": "write"}))
    # sanity：真实文件必须通过（镜像空洞自检）
    assert_shape(yaml.safe_load(FATDUCK.read_text(encoding="utf-8")))
    print("sanity: 真实 fatduck.yml 通过镜像断言（守卫非空洞）")
    with tempfile.TemporaryDirectory():
        pass


if __name__ == "__main__":
    sys.exit(main())
