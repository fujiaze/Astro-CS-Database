#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CONTRACT-FREEZE-001 越界写审计：
1) 四个 write_scope 下的全部新文件登记为 manifest；
2) 不得修改任何 tracked 文件；
3) scope 下 git 未跟踪文件集合 == manifest；
4) 生成器所有输出目标字面量必须在 scope 内。"""
import os, re, subprocess, sys, json

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
SCOPES = ("docs/science/v6/frozen/", "docs/contracts/v6/frozen/", "docs/algorithms/v6/frozen/",
          "reports/v6/contract-review/")

def git(*args):
    return subprocess.run(["git", "-c", "core.quotepath=false"] + list(args), cwd=ROOT,
                          capture_output=True, text=True).stdout

def main():
    fails = []
    manifest = []
    for base in SCOPES:
        for dirpath, _, files in os.walk(os.path.join(ROOT, base)):
            for fn in files:
                rel = os.path.relpath(os.path.join(dirpath, fn), ROOT).replace(os.sep, "/")
                manifest.append(rel)
    for rel in manifest:
        if not rel.startswith(SCOPES):
            fails.append("manifest outside scope: " + rel)

    st = git("status", "--porcelain", "-uall", "--ignored").splitlines()
    tracked_changes, untracked, ignored = [], [], []
    for line in st:
        if len(line) < 4:
            continue
        xy, path = line[:2], line[3:].strip()
        if path.startswith('"') and path.endswith('"'):
            path = path[1:-1]
        if xy == "!!":
            ignored.append(path)
        elif "?" in xy:
            untracked.append(path)
        else:
            tracked_changes.append(path)
    for p in tracked_changes:
        if p.startswith(SCOPES):
            fails.append("tracked file modified under write_scope: " + p)
    under = [p for p in untracked if p.startswith(SCOPES)]
    under_ignored = [p for p in ignored if p.startswith(SCOPES)]
    visible = set(under) | set(under_ignored)
    for p in manifest:
        if p not in visible:
            fails.append("manifest file neither untracked nor ignored: " + p)
    for p in under:
        if p not in manifest:
            fails.append("git-untracked under scope but not in manifest: " + p)

    # 静态：生成器输出目标字面量必须在 scope 内
    genp = os.path.join(ROOT, "reports", "v6", "contract-review", "tools", "gen_freeze.py")
    if os.path.isfile(genp):
        gtxt = open(genp, encoding="utf-8").read()
        targets = re.findall(r'\bw\(\s*"([^"]+)"', gtxt) + re.findall(r'write_doc\(\s*"([^"]+)"', gtxt)
        for t in targets:
            if not t.startswith(SCOPES):
                fails.append("generator output target outside scope: " + t)
    summary = {"manifest_count": len(manifest), "untracked_under_scope": len(under), "ignored_under_scope": len(under_ignored),
               "tracked_changes_total": len(tracked_changes),
               "tracked_changes_under_scope": [p for p in tracked_changes if p.startswith(SCOPES)],
               "manifest": sorted(manifest), "scopes": list(SCOPES), "violations": fails}
    os.makedirs(os.path.join(ROOT, "reports/v6/contract-review/evidence"), exist_ok=True)
    with open(os.path.join(ROOT, "reports/v6/contract-review/evidence/scope_check.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=1)
    print(json.dumps({k: summary[k] for k in ("manifest_count", "untracked_under_scope", "tracked_changes_total", "violations")}, ensure_ascii=False))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
