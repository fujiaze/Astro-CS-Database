import json
reg={c['id']:c for c in json.load(open('ci/checks.json',encoding='utf-8'))['checks']}
for cid in ('PRODUCTION-GRAPH','ISA-LEAK-SELFTEST','SERIAL-HEAVY-SELFTEST','PROD-REACH-SELFTEST','LOG-CONTRACT-SELFCHECK','TESTKIT-LIST','API-DOCS','UNIT-CLOSURE','DUPLICATION','CONTRACT-GRAPH'):
    c=reg.get(cid)
    if c: print('%-24s prof=%-46s waiv=%-5s outputs=%s' % (cid, ','.join(c['profiles']), c['waivable'], c['outputs']))
