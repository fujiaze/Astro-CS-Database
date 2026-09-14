import json, re, pathlib
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('waivable ids:', [c['id'] for c in reg if c['waivable']])
print()
for c in reg:
    if c['id'] in ('CI-BINDING-TESTS','UT-CPU-AVX512','UT-CLI','CTEST-LINUX-FULL','TRACEABILITY'):
        print(c['id'], '=>', ' '.join(map(str,c['command'])))
        print('    dirty_ignore:', {k:v for k,v in c.items() if 'dirty' in k}, 'waivable', c['waivable'])
print()
# suppression stats for CON-COMMENTS
repo=pathlib.Path('.')
srcs=list((repo/'lib').rglob('*.cpp'))+list((repo/'lib').rglob('*.h'))+list((repo/'lib').rglob('*.hpp'))
srcs=[p for p in srcs if 'third_party' not in str(p) and 'archive' not in str(p)]
n_with_v=0; n_suppressed=0; n_files_freeze=0
for p in srcs:
    t=p.read_text(encoding='utf-8',errors='ignore')
    if 'V19R2' in t or 'V19R3' in t:
        n_with_v+=1
        cs=re.findall(r'//.*|/\*.*?\*/', t, re.S)
        blob=[c for c in cs if 'V19R2' in c or 'V19R3' in c]
        if blob and any('冻结' in b for b in blob): n_suppressed+=1
    if '冻结' in t: n_files_freeze+=1
print('lib 内含 V19R2/V19R3 的文件数:', n_with_v, ' 其中被「冻结」抑制:', n_suppressed)
print('lib 内含「冻结」字样的文件数:', n_files_freeze, '/', len(srcs))
