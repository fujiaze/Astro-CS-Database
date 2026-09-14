import json,re
d=json.load(open('../runtime/pipeline/module_ports.registry.json'))
mods=d['modules']
print('modules is list; n=', len(mods), '| sample:', json.dumps(mods[0], ensure_ascii=False)[:500])
src=open('../lib/core/src/module_adapters.cpp').read()
# fn -> module_id mapping by scanning functions
funcs=[]
for m in re.finditer(r'ModuleDescriptor (\w+_descriptor)\(\)\s*\{', src):
    name=m.group(1); i=m.end()-1; depth=0
    j=i
    while j < len(src):
        if src[j]=='{': depth+=1
        elif src[j]=='}':
            depth-=1
            if depth==0: break
        j+=1
    body=src[i:j]
    mid=re.search(r'd\.module_id = "([^"]+)"', body)
    ec=re.search(r'd\.execution_class = "([^"]+)"', body)
    funcs.append((name, mid.group(1) if mid else '?', ec.group(1) if ec else '?'))
fn2id={n:(mi,ec) for n,mi,ec in funcs}
# cpp node bindings: {xxx_descriptor(), {Enum::Op, "operation", "entry"}}
binds=[]
for m in re.finditer(r'\{([a-z0-9_]+_descriptor)\(\),\s*\{\s*(\w+)::(\w+),\s*"([^"]+)",\s*"([^"]+)"\}\}', src):
    fn=m.group(1); binds.append((fn2id.get(fn,('?','?'))[0], m.group(3), m.group(4), m.group(5)))
binds=sorted(set(binds))
# json index
J={}
for e in mods:
    J[e.get('module_id')]=e
print('cpp bindings:', len(binds), '| json modules:', len(J))
print()
for mid,openum,op,entry in binds:
    je=J.get(mid)
    if je is None:
        print('%-28s %-20s %-30s JSON-MISSING' % (mid, op, entry))
    else:
        jop=je.get('operation') or je.get('operation_name') or (je.get('entry_points') or [{}])[0].get('operation')
        jen=je.get('entry') or je.get('entry_point') or (je.get('entry_points') or [{}])[0].get('entry')
        v='OK' if (jop==op and jen==entry) else 'CHECK'
        print('%-28s %-20s %-32s json: %s | %s | %s' % (mid, op, entry, jop, jen, v))
missing_in_cpp=[k for k in J if k not in [b[0] for b in binds]]
print(); print('json modules with no cpp binding row:', missing_in_cpp)
