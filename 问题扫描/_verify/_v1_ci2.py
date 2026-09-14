import json
d=json.load(open('ci/checks.json',encoding='utf-8'))
out=[]
def walk(o,ident=None):
    if isinstance(o,dict):
        cur=o.get('id',ident)
        for k,v in o.items():
            if k=='ctest_targets':
                out.append((cur,v))
            walk(v,cur)
    elif isinstance(o,list):
        for v in o: walk(v,ident)
walk(d)
print('nodes with ctest_targets:',len(out))
import fnmatch
names=['p1snr_science_units','p1snr_science_oracle','p1snr_science_negative','p1snr_science_production','p1snr_science_determinism','p1noise_units','p1snr_linux_a'] 
for ident,v in out:
    if isinstance(v,str): v=[v]
    matched=[n for n in names if any(fnmatch.fnmatch(n,g) for g in v)]
    if matched or any('snr' in str(g).lower() or 'p1noise' in str(g).lower() for g in v):
        print(ident, v, '=>MATCH', matched)