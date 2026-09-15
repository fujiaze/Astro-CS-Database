
import json,sys
d=json.load(open('ci/checks.json',encoding='utf-8'))
seen=[]
for c in d['checks']:
    cmd=c['command']
    ex=None
    for i,a in enumerate(cmd):
        if a.endswith('.py'): ex=cmd[i:]
    seen.append((c['id'], ' '.join(cmd)[:110], c.get('waivable'), ','.join(c.get('profiles',[]))))
print(len(d['checks']),'checks')
for s in seen: print('%-28s %-110s waivable=%s [%s]'%s)
