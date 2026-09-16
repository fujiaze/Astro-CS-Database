
import re
lines=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for f in ['projection','frame','coverage_output','sampler','parity','bitpix','hips_dir','out_dir','ra','dec','scale','w','h']:
    pat = re.compile(r'\bg[.-]>' if False else r'\bg(?:\.|->)' + f + r'\b')
    sites = [(i+1, lines[i].strip()[:100]) for i,l in enumerate(lines) if pat.search(l)]
    print('== %-16s %d hits' % (f, len(sites)))
    for n,s in sites[:10]:
        print('    %d: %s' % (n,s))
