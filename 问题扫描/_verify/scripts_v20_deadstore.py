
import re, os, collections
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
FILES=['lib/snr_estimator/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp',
       'lib/gaia_xpsd_client/src/module_entry.c','lib/calibration/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp']
for p in FILES:
    t=strip(open(p,encoding='utf-8',errors='replace').read())
    # collect struct field names declared locally: 'type name;' inside typedef struct {...}
    fields=set()
    for m in re.finditer(r'typedef\s+struct[^{]*\{([^}]*)\}([A-Za-z0-9_]+)\s*;', t, re.S):
        for line in m.group(1).split('\n'):
            fm=re.match(r'^\s*(?:const\s+)?[A-Za-z_][\w:<>,\[\]\s\*]*?([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]*)?;\s*$', line)
            if fm: fields.add(fm.group(1))
    dead=[]
    for f in sorted(fields):
        w=len(re.findall(r'(?:->|\.)\s*'+re.escape(f)+r'\s*(?:=[^=]|\+\+|--|\+=)', t))
        r=len(re.findall(r'(?:->|\.)\s*'+re.escape(f)+r'\b', t))
        init=len(re.findall(r'[.{]\s*'+re.escape(f)+r'\s*[:=]', t))
        tot_w=w+init
        if tot_w>0 and r-tot_w<=0:
            dead.append((f,tot_w,r))
    print(f'=== {p}: struct fields={len(fields)}  WRITE-ONLY={len(dead)}')
    for f,tw,tr in dead: print(f'    {f} writes={tw} total-refs={tr}')
