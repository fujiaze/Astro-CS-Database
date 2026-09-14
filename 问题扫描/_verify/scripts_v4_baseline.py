import json
d=json.load(open("/workspace/Astro CS Database/ci/ctest_baseline.json",encoding='utf-8'))
print("type:", type(d).__name__, "keys:", list(d)[:8] if isinstance(d,dict) else len(d))
def find(o, needles):
    s=json.dumps(o,ensure_ascii=False)
    return {n: (n in s) for n in needles}
print(find(d, ["io_ownership","p2_workers","mon001_recorder","mon002_gate","mon001_gate","cpu_monitor","p1_resource","p2_seam_gate","core_scheduler"]))
if isinstance(d,dict):
    for k,v in d.items():
        print(k, "->", (len(v) if isinstance(v,list) else v) if k!='tests' else len(v))
