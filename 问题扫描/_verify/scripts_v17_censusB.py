import os, re, json
ROOT = "/workspace/Astro CS Database"
# find: except ...: pass (silently swallow), and "if not exists: return/continue/pass" inside tools/ + ci/ gate scripts
pat_try = re.compile(r"except\s*(\([^)]*\]|[A-Za-z_.]+)\s*:\s*(#.*)?$")
pat_pass = re.compile(r"^\s*pass\s*(#.*)?$")
pat_exists = re.compile(r"if\s+(not\s+)?(os\.path\.(exists|isfile|isdir)|Path\(.*\)\.exists|\.exists\(\)|os\.path)")
out = {"try_pass": [], "exists_guard_return": []}
for d in ["tools", "ci", "scripts", "tests", "lib", "engineering", "packaging"]:
    for dp, dn, fn in os.walk(os.path.join(ROOT, d)):
        if "archive" in dp or "__pycache__" in dp or "问题扫描" in dp:
            continue
        for f in fn:
            if not f.endswith(".py"):
                continue
            p = os.path.relpath(os.path.join(dp, f), ROOT)
            try:
                lines = open(os.path.join(ROOT, p), encoding="utf-8", errors="replace").read().splitlines()
            except Exception:
                continue
            for i in range(len(lines)):
                ln = lines[i]
                if pat_try.search(ln):
                    # look at next non-blank line for pass
                    j = i + 1
                    while j < len(lines) and lines[j].strip() == "":
                        j += 1
                    if j < len(lines) and pat_pass.match(lines[j]):
                        out["try_pass"].append(p + ":" + str(i + 1) + ": " + ln.strip()[:100] + " -> " + lines[j].strip())
                if pat_exists.search(ln) and i + 1 < len(lines) and re.match(r"^\s*(return|continue|pass)\b", lines[i + 1]):
                    out["exists_guard_return"].append(p + ":" + str(i + 1) + ": " + ln.strip()[:100] + " -> " + lines[i + 1].strip()[:60])
print("try/except-pass:", len(out["try_pass"]))
for x in out["try_pass"]: print("  ", x)
print("exists-guard-return:", len(out["exists_guard_return"]))
for x in out["exists_guard_return"]: print("  ", x)
json.dump(out, open(os.path.join(ROOT, "问题扫描/_verify/scripts_v17_censusB.json"), "w"), ensure_ascii=False, indent=1)