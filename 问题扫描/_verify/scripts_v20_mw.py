
import re, os
for k in ['min_workers','max_workers']:
    print('=== %s: consumers outside module entries ===' % k)
    for root in ['lib','cli','runtime','providers','modules','tools']:
        for dp,dn,fn in os.walk(root):
            if 'third_party' in dp or 'build' in dp or '/tests' in dp.replace(os.sep,'/'): continue
            for f in fn:
                if not f.endswith(('.c','.cpp','.h','.py')): continue
                p=os.path.join(dp,f)
                for i,l in enumerate(open(p,encoding='utf-8',errors='ignore').read().split('\n'),1):
                    if k in l and 'module_entry' not in p:
                        print('  %s:%d %s' % (p,i,l.strip()[:110]))
