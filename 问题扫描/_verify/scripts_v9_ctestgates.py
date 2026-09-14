import json, glob, os, re
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
tg=[c for c in reg if any('ctest-target' in str(a) for a in c['command'])]
print('ctest-target gates:', len(tg))
def tgt(c):
    i=[str(a) for a in c['command']].index('--target'); return c['command'][i+1]
def outp(c):
    o=[str(a) for a in c['command']]
    return o[o.index('--output')+1] if '--output' in o else None
print('with --output:', sum(1 for c in tg if outp(c)), ' without:', [c['id'] for c in tg if not outp(c)])
print()
print('== any recorded ctest-target evidence anywhere? ==')
hits=[]
for pat in ['run/ci/ctest/*.json','artifacts/**/ctest*.json','run/ci/**/ctest*.json']:
    hits += glob.glob(pat, recursive=True)
hits=sorted(set(hits))
print('candidate files:', len(hits))
nopass=0; pass0=0
for h in hits[:400]:
    try: d=json.load(open(h,encoding='utf-8'))
    except Exception: continue
    steps=d.get('steps') or []
    for s in steps:
        tail=s.get('output_tail','')
        if 'No tests were found' in tail:
            nopass+=1
            print('   NO-TESTS step rc=%s file=%s' % (s.get('exit_code'), h))
print('steps containing No tests were found:', nopass)
print()
print('== sample of 38 target names ==')
for c in tg: print('   %-34s %s' % (c['id'], tgt(c)))
