
import re,sys
keys=['nside_mode','huber_delta','smoothing_lambda','zero_anchor_weight','control_grid_per_tile','max_iterations','persist_upm','upm_save_path','reject_profile','model_hash','input_manifest_hash','profile','tile_leaf_span']
src=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='replace').read().split('\n')
for k in keys:
    print('##### '+k)
    for i,l in enumerate(src,1):
        if '"'+k+'"' in l: print(f'  {i}: {l.strip()[:200]}')
