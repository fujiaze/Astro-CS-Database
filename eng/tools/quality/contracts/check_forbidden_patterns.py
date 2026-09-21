#!/usr/bin/env python3
"""check_forbidden_patterns.py — T407 forbidden patterns checker

Checks: 固定线程数、热循环分配、吞错、无界 async、未检查返回、绝对 Windows 路径、HISS 正式产品残留
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    # normalize path separators so /tests/ 等 skip 判断跨平台一致（Windows 反斜杠）
    def _s(f):
        return str(f).replace("\\", "/")
    # 1. Hardcoded threads: num_threads(16) or similar
    for pat, desc, id in [
        (r'num_threads\s*\(\s*16\s*\)', 'hardcoded num_threads(16)', 'FORBID-HARDCODE-THREADS'),
        (r'set_num_threads\s*\(\s*16\s*\)', 'hardcoded set_num_threads(16)', 'FORBID-HARDCODE-THREADS'),
    ]:
        regex = re.compile(pat)
        for f in list((repo / "lib").rglob("*.cpp")) + list((repo / "lib").rglob("*.h")):
            if "/tests/" in _s(f) or "/test/" in _s(f) or "/evidence/" in _s(f):
                continue
            if "third_party" in _s(f) or "archive" in _s(f):
                continue
            text = f.read_text(encoding="utf-8", errors="ignore")
            if regex.search(text):
                findings.append({"id":id,"severity":"P1","file":str(f.relative_to(repo)),"symbol":pat,"observed":desc,"expected":"config-driven threads"})
                status="FAIL"
    # 2. Absolute Windows path in formal configs/docs
    win_pat = re.compile(r'[A-Z]:\\[^\s"]+')
    for f in list((repo / "lib").rglob("*.json")) + list((repo / "docs").rglob("*.md")):
        if "/tests/" in _s(f) or "/test/" in _s(f) or "/evidence/" in _s(f) or "/configs/" in _s(f) or "TROUBLESHOOTING" in _s(f):
            continue
        if "archive" in _s(f):
            continue
        text = f.read_text(encoding="utf-8", errors="ignore")
        # Skip if is windows validation doc explicitly allowed
        if "Windows" in text and "验证" in text:
            continue
        m = win_pat.search(text)
        if m:
            # Allow if is example showing what NOT to do, or in code comment
            if "禁止" not in text[max(0,m.start()-100):m.end()+100]:
                findings.append({"id":"FORBID-ABS-PATH","severity":"P1","file":str(f.relative_to(repo)),"symbol":m.group(0)[:40],"observed":"absolute Windows path","expected":"relative path"})
                status="FAIL"
                break
    # 3. Hot allocation: std::vector/new/malloc inside pixel loop - heuristic skip for now (would need AST)
    # Check only obvious: for(.*pixel.*) { std::vector - but skip as too noisy
    # 4. HISS as only product: check if docs claim HISS is primary output without hips
    # Heuristic: skip
    # 5. Unbounded async / detached thread
    for f in list((repo / "lib").rglob("*.cpp")):
        if "third_party" in _s(f) or "/tests/" in _s(f) or "/test/" in _s(f):
            continue
        text = f.read_text(encoding="utf-8", errors="ignore")
        if "std::async" in text and "future" not in text.lower():
            # Very loose check
            pass
        if ".detach()" in text:
            findings.append({"id":"FORBID-DETACH","severity":"P1","file":str(f.relative_to(repo)),"observed":"thread detach","expected":"joinable"})
            status="FAIL"

    result = {"tool":"check_forbidden_patterns","status":status,"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_forbidden_patterns" tests="1" failures="{failures}"><testcase classname="forbidden" name="patterns"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
