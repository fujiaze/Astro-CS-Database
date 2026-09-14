
import json
s = json.load(open('ci/checks.schema.json', encoding='utf-8'))
print('SCHEMA KEYS:', list(s.keys()))
def walk(o, pre=''):
    if isinstance(o, dict):
        for k,v in o.items():
            if k=='properties' and isinstance(v,dict):
                for pk,pv in v.items():
                    print(pre+'PROP', pk, pv.get('type'), pv.get('default'), str(pv.get('description'))[:80])
            elif k in ('additionalProperties','required'):
                print(pre+k, ':', v)
            elif isinstance(v,(dict,list)):
                walk(v, pre+'  ')
walk(s)
