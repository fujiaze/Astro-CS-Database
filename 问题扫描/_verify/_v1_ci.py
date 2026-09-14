import json,re
d=json.load(open('ci/checks.json',encoding='utf-8'))
def walk(o,path=''):
    if isinstance(o,dict):
        for k,v in o.items():
            yield from walk(v,path+'/'+k)
    elif isinstance(o,list):
        for i,v in enumerate(o):
            yield from walk(v,path+f'[{i}]')
    else:
        yield path,o
hits=[]
checks = d if isinstance(d,list) else d.get('checks',d)
print('TOP',type(d), list(d)[:6] if isinstance(d,dict) else len(d))
for path,v in walk(d):
    if path.endswith('/ctest_targets') and isinstance(v,str):
        if 'p1' in v.lower() or 'snr' in v.lower() or '*' in v:
            hits.append((path.split('/')[2] if len(path.split('/'))>2 else path, v))
print('N',len(hits))
for h in hits[:60]: print(h)