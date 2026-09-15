import subprocess, os, re, json
REPO = "/workspace/Astro CS Database"
def git(args):
    return subprocess.run(["git","-c","core.quotePath=false"]+args, cwd=REPO, capture_output=True, text=True, errors="replace").stdout
packrel = "工程控制/REAUDIT_V4/v4_reaudit"
last = git(["log","--all","-1","--format=%H","--", packrel]).strip()
print("last touch:", last[:8] if last else None)
for rev in (last, last+"^"):
    fs = [x for x in git(["ls-tree","-r","--name-only",rev,"--",packrel]).splitlines() if x.strip()]
    print("rev", rev[:12], "-> files", len(fs), fs[:2])
ev = {"dels":[], "last":None, "adds":[]}
cur=None
for line in git(["log","--all","-M","--name-status","--date=iso-strict","--format=C%x1f%H%x1f%aI","--",packrel]).splitlines():
    if line.startswith("C\x1f"): _,cur,date = line.split("\x1f"); continue
    if not line.strip() or not cur: continue
    f=line.split("\t"); ev["dels" if f[0].startswith("D") else "adds" if f[0].startswith("A") else "last"].append((cur,date,f[-1]))
print("dels", [x[0][:8] for x in ev["dels"]][:3], "adds", [x[0][:8] for x in ev["adds"]][:3])
for cand,_ in ev["dels"][:2]:
    for rev in (cand+"^", cand+"^1", cand+"^2"):
        fs=[x for x in git(["ls-tree","-r","--name-only",rev,"--",packrel]).splitlines() if x.strip()]
        print("delcand", rev[:12], len(fs))
