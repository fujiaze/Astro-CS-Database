
import json,re
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
print("=== all checks whose changed_paths mention runtime/logging or schemas ===")
for c in d:
    cp=json.dumps(c.get("changed_paths",[]))
    if "logging" in cp or "schemas" in cp:
        print("  ",c["id"],"| cmd="," ".join(c["command"])[:150])
print()
print("=== does ANY check feed real logs to the contract checker? ===")
hits=[c["id"] for c in d if re.search(r"--jsonl|--stdin", json.dumps(c.get("command",[])))]
print("  gates with --jsonl/--stdin:",hits or "NONE")
print()
print("=== gates referencing LOG- in id ===")
for c in d:
    if "LOG" in c["id"]: print("  ",c["id"],"|"," ".join(c["command"])[:160])
