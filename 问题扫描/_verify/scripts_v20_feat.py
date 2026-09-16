
import re, os
for pat in [r'value\(\s*"mode"', r'contains\(\s*"mode"', r'\["mode"\]', r'value\(\s*"sampler_used"', r'contains\(\s*"sampler_used"', r'\["sampler_used"\]', r'value\(\s*"output_fits_path"', r'contains\(\s*"output_fits_path"']:
    print('###', pat)
    for root in ['lib','cli','runtime']:
        for dp,dn,fn in os.walk(root):
            if 'third_party' in dp or 'build' in dp: continue
            for f in fn:
                if not f.endswith(('.c','.cpp','.h')): continue
                p=os.path.join(dp,f)
                for i,l in enumerate(open(p,encoding='utf-8',errors='ignore').read().split('\n'),1):
                    if re.search(pat,l): print('   %s:%d %s' % (p,i,l.strip()[:110]))
