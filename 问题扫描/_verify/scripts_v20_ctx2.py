
import re
src=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='replace').read().split('\n')
def show(k, n=8):
    print('##### '+k)
    for i,l in enumerate(src,1):
        if '"'+k+'"' in l: print(f'  {i}: {l.strip()[:190]}')
for k in ['input_manifest_hash','reject_profile','max_sources','enabled','tile_leaf_span','depth','frame_slots','sample_mask_offset','n_pixels','nrej','accepted','max_tiles','sampler','longitude_parity','coverage_output','bitpix']:
    show(k)
