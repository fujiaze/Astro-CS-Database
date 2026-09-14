
import json, os
d=json.load(open('testdata/index.json', encoding='utf-8'))
for ds in d['datasets'][:3]:
    print(ds['id'], '|', ds.get('lights_dir'), '|', os.path.isdir(ds.get('lights_dir') or ''))
p=d['datasets'][0].get('lights_dir')
if p and os.path.isdir(p):
    f=[x for x in os.listdir(p) if x.lower().endswith(('.fits','.fts'))][:3]
    print('sample files:', f)
