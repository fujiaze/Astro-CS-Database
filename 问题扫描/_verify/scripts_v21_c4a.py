
import json,re
m=json.load(open('packaging/astrocs.product.json',encoding='utf-8'))
us=m.get('units',[])
print('units',len(us))
print('keysets',sorted({k for u in us for k in u}))
print('sha vals',{repr(u.get('sha256')) for u in us})
print('build_id vals',{repr(u.get('build_id')) for u in us})
print('abi vals',{repr(u.get('abi_version')) for u in us})
s=open('cmake/astrocs.product.windows.json.in',encoding='utf-8').read()
print('IN keys',re.findall(r'"(sha256|build_id|abi_version|source_commit|product_version)"\s*:\s*([^,}\n]*)',s))
