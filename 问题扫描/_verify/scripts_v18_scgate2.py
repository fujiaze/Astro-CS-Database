import json,re
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
for t in ["check_pipeline_graph.py","check_prod_reachability.py","check_isa_leak.py","check_serial_heavy.py","check_ctest_registration.py","validate_task_ledger.py","check_commits_csv.py"]:
    us=[c for c in d if t in json.dumps(c.get("command",[]))]
    print("### "+t+"  ci_entries=%d"%len(us))
    for c in us:
        print("    ",c["id"],"profiles=",c.get("profiles"),"waivable=",c.get("waivable"),"cmd="," ".join(c["command"]))
