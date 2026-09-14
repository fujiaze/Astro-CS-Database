import json, os, re, collections
root="/workspace/Astro CS Database"
j=json.load(open(os.path.join(root,"问题扫描/_cache/v12_mad.json")))
PROD=re.compile(r"^(lib|providers|cli|runtime|modules)/")
CODE=re.compile(r"^(lib|providers|cli|runtime|modules|tools|scripts|ci|cmake|packaging|tests)/")
for k in j:
    prod=[h for h in j[k] if PROD.match(h[0])]
    code=[h for h in j[k] if CODE.match(h[0])]
    print(f"### {k}: total={len(j[k])} code={len(code)} prod={len(prod)}")
print()
for k in ["1.482602218505602","1.4826(bare)","0.6745","1.230310"]:
    print("=== "+k+" :: PROD (non-comment) ===")
    for p,i,l in j[k]:
        if PROD.match(p):
            # mark comment-only
            tag = "//" if re.match(r"^(//|\*|/\*|#|\s*\*)", l) else "  "
            if tag=="//": continue
            print(f"  {p}:{i}  {l[:120]}")
    print()
