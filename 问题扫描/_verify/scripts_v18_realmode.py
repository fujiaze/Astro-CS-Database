import os,re
ROOT="/workspace/Astro CS Database"
tg=[("tests/monitoring/test_log_contract.py","check_log_contract"),("tests/monitoring/test_monitor_contract.py","check_log_contract"),
("tests/backend/test_p2006_canonical_pipeline.py","check_pipeline_graph"),("tests/cli/test_phase123_pipeline.py","check_pipeline_graph"),
("tests/cli/test_p1003_drizzle_path.py","check_prod_reachability"),("CMakeLists.txt","check_isa_leak")]
for f,t in tg:
    p=os.path.join(ROOT,f)
    if not os.path.exists(p): print("MISS",f); continue
    lines=open(p,encoding="utf-8",errors="replace").read().split(chr(10))
    print("=== "+f+" ("+str(len(lines))+" lines)")
    for i,l in enumerate(lines):
        if t in l:
            lo=max(0,i-3); hi=min(len(lines),i+8)
            for k in range(lo,hi): print("   %4d: %s"%(k+1,lines[k].rstrip()[:150]))
            print("   ---")