
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
targets=["const_field_bitwise","f1_detect_rc","f4_oracle_rc","recovery","skip_hdu","perf_parity"]
occ={t:[] for t in targets}
for f in files:
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    for i,l in enumerate(t.split(chr(10))):
        for n in targets:
            if n in l: occ[n].append((f,i+1,l.strip()[:120]))
for n in targets:
    print("### "+n+"  hits="+str(len(occ[n])))
    for f,ln,l in occ[n][:12]: print("    ",f+":"+str(ln),"|",l)
    print()
