import json, os, re, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
wb = json.load(open("ci/workflow_binding.json", encoding="utf-8"))
print("workflow_binding keys:", list(wb.keys()))
print(json.dumps(wb, ensure_ascii=False)[:3000])