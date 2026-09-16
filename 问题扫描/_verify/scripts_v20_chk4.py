
import re, hashlib
t=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read()
print('sha256-12', hashlib.sha256(t.encode()).hexdigest()[:12], 'lines', t.count(chr(10))+1)
ma=t.split('\n')
for pat in [r'g->frame|r'+'', r'wr\.value\("module_build_id"', r'Json wr\{', r'wr = ', r'p3_writer\.json', r'g\.bitpix|g->bitpix', r'\{"bitpix", g', r'\{"sampler", g', r'\{"nside", |nside_source', r'g\.projection']:
    pass
for pat in [r'wr\.value\("module_build_id"', r'p3_writer\.json', r'\{"bitpix", g\.bitpix', r'\{"sampler", g\.sampler', r'g\.projection\.c_str', r'g->frame', r'g->coverage_output', r'g->projection']:
    print('###', pat)
    for i,l in enumerate(ma,1):
        if re.search(pat,l): print('   %5d %s' % (i,l.strip()[:105]))
print('### 2930-2935 nside check + 2924-2930 mode vocab:')
print('\n'.join('%d: %s' % (i+1, ma[i].strip()[:100]) for i in range(2923,2936)))
