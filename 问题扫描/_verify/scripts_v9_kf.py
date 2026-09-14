import json, os, re, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
kf = json.load(open('ci/known_failures.json', encoding='utf-8'))
print('KF TOPKEYS:', list(kf.keys()))
print(json.dumps(kf, ensure_ascii=False, indent=1)[:6000])
