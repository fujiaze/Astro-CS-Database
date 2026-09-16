
import re
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for pat in [r'\{"nside_mode"', r'snr_max_sources"\]', r'\{"snr_max_sources"', r'snr_definition|reason|snr_catalogue_reason', r'sigma_location_se_status', r'hot_cold|reject' ]:
    print('### %s' % pat)
    for i,l in enumerate(ma,1):
        if re.search(pat,l) and 2600 < i < 3300:
            print('   %5d %s' % (i,l.strip()[:110]))
print('### 5878-5886:')
print('\n'.join('%d: %s' % (i+1, ma[i].strip()[:100]) for i in range(5877,5886)))
