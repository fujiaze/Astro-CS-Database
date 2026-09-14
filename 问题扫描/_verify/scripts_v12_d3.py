import os,re
root="/workspace/Astro CS Database"
tracked=set(open(os.path.join(root,"问题扫描/_cache/v12_tracked.txt")).read().splitlines())
def show(pats, scope=None, maxn=60):
    n=0
    for f in sorted(tracked):
        if scope and not f.startswith(scope): continue
        if os.path.splitext(f)[1] not in {".c",".h",".cpp",".hpp",".py",".md",".json",".yaml",".yml",".inc",".txt",".csv"}: continue
        try: lines=open(os.path.join(root,f),encoding="utf-8",errors="ignore").read().splitlines()
        except Exception: continue
        for i,l in enumerate(lines,1):
            for lab,pat in pats:
                if re.search(pat,l):
                    print(f"[{lab}] {f}:{i}: {l.strip()[:150]}"); n+=1
    print("(shown",n,")")
print("########## A. NUMERIC_STANDARD 常量位数纪律 ##########")
show([("numstd", r"常量|位数|有效数字|1e-12|ulp|float 常量")], scope=("docs/standards/NUMERIC_STANDARD.md",))
print()
print("########## B. 04_TASK_SPECIFICATIONS / MON-001/002 权威出处 ##########")
show([("mon", r"MON-001|MON-002|04_TASK_SPECIFICATIONS|04_CPU_RESOURCE_TASKS")])
