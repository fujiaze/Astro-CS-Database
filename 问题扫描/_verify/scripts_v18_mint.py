
import os,re,json
ROOT="/workspace/Astro CS Database"
files=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_files.json",encoding="utf-8"))
pat=re.compile(r'(VERIFIED|CONFORMANT|"PASS"|status\s*=\s*"|TEST-[A-Z0-9\-]|EVID-[A-Z0-9\-])')
rows=[]
for f in files:
    if not f.startswith("./tools/") and not f.startswith("./scripts/") and not f.startswith("./ci/"): continue
    if not f.endswith(".py"): continue
    try: t=open(os.path.join(ROOT,f[2:]),encoding="utf-8",errors="replace").read()
    except: continue
    writes = re.search(r'(write_text|open\([^)]*"w"|csv\.writer|json\.dump)', t)
    if not writes: continue
    minted = set(re.findall(r'f"[A-Z]{2,6}-(?:\{|[A-Z0-9])[^"]*"|f\'[A-Z]{2,6}-(?:\{|[A-Z0-9])[^\']*\'', t))
    status = set(re.findall(r'"(VERIFIED|CONFORMANT|PASS|IMPLEMENTED)"', t))
    if minted and status:
        rows.append((f,sorted(minted)[:4],sorted(status)))
print("tools/scripts/ci py that BOTH mints IDs by f-string AND writes a status literal:",len(rows))
for r in rows: print("   ",r[0],"| minted:",r[1],"| status:",r[2])
