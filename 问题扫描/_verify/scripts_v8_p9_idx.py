
import json, os, glob
d=json.load(open('testdata/index.json', encoding='utf-8'))
allf=[]
for ds in d['datasets']:
    for p in sorted(glob.glob(os.path.join(ds['lights_dir'],'**','*.fts'), recursive=True)):
        allf.append((ds['id'], p))
target=[x for x in allf if '075716' in x[1]]
print('total frames', len(allf))
for t in target:
    print('match:', t[0], t[1])
    # index under dataset-order (as listed) and under global path sort
    print('  dataset-order idx:', allf.index(t))
    sp=sorted(p for _,p in allf)
    print('  global-path-sorted idx:', sp.index(t[1]))
    # per-dataset idx within NGC55
    ng=[x for x in allf if x[0]==t[0]]
    print('  within-dataset idx:', [i for i,x in enumerate(ng) if x[1]==t[1]])
# what is at global idx 561 in each ordering
print('at dataset-order 561:', allf[561] if len(allf)>561 else None)
sp=sorted(p for _,p in allf)
print('at path-sorted 561:', sp[561])
