
import re
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for pat in ['pixfrac','precision_mode','auto_nside','nside_conflict','2 的幂|power']:
    print('### %s' % pat)
    for i,l in enumerate(ma,1):
        if re.search(pat,l) and 2900 < i < 3010:
            print('   %5d %s' % (i,l.strip()[:110]))
