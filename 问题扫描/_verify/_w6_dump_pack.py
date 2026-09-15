import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
for p in ["packaging/install-tree.contract.json", "packaging/astrocs.product.json"]:
    d = json.load(open(p, encoding="utf-8"))
    print("=====", p, "keys:", list(d.keys()))
    print(json.dumps(d, ensure_ascii=False, indent=1)[:3500])