
import json
d=json.load(open('testdata/index.json', encoding='utf-8'))
tot=0
for ds in d['datasets']:
    lc=ds.get('lights_count')
    print(ds.get('id'), 'lights_count=', lc, 'by_panel_filter len=', len(ds.get('lights_by_panel_filter') or []))
    if isinstance(lc,int): tot+=lc
print('SUM lights_count =', tot)
# also sum nested counts if available
sub=0
for ds in d['datasets']:
    for e in (ds.get('lights_by_panel_filter') or []):
        if isinstance(e, dict):
            for k,v in e.items():
                if isinstance(v,int): sub+=v
print('nested sum =', sub)
