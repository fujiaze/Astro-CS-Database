
import json,re,os
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
cmake=[f for f in files if f.endswith("CMakeLists.txt") or f.endswith(".cmake")]
for f in cmake:
    p=ROOT+"/"+f[2:]
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    if "set_tests_properties" in txt or "ENVIRONMENT" in txt:
        lines=txt.split("\n")
        for i,l in enumerate(lines):
            if "set_tests_properties" in l or ("ENVIRONMENT" in l and "add_test" not in l):
                blk=[l]; j=i
                while blk.count(")")<blk.count("(") and j+1<len(lines):
                    j+=1; blk.append(lines[j])
                print("---",f,":%d"%(i+1)); print("   " + "\n   ".join(x.strip() for x in blk)[:600])
