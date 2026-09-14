
import json, os, glob
d=json.load(open('testdata/index.json', encoding='utf-8'))
tot=0; found=0; samples=[]
for ds in d['datasets']:
    ld=ds.get('lights_dir') or ''
    if not os.path.isdir(ld):
        print('MISSING DIR', ld); continue
    fs=[]
    for ext in ('*.fits','*.fts','*.FIT','*.fits.gz'):
        fs+=glob.glob(os.path.join(ld,'**',ext), recursive=True)
    tot+=ds.get('lights_count',0)
    print(ds['id'], 'declared', ds.get('lights_count'), 'on-disk', len(fs))
    if fs and len(samples)<2: samples.append(fs[0])
print('declared total', tot)
print('samples', samples)
