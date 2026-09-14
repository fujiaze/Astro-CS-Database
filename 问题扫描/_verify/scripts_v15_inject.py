# -*- coding: utf-8 -*-
"""V15 普查器 E：注入式反向锁（故意造缺陷 → 断言门变红）的真实数量。
   判据（严格）：同一测试内既 (a) 构造/写入或注入一个被检物的缺陷变体
   （临时副本改写、篡改字段、设 env FAULT、写坏 fixture），
   又 (b) 断言该缺陷变体导致**失败结论**（rc!=0 / 返回 False / status==FAIL / raised）。
   只有 (b) 无 (a) 的不算（那是普通负例输入测试，也算一种，故分档统计）。"""
import os, re, json
ROOT = "/workspace/Astro CS Database"
SKIP_DIRS = {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports",
             "工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描",
             "third_party","testdata","CS","Database","graph",".git","astrocs_p1sess_neg",
             "astrocs_p1sess_perf","astrocs_p1sess_props","astrocs_p1sess_test"}

INJECT = re.compile(r'(setenv\(|putenv|environ\[|env\[|write_text\(|open\([^)]*"w"|os\.replace|shutil\.copy|copyfile|_BAD|bad_variant|tamper|corrupt|mutate|篡改|改回|注入|inject)')
EXPECT_FAIL = re.compile(r'(assertNotEqual\([^,]+,\s*0\)|assertGreater\(rc|rc\s*!=\s*0|returncode\s*!=\s*0|assertEqual\([^,]*returncode,\s*[1-9]\)|assertRaises|EXPECT.*FAIL|status\s*==\s*(FAIL|Failed)|!= 0|!=0|failed|not ok|FAIL\b)')
FAULTENV = re.compile(r'ASTROCS_[A-Z0-9_]*FAULT')

tiers = {"A_env_fault_injection": {}, "B_write_defect_then_expect_fail": {}, "C_expect_fail_only": {}}
def bump(t, f):
    t[f] = t.get(f, 0) + 1

for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
    for fn in filenames:
        if not fn.endswith((".py",".cpp",".c",".h",".hpp")): continue
        p = os.path.join(dirpath, fn)
        r = "./" + os.path.relpath(p, ROOT).replace(os.sep,"/")
        inT = r.startswith(("./tests/","./ci/","./tools/"))
        inL = r.startswith("./lib/") and (("/tests/" in r) or "_test." in r or "selftest" in r)
        inC = r.startswith("./cli/") or r.startswith("./providers/")
        if not (inT or inL or inC): continue
        try: txt = open(p, encoding="utf-8", errors="replace").read()
        except Exception: continue
        lines = txt.split("\n")
        # A: env FAULT
        n = len(FAULTENV.findall(txt))
        if n and (inT or inL): bump(tiers["A_env_fault_injection"], r)
        # B: per-function heuristic (py: test method; cpp: between funcs rough)
        if fn.endswith(".py"):
            cur = None; buf = []
            for i, ln in enumerate(lines + ["    def _sentinel()"]):
                if re.match(r"\s*def test", ln):
                    if cur:
                        b = "\n".join(buf)
                        if INJECT.search(b) and EXPECT_FAIL.search(b): bump(tiers["B_write_defect_then_expect_fail"], cur)
                        elif EXPECT_FAIL.search(b): bump(tiers["C_expect_fail_only"], cur)
                    cur = f"{r}::{ln.strip().split('(')[1].split(':')[0].replace('def_','') if '(' in ln else ln.strip()}"
                    cur = f"{r}::{ln.strip()}"
                    buf = []
                else:
                    buf.append(ln)
            continue
        else:
            b = txt
            if INJECT.search(b) and re.search(r'CHECK\([^)]*(!= *0|== *-|failed|failures)', b):
                bump(tiers["B_write_defect_then_expect_fail"], r)

print("A 类（env FAULT 注入名命中，文件级）:", len(tiers["A_env_fault_injection"]), "文件")
for f,c in sorted(tiers["A_env_fault_injection"].items()): print(f"   {c:4d}  {f}")
print()
print("B 类（构造缺陷变体 + 断言失败，py 逐测试函数 / cpp 逐文件）:", len(tiers["B_write_defect_then_expect_fail"]))
for f in sorted(tiers["B_write_defect_then_expect_fail"]): print("   ", f)
print()
print("C 类（只断言失败、无自建缺陷变体的 py 测试函数数）:", len(tiers["C_expect_fail_only"]))
json.dump({k:{kk:vv for kk,vv in v.items()} for k,v in tiers.items()},
          open(os.path.join(ROOT,"问题扫描/_verify/scripts_v15_inject.json"),"w",encoding="utf-8"), ensure_ascii=False, indent=1)
