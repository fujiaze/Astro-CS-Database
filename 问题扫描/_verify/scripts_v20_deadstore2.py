
import re, os
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
FILES=['lib/snr_estimator/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp',
       'lib/gaia_xpsd_client/src/module_entry.c','lib/calibration/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp']
for p in FILES:
    t=strip(open(p,encoding='utf-8',errors='replace').read())
    fields={}
    for m in re.finditer(r'typedef\s+struct[^{;]*\{([^}]*)\}\s*([A-Za-z0-9_]+)\s*;', t, re.S):
        tag=m.group(2)
        for line in m.group(1).split('\n'):
            fm=re.match(r'^\s*(?:const\s+)?[A-Za-z_][\w:<>,\[\]\s\*]*?([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*(?://.*)?$', line)
            if fm: fields.setdefault(fm.group(1),[]).append(tag)
    dead=[]
    for f,tags in sorted(fields.items()):
        w=len(re.findall(r'(?:->|\.)\s*'+re.escape(f)+r'\s*(?:=[^=]|\+\+|--|\+=)', t))
        init=len(re.findall(r'[.{]\s*'+re.escape(f)+r'\s*[:=]', t))
        r=len(re.findall(r'(?:->|\.)\s*'+re.escape(f)+r'\b', t))
        tw=w+init
        if tw>0 and r-tw<=0: dead.append((f,tags,tw,r))
    print(f'=== {p}: fields={len(fields)} WRITE-ONLY={len(dead)}')
    for f,tags,tw,r in dead: print(f'    {f} ({tags}) writes={tw} refs={r}')
