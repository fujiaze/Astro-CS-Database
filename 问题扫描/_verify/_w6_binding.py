import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
wb = json.load(open("ci/workflow_binding.json", encoding="utf-8"))
checks = {c["id"]: c for c in json.load(open("ci/checks.json", encoding="utf-8"))["checks"]}
steps = wb["steps"]
print("binding steps:", len(steps))
ids = [s["step_id"] for s in steps]
dup = [k for k, v in collections_ids(ids).items() if v > 1] if False else None
import collections
dup = [k for k, v in collections.Counter(ids).items() if v > 1]
print("重复 step_id:", dup)
# wf_step.py --step X 被谁调用：workflow 文本 + checks 命令
wf_text = ""
for w in [".github/workflows/ci-linux.yml", ".github/workflows/ci-windows.yml", ".github/workflows/fatduck.yml", ".github/workflows/fatduck-admin.yml"]:
    wf_text += open(w, encoding="utf-8", errors="replace").read()
called = set(re.findall(r"wf_step\.py\s+--step\s+([A-Z0-9\-]+)", wf_text))
called |= set(re.findall(r"--step\s+([A-Z0-9\-]+)", json.dumps(list(checks.values()), ensure_ascii=False)))
bound = set(ids)
print("workflow/checks 实际调用的 step:", sorted(called))
print("绑定了但无人调用:", sorted(bound - called))
print("被调用但未绑定:", sorted(called - bound))
# serves_checks 指向的门是否存在 / 是否存在 profile 交错
print()
for s in steps:
    for tgt in s.get("serves_checks") or []:
        if tgt not in checks:
            print("DANGLING serves_check", s["step_id"], "->", tgt)
# exec_body 里 --step 引用的 step 是否存在
for s in steps:
    body = json.dumps(s, ensure_ascii=False)
    for m in re.findall(r"--step\s+([A-Z0-9\-]+)", body):
        if m not in bound:
            print("DANGLING exec --step", s["step_id"], "->", m)
# 每个 checks 的 profiles 与 platform 组合
prof = collections.Counter()
for cid, c in checks.items():
    prof[(tuple(c["profiles"]), c["platform"])] += 1
print()
print("=== profiles × platform 组合 ===")
for k, v in sorted(prof.items(), key=lambda x: -x[1]):
    print("  ", k, v)