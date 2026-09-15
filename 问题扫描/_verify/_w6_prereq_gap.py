import json, os, re, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
TOOLS = ["g++", "gcc", "nm", "objdump", "tar", "taskset", "make", "cmake", "ctest", "clang", "clang++", "dumpbin", "ar", "readelf", "git", "bash", "sh", "where"]
SQ = chr(39)
DQ = chr(34)
pyfiles = [p for p in subprocess.run(["git", "--no-optional-locks", "-c", "core.quotepath=false", "ls-files", "-z", "*.py"], capture_output=True, text=True).stdout.split("\0") if p]
use = collections.defaultdict(set)
SKIP = ("问题扫描/", "工程控制/", "设计大纲/", "worktrees/", "run/", "build/")
for f in pyfiles:
    if any(f.startswith(s) for s in SKIP):
        continue
    txt = open(f, encoding="utf-8", errors="replace").read()
    for t in TOOLS:
        pats = []
        for q in (SQ, DQ):
            pats.append(r"\[\s*" + q + re.escape(t) + q)
            pats.append(r"\(\s*" + q + re.escape(t) + q)
            pats.append(r"which\(\s*" + q + re.escape(t) + q)
        for p in pats:
            if re.search(p, txt):
                use[t].add(f)
                break
for t in TOOLS:
    print("TOOL", t, "pyfiles:", len(use[t]))

imp_cache = {}
def imports_of(f):
    if f in imp_cache:
        return imp_cache[f]
    txt = open(f, encoding="utf-8", errors="replace").read()
    mods = set(re.findall(r"^(?:from|import)\s+([A-Za-z_][A-Za-z0-9_.]*)", txt, re.M))
    res = []
    for m in mods:
        cand = m.replace(".", "/") + ".py"
        if os.path.exists(cand):
            res.append(cand)
    imp_cache[f] = res
    return res

rows = []
for c in checks:
    cmd = c["command"]
    es = [a for a in cmd if isinstance(a, str) and a.endswith(".py") and os.path.exists(a)]
    corpus = set(es)
    for e in es:
        corpus.update(imports_of(e))
    hit = {}
    for t in TOOLS:
        fs = sorted(x for x in corpus if x in use[t])
        if fs:
            hit[t] = fs
    if hit:
        rows.append((c["id"], c["profiles"], c.get("prerequisite_tools") or [], hit))
print()
print("=== 口径 W6-E: 门入口脚本(+一级 import) 字面量调用外部工具 vs prerequisite_tools 声明 ===")
und = 0
toolgap = collections.Counter()
for cid, prof, pre, hit in rows:
    miss = sorted(t for t in hit if t not in pre)
    if miss:
        und += 1
        for t in miss: toolgap[t] += 1
        print("GATE", cid, "| declared:", pre, "| UNDECLARED:", [[t, hit[t]] for t in miss if t not in ("git",)])
print("有工具字面量调用的门数:", len(rows), "| 存在未声明工具的门数:", und)
print("未声明工具分布:", dict(toolgap))