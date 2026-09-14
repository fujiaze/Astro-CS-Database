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
            bt=b[hdr_end+1:] if hdr_end else ""
            bt_nostr=re.sub(r"#\s*([A-Za-z_]\w*)", lambda mm: "@@HIDDEN@@", bt)
            ps=[x.strip() for x in m.group(2).split(",") if x.strip()]
            never=[x for x in ps if x!="..." and not re.search(r"\b"+re.escape(x)+r"\b", bt_nostr)]
            if never:
                rows.append({"file":f,"line":i+1,"macro":m.group(1),"params":ps,"never_used":never,"body":bt.strip()[:220]})
            i=j+1; continue
        i+=1
json.dump(rows,open(ROOT+"/问题扫描/_verify/scripts_v18_str.json","w"),ensure_ascii=False,indent=1)
print("macros with a param that is NEVER evaluated (stringify-only or dropped):",len(rows))
for r in rows:
    print("  %-30s :%d  %s" % (r["macro"], r["line"], r["file"]))
    print("      params=%s  NEVER-EVAL=%s" % (r["params"], r["never_used"]))
    print("      body="+r["body"].replace(chr(10)," / ")[:180])
