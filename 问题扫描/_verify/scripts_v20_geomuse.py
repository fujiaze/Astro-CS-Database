
import re
src=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='replace').read().split('\n')
for i,l in enumerate(src,1):
    if re.search(r'\b(geom|g)->?(projection|frame|coverage_output|bitpix|sampler|parity)\b', l) or re.search(r'\b(g|geom)\.(projection|frame|coverage_output|bitpix|sampler|parity)\b', l):
        print(f'{i}: {l.strip()[:190]}')
