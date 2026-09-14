
import json,re,os,subprocess
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
cmake=[f for f in files if f.endswith("CMakeLists.txt") or f.endswith(".cmake")]
print("tracked cmake-ish:",len(cmake))
rows=[]
for f in cmake:
    p=ROOT+"/"+f[2:]
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    lines=txt.split("\n")
    # find add_test( blocks (may be multiline)
    for i,l in enumerate(lines):
        if re.search(r'add_test\s*\(', l):
            blk=[l]; j=i
            while blk and blk[-1].count("(")>blk[-1].count(")") and j+1<len(lines):
                j+=1; blk.append(lines[j])
            t="\n".join(x.rstrip() for x in blk)
            if "selfcheck" in t.lower() or "SELFCHK" in t.upper():
                rows.append((f,i+1,t))
print("add_test blocks mentioning selfcheck:",len(rows))
for f,ln,t in rows:
    env = re.search(r'ENVIRONMENT[^"]*"([^"]*)"', t)
    pre = re.search(r'PASS_REGULAR_EXPRESSION[^"]*"([^"]*)"', t)
    fail = re.search(r'FAIL_REGULAR_EXPRESSION[^"]*"([^"]*)"', t)
    nm = re.search(r'NAME\s+\S+\s+"?([^"\s]+)', t)
    print("---", f, ":%d"%ln, "| name=", nm.group(1) if nm else "?")
    print("    ENVIRONMENT=", env.group(1) if env else None)
    print("    PASS_RE=", pre.group(1) if pre else None, " FAIL_RE=", fail.group(1) if fail else None)
    if not env and not pre: print("    RAW:", t.replace("\n"," ")[:300])
