import re, pathlib, json, collections
REPO=pathlib.Path("/workspace/Astro CS Database")
names=collections.defaultdict(list)
for cm in REPO.glob("tests/**/CMakeLists.txt"):
    txt=cm.read_text(encoding='utf-8', errors='replace')
    for m in re.finditer(r'add_test\s*\(\s*NAME\s+([A-Za-z0-9_.\-/]+)', txt):
        names[m.group(1)].append(str(cm.relative_to(REPO)))
    for m in re.finditer(r'add_test\s*\(\s*(COMMAND|\$\{)', txt):
        names['<NON-LITERAL-'+cm.name+'>'].append(str(cm.relative_to(REPO)))
print("total literal add_test names:", len(names))
want=["p2_workers","io_ownership","mon001_recorder","mon002_gate","mon001_gate","p1_resource","cpu_monitor","p2_seam_gate","core_scheduler"]
for w in want:
    hit=[k for k in names if w in k]
    print(f"  {w:16s} -> {hit}")
print()
print("=== all add_test names containing worker/ownership/mon00/sched ===")
for k,v in sorted(names.items()):
    if re.search(r'worker|ownership|mon00|sched|resource', k):
        print("  ", k, "<-", v)
