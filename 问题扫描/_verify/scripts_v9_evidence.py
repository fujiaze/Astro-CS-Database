import json, glob, os, collections
runs=sorted(glob.glob('artifacts/ci/*/*/checks'))
print('evidence runs:', len(runs))
latest={}
for rd in runs:
    cid=os.path.basename(rd)
    for f in glob.glob(os.path.join(rd,'*.json')):
        try: d=json.load(open(f,encoding='utf-8'))
        except Exception: continue
        gid=d.get('id') or os.path.basename(f)[:-5]
        t=d.get('started_utc') or ''
        if gid not in latest or t > latest[gid][0]:
            latest[gid]=(t, d.get('verdict'), rd, d.get('exit_code'))
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('gates total', len(reg))
none=[c['id'] for c in reg if c['id'] not in latest]
print('GATES WITH NO EXECUTION EVIDENCE EVER:', len(none)); print('  ', none)
print()
ver=collections.Counter(v[1] for v in latest.values())
print('latest-verdict histogram:', dict(ver))
print()
rows=sorted(latest.items(), key=lambda kv: kv[1][0])
print('oldest 15 latest-evidence timestamps:')
for k,v in rows[:15]: print('  %-30s %s %-22s exit=%s' % (k, v[0], v[1], v[3]))
print()
print('all non-PASS latest evidence:')
for k,v in sorted(latest.items()):
    if v[1]!='PASS': print('  %-30s %s %-22s exit=%s dir=%s' % (k, v[0], v[1], v[3], v[2].split('/')[1]))
