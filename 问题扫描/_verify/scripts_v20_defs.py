
import re, os, collections, json
# Collect exported C-ABI function definitions across lib/ providers/ runtime/ include/
roots=['lib','providers','runtime','include']
files=[]
for root in roots:
    for dp,dn,fn in os.walk(root):
        if '/third_party' in dp: continue
        for f in fn:
            if f.endswith(('.c','.cpp','.cc')): files.append(os.path.join(dp,f))
print('files scanned:', len(files))
# exported markers: extern "C" blocks are pervasive; we detect definitions with a body.
sig = re.compile(r'^(?:static\s+)?(?:inline\s+)?(?:__declspec\([^)]*\)|GAIA_EXPORT|AIO_EXPORT|[A-Z_]*_EXPORT)?\s*[A-Za-z_][\w\*\s:<>,\[\]]*?\b([A-Za-z_][\w]*)\s*\(([^)]*)\)\s*(?:const\s*)?\{?\s*$')
out=[]
for p in files:
    src=open(p,encoding='utf-8',errors='replace').read()
    lines=src.split('\n')
    for i,l in enumerate(lines):
        m=sig.match(l)
        if not m: continue
        name=m.group(1); params=m.group(2)
        if name in ('if','for','while','switch','return','sizeof'): continue
        if not params.strip(): continue
        out.append((p,i+1,name,params))
print('candidate defs with params:', len(out))
json.dump(out, open('问题扫描/_cache/v20_defs.json','w'))
cnt=collections.Counter(os.path.basename(o[0]) for o in out)
for k,v in cnt.most_common(25): print(' ',k,v)
