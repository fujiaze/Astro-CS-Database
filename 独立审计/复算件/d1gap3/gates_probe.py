import json, io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
P = r"F:\Astro dev\Astro CS Normalization Database\eng\ci\checks.json"
d = json.load(open(P, encoding="utf-8"))

rows = []
def walk(node, trail):
    if isinstance(node, dict):
        if "id" in node and isinstance(node["id"], str):
            rows.append((trail, node))
        for k, v in node.items():
            walk(v, trail + [k] if not isinstance(v, (dict, list)) or k not in ("checks", "steps", "groups") else trail)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            walk(v, trail)
walk(d, [])
print("top-level keys:", list(d) if isinstance(d, dict) else type(d))
print("nodes with id:", len(rows))

KW = re.compile(r"test|oracle|seed|determin|coverage|gcov|skip|artifact|hash|naming|contract|unit|public|api|prod", re.I)
def txt(n):
    return " ".join(str(v) for k, v in n.items() if not isinstance(v, (dict, list)))

seen = set()
out = []
for trail, n in rows:
    i = n["id"]
    if i in seen:
        continue
    seen.add(i)
    if KW.search(i) or KW.search(txt(n)):
        out.append((i, n))
print("matched ids:", len(out), "of", len(seen))
for i, n in out[:70]:
    fields = {k: v for k, v in n.items() if k in ("name", "title", "description", "desc", "purpose", "command", "cmd")}
    s = json.dumps(fields, ensure_ascii=False)
    print("-", i, "|", s[:260])
