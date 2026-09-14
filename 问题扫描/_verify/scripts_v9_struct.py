import json, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
print('TOPKEYS:', list(d.keys()))
for k,v in d.items():
    print(' ', k, type(v).__name__, (len(v) if hasattr(v,'__len__') else v))
ch = d.get('checks') or d.get('items')
print('CHECKS type', type(ch).__name__, len(ch))
if isinstance(ch, list):
    print('SAMPLE0:', json.dumps(ch[0], ensure_ascii=False, indent=1)[:1500])
    keysets = collections.Counter(tuple(sorted(x.keys())) for x in ch)
    print('KEYSETS:')
    for ks,c in keysets.most_common():
        print('  ', c, ks)
