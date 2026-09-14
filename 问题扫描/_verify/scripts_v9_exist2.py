import json, os, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
rows=[]
for c in ch:
    cmd = list(map(str, c['command']))
    tgt = None
    if cmd[0]=='python3':
        if len(cmd)>1 and cmd[1]=='-m':
            tgt = '<module:%s>' % cmd[2]
        else:
            i=1
            while i<len(cmd) and cmd[i].startswith('-'): i+=1
            tgt = cmd[i] if i<len(cmd) else None
    elif cmd[0]=='bash': tgt = cmd[1]
    else: tgt = cmd[0]
    ex = (os.path.exists(tgt) if tgt and not tgt.startswith('<') else True)
    rows.append((c['id'], tgt, ex, c['platform'], c['profiles'], c['waivable']))
print('== nonexistent script targets ==')
for r in rows:
    if not r[2]: print('  MISSING:', r)
print('== ctest_targets field ==')
for c in ch:
    if 'ctest_targets' in c:
        print(c['id'], c['ctest_targets'], '| -R in cmd:', '-R' in list(map(str,c['command'])))
