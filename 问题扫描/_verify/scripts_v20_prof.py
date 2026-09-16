
import re, os
pats = ['wbpp_current','wbpp_2_9_1','reject_profile']
for root in ['lib','cli','docs','runtime','tests','tools']:
    for dp,dn,fn in os.walk(root):
        if 'third_party' in dp or 'build' in dp: continue
        for f in fn:
            if not f.endswith(('.c','.cpp','.h','.hpp','.md','.py','.json','.yaml')): continue
            p=os.path.join(dp,f)
            try: t=open(p,encoding='utf-8',errors='ignore').read()
            except Exception: continue
            for pat in pats:
                if pat in t:
                    for i,l in enumerate(t.split('\n'),1):
                        if pat in l:
                            print('%-52s %-14s %d: %s' % (p,pat,i,l.strip()[:120]))
