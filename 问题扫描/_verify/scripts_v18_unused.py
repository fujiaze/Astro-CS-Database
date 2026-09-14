import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
EXT=(".c",".cc",".cpp",".h",".hpp",".inc")
BS=chr(92)
rows=[]
for f in files:
    if not f.endswith(EXT): continue
    p=os.path.join(ROOT,f[2:])
    try: txt=open(p,encoding="utf-8",errors="replace").read()
    except: continue
    lines=txt.split(chr(10))
    i=0
    while i<len(lines):
        m=re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)\(([^)]*)\)", lines[i])
        if m:
            body=[lines[i]]; j=i
            while body[-1].rstrip().endswith(BS) and j+1<len(lines): j+=1; body.append(lines[j])
            b=chr(10).join(body)
            d=0; hdr_end=None
            for k,ch in enumerate(b):
                if ch=="(": d+=1
                elif ch==")":
                    d-=1
                    if d==0: hdr_end=k; break
            bodytext=b[hdr_end+1:] if hdr_end else ""
            ps=[x.strip() for x in m.group(2).split(",") if x.strip()]
            unused=[x for x in ps if x!="..." and not re.search(r"\b"+re.escape(x)+r"\b", bodytext)]
            empty=not bodytext.strip().replace(BS,"").strip()
            if (unused or empty) and ps:
                rows.append({"file":f,"line":i+1,"macro":m.group(1),"params":ps,"unused":unused,"empty_body":empty,"body":bodytext.strip()[:150]})
            i=j+1; continue
        i+=1
json.dump(rows,open(ROOT+"/问题扫描/_verify/scripts_v18_unused.json","w"),ensure_ascii=False,indent=1)
print("macros with unused-param or empty body:",len(rows))
def isassert(n): return bool(re.search(r"CHECK|ASSERT|EXPECT|VERIFY|REQUIRE",n))
a=[r for r in rows if isassert(r["macro"])]
print("of which assertion-like:",len(a))
for r in a: print("  ASSERTLIKE", r["file"]+":"+str(r["line"]), r["macro"], "params="+str(r["params"]), "UNUSED="+str(r["unused"]), "| body:", r["body"][:80].replace(chr(10)," "))
print()