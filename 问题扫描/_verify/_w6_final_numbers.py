import os, re, json, subprocess, collections, fnmatch
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
base = json.load(open("ci/ctest_baseline.json", encoding="utf-8"))
R = json.load(open(os.path.join(HERE, "_w6_reach.json"), encoding="utf-8"))
reach = set(R["reachable"])
SKIP = {"run","build","out","artifacts",".git","third_party","node_modules",".venv","__pycache__","BASS DR3","AstroCS.wiki"}
add_re = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")
allf = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","*CMakeLists.txt","*.cmake"], capture_output=True, text=True).stdout.split(chr(0)) if p]
act = [f for f in allf if not (set(f.split("/")) & SKIP) and not any(s in f for s in ("archive","superseded"))]
names_all, names_reach = set(), set()
for f in act:
    t = open(f, encoding="utf-8", errors="replace").read()
    body = "\n".join(l for l in t.splitlines() if not l.lstrip().startswith("#"))
    ns = set(m.group(1).strip().strip(chr(34)) for m in add_re.finditer(body))
    names_all |= ns
    if f in reach: names_reach |= ns
pats = []
for c in checks:
    for t in (c.get("ctest_targets") or []): pats.append(t)
lit = set(p for p in pats if not any(ch in p for ch in "*?["))
globs = [p for p in pats if any(ch in p for ch in "*?[")]
cov = set(lit) | set(n for n in names_all for g in globs if fnmatch.fnmatch(n, g))
print("口径 W6-N 活动 CMake 源:", len(act), "| add_test 名(全活动面):", len(names_all), "| 根图可达面:", len(names_reach))
print("门 ctest_targets: 字面", len(lit), "/ glob", len(globs), "-> 覆盖名", len(cov & names_all))
print("根图可达但无任何门 ctest_targets 覆盖:", len(names_reach - cov))
print("其中在 baseline:", len((names_reach - cov) & set(base["targets"])), "| 既无门也不在 baseline:", sorted((names_reach - cov) - set(base["targets"]))[:6])
print("baseline targets:", len(base["targets"]), "| sources:", len(base.get("sources", [])), "| baseline 中属不可达子图:", sorted(set(base["targets"]) - names_reach))
print("在根图可达面但不在 baseline:", len(names_reach - set(base["targets"])))
print()
decl = [(c["id"], c.get("prerequisite_tools") or []) for c in checks if c.get("prerequisite_tools")]
print("口径 W6-D 声明 prerequisite_tools 的门:", len(decl), "/", len(checks))
vals = collections.Counter(v for _, vs in decl for v in vs)
print("   声明值集合:", dict(vals))
for t in ["g++","gcc","nm","objdump","tar","bash","taskset","ctest","make"]:
    print("   %-9s 被声明次数 %d" % (t, sum(1 for _, vs in decl for v in vs if str(v).split(":")[0] == t)))
print()
print("口径 W6-H struct_size 普查（受跟踪 .h/.hpp，排除 third_party/ACR/browser_qt/orchestrator）:")
hs = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","*.h","*.hpp"], capture_output=True, text=True).stdout.split(chr(0)) if p and "third_party" not in p and "/acr/" not in p and "healpix_browser_qt" not in p and "orchestrator" not in p and "/archive/" not in p]
tot = withs = withhead = 0
for h in hs:
    t = open(h, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"typedef\s+struct[^{]*\{", t):
        i = t.find("{", m.start()); d = 0; j = i
        while j < len(t):
            if t[j] == "{": d += 1
            elif t[j] == "}":
                d -= 1
                if d == 0: break
            j += 1
        blk = t[i:j]
        nm = re.match(r"typedef\s+struct\s*(?:\w+\s*)?\{", m.group(0)) and (not re.match(r"typedef\s+struct\s*\{", m.group(0)))
        if re.search(r"typedef\s+struct\s*\{", m.group(0)) or re.search(r"\}\s*[A-Za-z_]\w*\s*;", t[j:j+80]):
            tot += 1
            if "struct_size" in blk: withs += 1
            elif "acs_head" in blk: withhead += 1
print("   头文件分母:", len(hs), "| typedef struct{{}} X; 匿名形:", tot, "| 带 struct_size:", withs, "| 带 acs_head(无 struct_size):", withhead, "| 两者皆无:", tot - withs - withhead)