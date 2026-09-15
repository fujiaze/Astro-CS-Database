
import re, os, json, collections
# 1) collect struct field names from headers that define cross-module structs
HDR=[]
for root in ['include','lib','runtime','providers','cli','modules']:
    for dp,dn,fn in os.walk(root):
        if 'third_party' in dp: continue
        for f in fn:
            if f.endswith(('.h','.hpp')): HDR.append(os.path.join(dp,f))
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
structs={}
sre=re.compile(r'typedef\s+struct\s*([A-Za-z0-9_]*)\s*\{([^}]*)\}\s*([A-Za-z0-9_]+)\s*;', re.S)
sre2=re.compile(r'\bstruct\s+([A-Za-z0-9_]+)\s*(?::\s*[A-Za-z0-9_]+)?\s*\{([^}]*)\}', re.S)
for p in HDR:
    t=strip(open(p,encoding='utf-8',errors='replace').read())
    for m in list(sre.finditer(t))+list(sre2.finditer(t)):
        tag=m.group(1) or m.group(3) or '?'
        body=m.group(2) if m.re is sre2 else m.group(2)
        fields=[]
        for line in body.split('\n'):
            line=line.strip()
            if not line or line.startswith('#'): continue
            fm=re.match(r'((?:const\s+|unsigned\s+|struct\s+)*[A-Za-z_][\w \*\[\]]*?)\b([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?\s*(?:=\s*[^;]+)?;\s*(?://.*)?$', line)
            if fm: fields.append(fm.group(2))
        if fields:
            structs.setdefault(p,[]).append((tag,fields))
print('headers with structs:', len(structs))
n=sum(len(v) for v in structs.values())
print('structs total:', n, ' fields total:', sum(len(f) for v in structs.values() for _,f in v))
json.dump({k:[(t,fl) for t,fl in v] for k,v in structs.items()}, open('问题扫描/_cache/v20_structs.json','w'))
