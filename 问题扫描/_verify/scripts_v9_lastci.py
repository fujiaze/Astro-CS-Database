import json, glob, os
p='artifacts/ci/48ceee59fe52/20260913T210003Z-766d84ce/CI_RESULT.json'
d=json.load(open(p,encoding='utf-8'))
print('TOPKEYS', list(d.keys()))
for k in ('profile','platform','source_sha','started_utc','finished_utc','verdict','exit_code','summary','selection'):
    if k in d: print(k,'=',json.dumps(d[k],ensure_ascii=False)[:400])
rows = d.get('checks') or d.get('results') or []
print('N checks', len(rows))
import collections
print(collections.Counter(r.get('verdict') for r in rows))
for r in rows:
    if r.get('verdict')!='PASS':
        print('  %-32s %-24s %s' % (r.get('id'), r.get('verdict'), str(r.get('reason'))[:120]))
