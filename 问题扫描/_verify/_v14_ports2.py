import json,re
d=json.load(open('../runtime/pipeline/module_ports.registry.json'))
mods={e['module_id']:e for e in d['modules']}
print('note:', d.get('note','')[:400])
src=open('../lib/core/src/module_adapters.cpp').read()
funcs={}
for m in re.finditer(r'ModuleDescriptor (\w+_descriptor)\(\)\s*\{', src):
    name=m.group(1); i=m.end()-1; depth=0; j=i
    while j < len(src):
        if src[j]=='{': depth+=1
        elif src[j]=='}':
            depth-=1
            if depth==0: break
        j+=1
    body=src[i:j]
    mid=re.search(r'd\.module_id = "([^"]+)"', body)
    ec=re.search(r'd\.execution_class = "([^"]+)"', body)
    par=re.search(r'd\.parallel_ok = (\w+)', body)
    funcs[mid.group(1) if mid else '?']={'ec': ec.group(1) if ec else None, 'par': par.group(1) if par else None}
binds={}
for m in re.finditer(r'\{([a-z0-9_]+_descriptor)\(\),\s*\{\s*(\w+)::(\w+),\s*"([^"]+)",\s*"([^"]+)"\}\}', src):
    fn=m.group(1)
    # find module_id of that fn: rescan
    mm=re.search(r'ModuleDescriptor '+re.escape(fn)+r'\(\)\s*\{', src)
    i=mm.end()-1; depth=0; j=i
    while j<len(src):
        if src[j]=='{': depth+=1
        elif src[j]=='}':
            depth-=1
            if depth==0: break
        j+=1
    mid=re.search(r'd\.module_id = "([^"]+)"', src[i:j]).group(1)
    binds[mid]=(m.group(4), m.group(5))
bad=[]
for mid,(op,entry) in sorted(binds.items()):
    je=mods.get(mid)
    ops=je.get('operations',[])
    # match by operation name
    hit=[o for o in ops if o['operation']==op]
    ent_ok = hit and hit[0].get('entry')==entry
    rc = hit[0].get('resource_class') if hit else None
    ec = funcs.get(mid,{}).get('ec')
    rcmatch = (rc==ec)
    if not (ent_ok and rcmatch and len(ops)==1):
        bad.append((mid, op, entry, len(ops), bool(ent_ok), rc, ec, rcmatch))
    print('%-27s op=%-18s entry_ok=%s n_ops=%d rc_json=%-10s rc_cpp=%-10s rc_match=%s' % (mid,op,ent_ok,len(ops),rc,ec,rcmatch))
print(); print('SUSPECTS:',len(bad))
for b in bad: print(b)
# modules in json whose ops entry != cpp
extra=[k for k in mods if k not in binds]
print('json-only:', extra)
