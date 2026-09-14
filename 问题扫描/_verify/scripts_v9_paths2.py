import json, os
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
import re
# gates whose command reads docs/** or ci/** or tools/** ; compare with changed_paths prefixes
def g2re(g):
    out=''; i=0
    while i<len(g):
        if g.startswith('**/',i): out+='(?:.*/)?'; i+=3
        elif g.startswith('/**',i): out+='(/.*)?'; i+=3
        elif g.startswith('**',i): out+='.*'; i+=2
        elif g=='*': out+='[^/]*'; i+=1
        else: out+=re.escape(g[i]); i+=1
    return re.compile('^'+out+'$')
def gmatch(p,pats):
    p=p.lstrip('./'); return any(g2re(x).match(p) for x in pats)
bad=[]
for c in reg:
    cp=c['changed_paths']
    cmd=[str(a) for a in c['command']]
    reads=[a for a in cmd if a.startswith('docs/') or a.startswith('ci/') or a.endswith('.py')]
    for rpath in reads:
        if not gmatch(rpath,cp):
            bad.append((c['id'], rpath, ','.join(cp)))
print('GATES whose own invoked target is NOT matched by its changed_paths (--focus would skip it):', len(bad))
seen=set()
for b in bad:
    if b[0] in seen: continue
    seen.add(b[0])
    print('  %-30s 未覆盖 %-52s paths=%s' % b)
print()
print('sample of gates reading docs/:')
for c in reg:
    if any(str(a).startswith('docs/') for a in c['command']):
        print('  %-28s %s' % (c['id'], ','.join(c['changed_paths'])))
