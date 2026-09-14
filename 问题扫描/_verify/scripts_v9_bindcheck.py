import json, os
c=json.load(open('docs/algorithms/anchors/anchor_contract.json',encoding='utf-8'))
b=c['bindings']
print('bindings:', len(b))
for x in b:
    if x.get('id','').startswith(('PLATESOLVE','NOISE-MIN','P3RSMP-DESC')):
        print('  ', json.dumps(x, ensure_ascii=False))
print()
# verify one: PLATESOLVE-CREATE
tgt=[x for x in b if x.get('id')=='PLATESOLVE-CREATE'][0]
doc, target, sym = tgt['doc'], tgt['target'], tgt['symbol']
print('doc=',doc,'exists=',os.path.isfile(doc))
print('target=',target,'exists=',os.path.isfile(target))
lines=open(target,encoding='utf-8',errors='replace').read().splitlines()
hits=[i for i,l in enumerate(lines,1) if sym in l]
print('symbol %r occurrences in target:'%sym, hits[:10], 'target lines=', len(lines))
import re
doclines=open(doc,encoding='utf-8',errors='replace').read().splitlines()
pat=re.compile(re.escape(os.path.basename(target))+r'[:#]L?(\d+)')
print('anchors in doc pointing at %s:'%os.path.basename(target))
for i,l in enumerate(doclines,1):
    for m in pat.finditer(l):
        print('   doc line %d: %s' % (i, l.strip()[:140]))
        break
