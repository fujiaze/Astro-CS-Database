
import json, os, collections
p='testdata/index.json'
print('exists', os.path.exists(p))
d=json.load(open(p, encoding='utf-8'))
print('toplevel type', type(d), (list(d)[:6] if isinstance(d,dict) else len(d)))
def count_frames(d):
    if isinstance(d, list): return len(d)
    if isinstance(d, dict):
        for k,v in d.items():
            if isinstance(v, list):
                print('  list key:', k, len(v))
        return None
count_frames(d)
# frames summary if entries have name/target fields
if isinstance(d, dict):
    for key in ('frames','entries','files','items','datasets'):
        if key in d and isinstance(d[key], list):
            arr=d[key]; print(key, len(arr)); print(json.dumps(arr[0], ensure_ascii=False)[:300]); break
