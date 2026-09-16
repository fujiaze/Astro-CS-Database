
import json
p = '问题扫描/_cache/v20_params3.json'
d = json.load(open(p))
print(type(d))
if isinstance(d, dict):
    ks = list(d)
    print('keys sample:', ks[:6])
    print('repr first:', json.dumps(d[ks[0]], ensure_ascii=False)[:500])
else:
    print('len', len(d))
    print(json.dumps(d[0], ensure_ascii=False)[:500])
