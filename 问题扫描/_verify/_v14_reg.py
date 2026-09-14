
import re,glob,yaml
BT=chr(96)
src=open('../lib/core/src/module_adapters.cpp').read()
desc_ids=sorted(set(re.findall(r'd\\.module_id = "([^"]+)"', src)))
pages=sorted(glob.glob('../docs/modules/registry/astrocs.*.md'))
page_ids=sorted(set(p.split('astrocs.',1)[1][:-3] for p in pages))
yids=[]
for f in glob.glob('../lib/*/module.yaml')+glob.glob('../modules/*/*/module.yaml'):
    d=yaml.safe_load(open(f)); yids.append(d.get('module_id'))
print('descriptors:',len(desc_ids),'pages:',len(page_ids))
print('desc-only:',sorted(set(desc_ids)-set('astrocs.'+i for i in page_ids)))
print('page-only:',sorted(set('astrocs.'+i for i in page_ids)-set(desc_ids)))
# sequential parse of descriptor functions for module_id -> execution_class
pairs=[]
cur=None
for line in src.splitlines():
    m=re.search(r'd\\.module_id = "([^"]+)"',line)
    if m: cur=m.group(1)
    m2=re.search(r'd\\.execution_class = "([^"]+)"',line)
    if m2 and cur: pairs.append((cur,m2.group(1)))
dm=dict(pairs)
print()
for p in pages:
    pid=p.split('astrocs.',1)[1][:-3]
    t=open(p).read()
    m=re.search(r'execution_class=.([a-z_]+)', t)
    page_ec=m.group(1) if m else ('NONE' if 'execution_class' not in t else 'UNK')
    ec=dm.get('astrocs.'+pid)
    flag='OK' if ec==page_ec else 'MISMATCH' if ec else 'NO-DESC'
    print(f'{pid:32s} page={page_ec:10s} desc={str(ec):10s} {flag}')
