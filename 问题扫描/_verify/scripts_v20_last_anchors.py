
import re
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('### module_build_id lines:')
for i,l in enumerate(ma,1):
    if 'module_build_id' in l: print('   %5d %s' % (i,l.strip()[:100]))
print('### p3n_input_manifest_hash / out_dir fits:')
for i,l in enumerate(ma,1):
    if 'p3n_input_manifest_hash' in l or 'fits_path = ' in l: print('   %5d %s' % (i,l.strip()[:100]))
print('### validate_config region 4832-4870:')
print('\n'.join('%d: %s' % (i+1, ma[i].strip()[:100]) for i in range(4831,4870)))
