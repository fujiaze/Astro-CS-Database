
import re, os
targets = ['output_fits_path','sampler_used']
for k in targets:
    print('==== %s ====' % k)
    for root in ['docs','contracts','tests','cli','lib','runtime']:
        for dp,dn,fn in os.walk(root):
            if 'third_party' in dp or 'build' in dp: continue
            for f in fn:
                if not f.endswith(('.md','.json','.yaml','.csv','.py','.cpp','.h')): continue
                p=os.path.join(dp,f)
                for i,l in enumerate(open(p,encoding='utf-8',errors='ignore').read().split('\n'),1):
                    if k in l:
                        print('  %s:%d %s' % (p,i,l.strip()[:130]))
