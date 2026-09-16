
import re
lines=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
src='\n'.join(lines)
for f in ['frame','coverage_output','projection','sampler','parity','bitpix','max_tiles','hips_dir','out_dir']:
    w=[i+1 for i,l in enumerate(lines) if re.search(r'g->'+f+r'\b\s*=', l)]
    r=[i+1 for i,l in enumerate(lines) if re.search(r'g->'+f+r'(?!\s*=[^=])', l) and i+1 not in w]
    print('%-16s writes=%s reads=%s' % (f, w, r[:8]))
print()
print('--- P3nGeom struct def ---')
for i,l in enumerate(lines):
    if 'struct P3nGeom' in l:
        print('\n'.join('%d: %s' % (j+1, lines[j]) for j in range(i, i+22)))
        break
