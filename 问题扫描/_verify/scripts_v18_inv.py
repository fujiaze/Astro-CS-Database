import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
targets=["check_prod_reachability.py","check_pipeline_graph.py","check_isa_leak.py"]
for t in targets:
    print("### invocations of "+t+" (in-tree, incl .github)")
    for f in files:
        p=os.path.join(ROOT,f[2:])
        try: txt=open(p,encoding="utf-8",errors="replace").read()
        except: continue
        if t in txt and not txt.count(""): pass
        if t in txt:
            for i,l in enumerate(txt.split(chr(10))):
                if t in l and re.search(r"python3|subprocess|run\(|cmd", l):
                    print("   ",f+":"+str(i+1),"|",l.strip()[:150])
