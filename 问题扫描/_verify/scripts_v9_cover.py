import json, re
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
def g2re(g):
    out=''; i=0
    while i<len(g):
        if g.startswith('**/',i): out+='(?:.*/)?'; i+=3
        elif g.startswith('/**',i): out+='(/.*)?'; i+=3
        elif g.startswith('**',i): out+='.*'; i+=2
        elif g=='*': out+='[^/]*'; i+=1
        else: out+=re.escape(g[i]); i+=1
    return re.compile('^'+out+'$')
def gm(p,pats): return any(g2re(x).match(p) for x in pats)
ci=[c['id'] for c in reg if gm('ci/checks.json', c['changed_paths'])]
print('gates whose changed_paths DO cover ci/checks.json (%d):'%len(ci), ci)
static=[c['id'] for c in reg if not any(str(a).startswith(('run/','artifacts/')) or 'cmake' in str(a) or 'ctest' in str(a) or 'unittest' in str(a) or 'wf_step' in str(a) or 'resource_monitor' in str(a) for a in c['command'])]
print()
print('纯静态 checker 门数:', len(static))
recomputed={'CON-TRACEABILITY','TRACEABILITY-MATRIX','DOC-LINE-ANCHORS','CON-API-CONTRACTS','CON-BUILD-GRAPH','CON-COMMENTS','CON-CONFIG-CONTRACTS','CON-SCIENCE-UNITS','CON-TEST-CONTRACTS','CTEST-REGISTRATION','KNOWN-FAILURES-BASELINE-VERIFY','TRACEABILITY','CON-FULL-INTEGRATION'}
print('未逐一复算的静态门 (%d):'%(len([x for x in static if x not in recomputed])))
print('  ', [x for x in static if x not in recomputed])
