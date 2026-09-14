import json
d=json.load(open('ci/checks.json',encoding='utf-8'))
out=[]
def walk(o,ident=None):
    if isinstance(o,dict):
        cur=o.get('id',ident)
        for k,v in o.items():
            if k=='ctest_targets': out.append((cur,v))
            walk(v,cur)
    elif isinstance(o,list):
        for v in o: walk(v,ident)
walk(d)
for ident,v in out: print(ident, v)