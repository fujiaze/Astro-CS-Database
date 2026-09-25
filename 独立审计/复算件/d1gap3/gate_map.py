import json, io, sys, re, collections
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
P = r"F:\Astro dev\Astro CS Normalization Database\eng\ci\checks.json"
d = json.load(open(P, encoding="utf-8"))
gates = d["gates"] if isinstance(d, dict) and "gates" in d else d
print("type:", type(d), "keys:", list(d)[:8] if isinstance(d, dict) else None)

def flatten(x):
    out = []
    if isinstance(x, dict):
        if "id" in x and isinstance(x.get("id"), str):
            out.append(x)
        for v in x.values():
            out += flatten(v)
    elif isinstance(x, list):
        for v in x:
            out += flatten(v)
    return out

G = flatten(gates)
uniq = {}
for g in G:
    uniq.setdefault(g["id"], g)
print("total id-nodes:", len(G), " unique ids:", len(uniq))

# collect every docs pointer per gate
ptr = collections.defaultdict(set)
def docs_of(node):
    s = json.dumps(node, ensure_ascii=False)
    return set(re.findall(r"docs/[A-Za-z0-9_./-]+", s)) | set(re.findall(r"[A-Za-z0-9_]+_STANDARD\.md", s))

for gid, g in uniq.items():
    for p in docs_of(g):
        ptr[p].add(gid)

print("\n=== which gates reference any docs/ path ===")
for p in sorted(ptr):
    if p.startswith("docs/"):
        print(f"{len(ptr[p]):3d}  {p}")

print("\n=== TEST_STANDARD.md referenced by ===")
for p in sorted(ptr):
    if "TEST_STANDARD" in p:
        print("  ", p, sorted(ptr[p]))
print("  (none)" if not any("TEST_STANDARD" in p for p in ptr) else "")

print("\n=== every *_STANDARD.md token appearing anywhere in checks.json ===")
blob = open(P, encoding="utf-8").read()
print(sorted(set(re.findall(r"[A-Za-z0-9_]+_STANDARD\.md", blob))) or "NONE")
print("count '_STANDARD' :", blob.count("_STANDARD"))

print("\n=== gate ids whose docs pointer set is EMPTY (no doc anchor) ===")
no_doc = [gid for gid, g in uniq.items() if not docs_of(g)]
print(len(no_doc), "of", len(uniq))

print("\n=== tier/name sample for candidate test-standard gates ===")
KEY = re.compile(r"test|oracle|coverage|seed|determin|naming|contract|unit|prod|artifact|hash", re.I)
for gid, g in sorted(uniq.items()):
    if KEY.search(gid) or KEY.search(json.dumps({k: v for k, v in g.items() if k in ("name", "docs", "description")}, ensure_ascii=False)):
        print(f"- {gid} | tier={g.get('tier')} | name={str(g.get('name'))[:60]} | docs={sorted(docs_of(g))[:3]}")
