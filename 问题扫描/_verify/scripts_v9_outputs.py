import json, os
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('== declared outputs NOT mentioned in command ==')
susp=[]
for c in reg:
    outs=c.get('outputs') or []
    if not outs: continue
    cmd=' '.join(map(str,c['command']))
    for o in outs:
        if o not in cmd:
            susp.append((c['id'], o, cmd[:150], c['waivable']))
for s in susp: print('  ', s)
print('COUNT', len(susp))
print()
print('== outputs dir existence (non-run) ==')
for c in reg:
    for o in (c.get('outputs') or []):
        if not o.startswith('run/'):
            print('  ', c['id'], o, 'exists=', os.path.exists(o), 'ignored-check(dirty_ignore)=', bool(c.get('dirty_ignore_prefixes') or c.get('dirty_ignore_exact')))
print()
print('== gates with dirty_ignore fields ==')
for c in reg:
    if 'dirty_ignore_prefixes' in c or 'dirty_ignore_exact' in c:
        print('  ', c['id'], c.get('dirty_ignore_prefixes'), c.get('dirty_ignore_exact'), 'mutates=',c['mutates_workspace'], 'waivable=',c['waivable'])
