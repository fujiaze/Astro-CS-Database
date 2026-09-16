
import re, os
for k in ['noise_sigma','psf_mode','n_fit_input','psf_fit_truncated','photometry_applied','tile_leaf_span','union_cells']:
    print('=== %s ===' % k)
    for root in ['lib','cli','runtime']:
        for dp,dn,fn in os.walk(root):
            if 'third_party' in dp or 'build' in dp: continue
            for f in fn:
                if not f.endswith(('.c','.cpp','.h')): continue
                p=os.path.join(dp,f)
                for i,l in enumerate(open(p,encoding='utf-8',errors='ignore').read().split('\n'),1):
                    if k in l:
                        kind = 'WRITE' if re.search(r'[\{"\[]\s*"'+k+r'"\s*,|\["'+k+r'"\]\s*=|"%s"\s*:' % k, l) else 'read '
                        print('  %s %s:%d %s' % (kind,p,i,l.strip()[:100]))
