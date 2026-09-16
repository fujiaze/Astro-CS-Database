
import re, hashlib
t=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read()
ma=t.split('\n')
for name,pat in [('g.sampler reads',r'g\.sampler|g->sampler'),('g.bitpix call',r'g\.bitpix'),('ac_correct',r'ac_correct_frame'),('max_stars',r'max_stars'),('hp_run',r'hp_drizzle_run'),('filter_read',r'doc\.value\("filter_passband"|"filter_passband", std::string'),('p3_output_call',r'p3_output_write_atomic_ex'),('sampler_echo',r'\{"sampler", g\.sampler\}'),('method_map',r'== "bilinear"|MEDIAN')]:
    print('###', name)
    for i,l in enumerate(ma,1):
        if re.search(pat,l): print('   %5d %s' % (i,l.strip()[:105]))
