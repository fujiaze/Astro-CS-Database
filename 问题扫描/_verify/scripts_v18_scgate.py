import json,re
ROOT="/workspace/Astro CS Database"
d=json.load(open(ROOT+"/ci/checks.json",encoding="utf-8"))["checks"]
# which tools scripts have a selfcheck capability (from previous scan)
tools_with_self=["validate_task_ledger.py","check_log_contract.py","check_commits_csv.py","check_isa_leak.py",
"gen_run_graphs.py","known_failures_baseline.py","check_pipeline_graph.py","frame_qc_grid.py","render_run_graph.py",
"check_ctest_registration.py","check_prod_reachability.py","check_serial_heavy.py"]
cmdmap={}
for c in d:
    cmdmap[c["id"]]=json.dumps(c.get("command",[]),ensure_ascii=False)+" | profiles="+str(c.get("profiles"))+" | waivable="+str(c.get("waivable"))
print("=== gates whose command mentions --selfcheck ===")
for k,v in cmdmap.items():
    if "selfcheck" in v: print("  ",k,"->",v[:230])
print()
for t in tools_with_self:
    users=[(k,v) for k,v in cmdmap.items() if t in v]
    sc=[ (k,v) for k,v in users if "--selfcheck" in v]
    print("%-32s ci_uses=%d  with_selfcheck=%d" % (t, len(users), len(sc)))
    for k,v in users:
        print("      ", k, "selfcheck" if "--selfcheck" in v else "NO-SELFCHECK", "|", v[:150])