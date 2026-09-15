
import re, os, json, collections
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
# targeted: cross-module ABI headers
HDRS=[]
for dp,dn,fn in os.walk('include'):
    for f in fn:
        if f.endswith(('.h','.hpp')): HDRS.append(os.path.join(dp,f))
print('include headers:', len(HDRS))
# gather all source files for read/write scan
SRC=[]
for root in ['lib','cli','runtime','providers','modules','tests','tools','scripts','include']:
    for dp,dn,fn in os.walk(root):
        if 'third_party' in dp: continue
        for f in fn:
            if f.endswith(('.c','.cpp','.cc','.h','.hpp')): SRC.append(os.path.join(dp,f))
blobs={p:strip(open(p,encoding='utf-8',errors='replace').read()) for p in SRC}
print('source files:', len(SRC))

sre=re.compile(r'typedef\s+struct[^{;]*\{([^}]*)\}\s*([A-Za-z0-9_]+)\s*;', re.S)
rows=[]
for hp in HDRS:
    t=strip(open(hp,encoding='utf-8',errors='replace').read())
    for m in sre.finditer(t):
        body=m.group(1); tag=m.group(2)
        fields=[]
        for line in body.split('\n'):
            line=line.strip()
            if not line or line.startswith('#') or line.startswith('/*'): continue
            fm=re.match(r'^(?!struct|const|unsigned|signed)([A-Za-z_][\w:<>,\[\]\s\*]*?)([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?\s*(?:=[^;]*)?;\s*(?://.*)?$', line)
            if fm: fields.append(fm.group(2))
        if not fields: continue
        rows.append((hp,tag,fields))
print('structs parsed in include/:', len(rows))
out=[]
for hp,tag,fields in rows:
    for f in fields:
        wr=[]; rd=[]
        pat_w=re.compile(r'(?:->|\.)\s*'+re.escape(f)+r'\s*(?:=[^=]|\+\+|--|\+=|\|=)')
        pat_r=re.compile(r'(?:->|\.)\s*'+re.escape(f)+r'\b')
        init=re.compile(r'[.\{]\s*'+re.escape(f)+r'\s*[:=]')
        for p,b in blobs.items():
            if p==hp: continue
            nw=len(pat_w.findall(b))+len(init.findall(b))
            nr=len(pat_r.findall(b))
            if nw: wr.append(p)
            if nr: rd.append(p)
        if (wr and not rd) or (rd and not wr):
            out.append({'header':hp,'struct':tag,'field':f,'writers':len(wr),'readers':len(rd),'w_files':sorted(set(wr))[:3],'r_files':sorted(set(rd))[:3]})
print('asymmetric fields (write-only or read-only):', len(out))
json.dump(out, open('问题扫描/_cache/v20_fieldsdiff.json','w'), indent=0)
ro=[o for o in out if o['writers']==0]
wo=[o for o in out if o['readers']==0]
print('READ-BUT-NEVER-WRITTEN:', len(ro))
for o in ro: print('   ', o['header'],'::',o['struct'],'.',o['field'],' readers=',o['readers'], o['r_files'][:3])
print('WRITTEN-BUT-NEVER-READ:', len(wo))
for o in wo[:60]: print('   ', o['header'],'::',o['struct'],'.',o['field'],' writers=',o['writers'], o['w_files'][:3])
