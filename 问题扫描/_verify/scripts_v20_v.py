
import re
def show(fp, a, b, tag):
    lines=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    print('=== %s (%s) ===' % (tag, fp))
    print('\n'.join('%d: %s' % (i+1, lines[i].rstrip()[:120]) for i in range(a-1, min(b,len(lines)))))
    print()
show('lib/core/src/module_adapters.cpp',5846,5860,'fits_path def')
show('lib/hips/src/module_entry.cpp',660,672,'hips plan json')
show('lib/snr_estimator/src/module_entry.cpp',556,578,'snr plan json')
show('lib/drizzle/src/module_entry.cpp',302,318,'drz_cfg struct')
show('lib/drizzle/include/astrocs/drizzle/types.h',60,70,'rev key macros')
show('lib/snr_estimator/src/module_entry.cpp',348,356,'noise_cfg cfg_present decl')
