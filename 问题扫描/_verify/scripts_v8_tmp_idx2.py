
import json, re
d=json.load(open('testdata/index.json', encoding='utf-8'))
ds0=d['datasets'][0]
print('dataset keys:', list(ds0.keys()))
tot=0
for ds in d['datasets']:
    for k,v in ds.items():
        if isinstance(v,list) and v and isinstance(v[0],(str,dict)):
            print(' ', ds.get('id'), k, len(v))
    for k in ('frame_count','n_frames'):
        if k in ds and isinstance(ds[k], int):
            tot+=ds[k]
print('sum ints:', tot)
s=json.dumps(d, ensure_ascii=False)
print('.fits mentions:', len(re.findall(r'\.fits', s)), ' .fts:', len(re.findall(r'\.fts', s)))
