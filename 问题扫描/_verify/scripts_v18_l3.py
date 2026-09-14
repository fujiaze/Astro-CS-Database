import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
for t in ["check_log_contract","check_pipeline_graph","check_isa_leak","check_prod_reachability"]:
    print("### L3 tree-wide references to "+t)
    n=0
    for f in files:
        p=os.path.join(ROOT,f[2:])
        try: txt=open(p,encoding="utf-8",errors="replace").read()
        except: continue
        if t in txt:
            n+=1
            print("    ",f, "  occurrences:",txt.count(t))
    print("    files:",n); print()