
import re, os, json, collections
READ=re.compile(r'\.(?:value|contains|count|find|at)\s*\(\s*"([A-Za-z0-9_]+)"')
READ2=re.compile(r'\b(?:p1_int|p1_flag|p1_num|p1_has)\s*\([^,]+,\s*"([A-Za-z0-9_]+)"')
READ3=re.compile(r'\[\s*"([A-Za-z0-9_]+)"\s*\]')            # doc["k"] read or write
WRITE1=re.compile(r'\[\s*"([A-Za-z0-9_]+)"\s*\]\s*(?:\+\+|--|=[^=])')  # doc["k"] = ...
WRITE2=re.compile(r'[\{,]\s*"([A-Za-z0-9_]+)"\s*,')          # {"k", val}
WRITE3=re.compile(r'\"([a-z0-9_]+)\":')                       # \"k\": in snprintf
roots=['lib','cli','runtime','providers','modules','include']
reads=collections.defaultdict(set); writes=collections.defaultdict(set)
files=[]
for root in roots:
    for dp,dn,fn in os.walk(root):
        if 'third_party' in dp: continue
        for f in fn:
            if f.endswith(('.c','.cpp','.cc','.h','.hpp','.json')): files.append(os.path.join(dp,f))
for p in files:
    try: lines=open(p,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,l in enumerate(lines,1):
        code=re.sub(r'//.*$','',l)
        for pat,dd in ((READ,reads),(READ2,reads)):
            for m in pat.finditer(code): dd[m.group(1)].add(f'{p}:{i}')
        if p.endswith(('.c','.cpp','.cc','.h','.hpp')):
            for m in WRITE1.finditer(code): writes[m.group(1)].add(f'{p}:{i}')
            for m in WRITE2.finditer(code): writes[m.group(1)].add(f'{p}:{i}')
            for m in WRITE3.finditer(code): writes[m.group(1)].add(f'{p}:{i}')
json.dump({'reads':{k:sorted(v) for k,v in reads.items()},'writes':{k:sorted(v) for k,v in writes.items()}}, open('问题扫描/_cache/v20_rw.json','w'))
# keys read in module_adapters (node config face)
ma=[k for k in reads if any('module_adapters' in s for s in reads[k])]
print('module_adapters read keys:', len(ma))
print('--- read but NO writer anywhere in lib/cli/runtime/providers/modules/include ---')
for k in sorted(ma):
    if not writes.get(k): print(f'  {k}\t{sorted(reads[k])[:4]}')
