
import re, os
for k in ['union_cells','artifact_bin','tile_leaf_span','frame_slots','sample_mask_offset','uncertainty_available','weight_mode','legacy_allow_weight_fallback']:
    w=[]; r=[]
    for root in ['lib','cli','runtime']:
        for dp,dn,fn in os.walk(root):
            if 'third_party' in dp or 'build' in dp: continue
            for f in fn:
                if not f.endswith(('.c','.cpp','.h')): continue
                p=os.path.join(dp,f)
                for i,l in enumerate(open(p,encoding='utf-8',errors='ignore').read().split('\n'),1):
                    if k not in l: continue
                    if re.search(r'\{"'+k+r'"\s*,|\["'+k+r'"\]\s*=|"%s"\s*:' % k, l): w.append('%s:%d'%(p,i))
                    else: r.append('%s:%d'%(p,i))
    print('%-28s WRITE(%d)=%s | READ(%d)=%s' % (k, len(w), w[:4], len(r), r[:4]))
