#!/usr/bin/env python3
"""AUD-501 census5: AST-precise sweeps over gate implementation files.
1) subprocess text=True/universal_newlines without encoding/errors
2) empty-scan-green: `if not X: return 0/exit(0)/return 'PASS'` and `if len(X)==0: ...`
3) hardcoded verdict constants in return/assignment position
4) self-test surface presence
Fail-closed: zero files scanned -> exit 1.
"""
import ast, os, sys, glob, re

REPO = r"F:/Astro dev/Astro CS Normalization Database"
FILES = []
for pat in ("eng/ci/*.py", "eng/ci/steps/*.py", "eng/tools/**/*.py", "eng/packaging/*.py",
            "eng/contracts/*.py", "eng/build/*.py", "eng/cmake/*.py"):
    for f in glob.glob(os.path.join(REPO, *pat.split("/")), recursive=True):
        rel = os.path.relpath(f, REPO).replace("\\", "/")
        if "__pycache__" in rel or "/tests/" in rel or rel.startswith("eng/tests/"):
            continue
        FILES.append(rel)
FILES = sorted(set(FILES))
if not FILES:
    print("CENSUS-FAIL: no files scanned"); sys.exit(1)
print("FILES_SCANNED", len(FILES))

def parse(rel):
    try:
        return ast.parse(open(os.path.join(REPO, rel), encoding="utf-8", errors="replace").read())
    except SyntaxError as e:
        print("  PARSE-ERROR", rel, e); return None

# ---------- 1) subprocess encoding ----------
hits = []
SUB = {"run","check_output","check_call","Popen","call"}
for rel in FILES:
    t = parse(rel)
    if not t: continue
    for n in ast.walk(t):
        if isinstance(n, ast.Call):
            f = n.func
            name = f.attr if isinstance(f, ast.Attribute) else (f.id if isinstance(f, ast.Name) else "")
            mod = ast.unparse(f.value) if isinstance(f, ast.Attribute) else ""
            if name in SUB and ("subprocess" in mod or name == "Popen"):
                kws = {k.arg for k in n.keywords}
                texty = ("text" in kws) or ("universal_newlines" in kws)
                if texty and not ({"encoding","errors"} & kws):
                    hits.append((rel, n.lineno, name, sorted(kws)))
print("\n== (1) subprocess text=True WITHOUT encoding/errors ==", len(hits))
for rel, ln, name, kws in hits:
    print(f"  {rel}:{ln}  {name}  kws={kws}")

# ---------- 2) empty-scan-green ----------
def ret_is_zero(node):
    if isinstance(node, ast.Return):
        v = node.value
        if isinstance(v, ast.Constant) and (v.value == 0 or v.value in ("PASS","pass","OK")):
            return repr(v.value)
        return None
    if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call):
        c = node.value
        s = ast.unparse(c)
        if re.search(r"sys\.exit\(0\)|raise SystemExit\(0\)", s):
            return "exit0"
    return None

empty_green = []
for rel in FILES:
    t = parse(rel)
    if not t: continue
    for n in ast.walk(t):
        if not isinstance(n, ast.If): continue
        test = n.test
        empt = None
        if isinstance(test, ast.UnaryOp) and isinstance(test.op, ast.Not):
            empt = ast.unparse(test.operand)
        elif isinstance(test, ast.Compare) and isinstance(test.ops[0], (ast.Eq, ast.LtE)) \
             and ast.unparse(test.ops[0]) in ("==","<=") and isinstance(test.comparators[0], ast.Constant) \
             and test.comparators[0].value == 0:
            empt = ast.unparse(test.left)
        if not empt: continue
        for st in n.body:
            r = ret_is_zero(st)
            if r:
                empty_green.append((rel, n.lineno, empt, r))
                break
print("\n== (2) EMPTY-SCAN-GREEN candidates (`if not X: return 0/exit0/'PASS'`) ==", len(empty_green))
for rel, ln, e, r in empty_green:
    print(f"  {rel}:{ln}  if not {e} -> {r}")

# ---------- 3) hardcoded verdict literals ----------
hard = []
for rel in FILES:
    t = parse(rel)
    if not t: continue
    for n in ast.walk(t):
        if isinstance(n, ast.Assign) and len(n.targets) == 1 and isinstance(n.targets[0], ast.Name):
            tgt = n.targets[0].id
            v = n.value
            if isinstance(v, ast.Constant) and isinstance(v.value, str) and re.fullmatch(
                    r"(PASS|pass|OK|ok|GREEN|green|VERIFIED|SUCCESS)", str(v.value)) \
               and re.search(r"(verdict|result|status|conclusion|state)", tgt, re.I):
                hard.append((rel, n.lineno, tgt, v.value))
        if isinstance(n, ast.Return) and isinstance(n.value, ast.Constant) \
           and isinstance(n.value.value, str) and re.fullmatch(r"(PASS|pass|OK|GREEN|VERIFIED)", n.value.value):
            hard.append((rel, n.lineno, "(return)", n.value.value))
print("\n== (3) HARDCODED verdict literals (name matches verdict|result|status|conclusion|state) ==", len(hard))
for rel, ln, tgt, v in hard:
    print(f"  {rel}:{ln}  {tgt} = {v!r}")

# ---------- 4) self-test surface ----------
st_yes, st_no = [], []
for rel in FILES:
    if not os.path.basename(rel).startswith("check_"): continue
    s = open(os.path.join(REPO, rel), encoding="utf-8", errors="replace").read()
    (st_yes if "--self-test" in s or "self_test" in s else st_no).append(rel)
print("\n== (4) checker scripts WITHOUT --self-test surface ==", len(st_no), "of", len(st_yes)+len(st_no))
for rel in st_no: print("  ", rel)
