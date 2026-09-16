
lines=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(1444,1476)))
print('=== DATA_SEMANTICS dark_scale_factor rows ===')
import re
d=open('docs/contracts/DATA_SEMANTICS.md',encoding='utf-8',errors='ignore').read().split('\n')
for i,l in enumerate(d,1):
    if 'dark_scale_factor' in l: print(i, l[:200])
