
import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
F = json.load(open(os.path.join(HERE, "_w6_cmake_facts.json"), encoding="utf-8"))
R = json.load(open(os.path.join(HERE, "_w6_reach.json"), encoding="utf-8"))
reach = set(R["reachable"])

# 复刻 CI-REG-002 的采集面（CMake 源文本）与根图真实注册面（add_subdirectory 可达闭包）
files = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","*CMakeLists.txt","*.cmake"], capture_output=True, text=True).stdout.split(chr(0)) if p]
SKIP_DIRS = {"run","build","out","artifacts",".git","third_party","node_modules",".venv","__pycache__","BASS DR3","AstroCS.wiki"}
act = [f for f in files if not (set(f.split("/")) & SKIP_DIRS) and not any(s in f for s in ("archive","superseded"))]
print("口径 W6-N：CI-REG-002 视角的活动 CMake 源 =", len(act), "| 根图 add_subdirectory 可达闭包 =", len(reach))
add_re = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")
act_targets = {}
for f in act:
    txt = open(f, encoding="utf-8", errors="replace").read()
    body = "\n".join(l for l in txt.splitlines() if not l.lstrip().startswith("#"))
    for m in add_re.finditer(body):
        act_targets.setdefault(m.group(1).strip().strip('"'), []).append(f)
root_targets = {}
for f in reach:
    if f not in act: pass
    txt = open(f, encoding="utf-8", errors="replace").read()
    body = "\n".join(l for l in txt.splitlines() if not l.lstrip().startswith("#"))
    for m in add_re.finditer(body):
        root_targets.setdefault(m.group(1).strip().strip('"'), []).append(f)
print("CI-REG-002 采集面目标名:", len(act_targets), " 根图可达采集面目标名:", len(root_targets))
only_not_graph = sorted(set(act_targets) - set(root_targets))
print("在采集面但不在根图（不可达子图）:", len(only_not_graph))
for t in only_not_graph: print("   OFF-GRAPH-TARGET", t, act_targets[t])

# 条件块内的 add_test：向上找最近的 if( 判断 guard 类型
cond = collections.defaultdict(list)
for f in sorted(set(sum(root_targets.values(), []) + sum(act_targets.values(), []))):
    txt = open(f, encoding="utf-8", errors="replace").read()
    lines = txt.splitlines()
    stack = []
    for n, raw in enumerate(lines, 1):
        code = raw.split("#")[0]
        for m in re.finditer(r"\b(if|foreach|while|if\()\s*\(([^)]*)\)|(if|foreach|while)\s*\(([^)]*)\)|\bendif\s*\(\s*\)|\bend(if|foreach|while)\s*\(", code):
            pass
        low = code.strip().lower()
        if low.startswith("if(") or low.startswith("if ("):
            g = re.sub(r"^if\s*\(\s*|\s*\)\s*$", "", code.strip())
            stack.append((n, g))
        elif low.startswith("endif"):
            if stack: stack.pop()
        am = add_re.search(code)
        if am:
            guards = [g for _, g in stack]
            hit = [g for g in guards if re.search(r"\bTARGET\b|\bEXISTS\b|_FOUND\b|\bAND\b|MSVC|UNIX|WIN32|GTest|OpenMP|Python", g, re.I)]
            if hit:
                cond[f + ":" + str(n)].append((am.group(1).strip().strip(chr(34)), " || ".join(hit)))
print()
print("=== 口径 W6-O：根图内位于 if() 条件块中的 add_test（其注册取决于 configure 期条件） ===")
n = 0
for loc, v in sorted(cond.items()):
    if loc.split(":")[0] not in reach: continue
    for name, g in v:
        n += 1
        print("   %-34s %-38s guard=%s" % (name, loc, g[:78]))
print("   小计(根图内条件用例名):", n)
