
import re
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
def find(pat, lim=8):
    n=0
    for i,l in enumerate(ma,1):
        if re.search(pat,l):
            print('   %5d %s' % (i,l.strip()[:112])); n+=1
            if n>=lim: break
print('### nested check:'); find(r'nested', 6)
print('### nside_mode echo:'); find(r'nside_mode', 8)
print('### snr_max_sources:'); find(r'snr_max_sources', 6)
print('### sci.valid/reason + m5:'); find(r'sci\.valid|frame_depth_m5_mag', 6)
print('### fits_path:'); find(r'std::string fits_path|fits_path =', 4)
p3=open('lib/phase3_session/p3_session.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('### p3_session frame/coverage checks:')
for i,l in enumerate(p3,1):
    if re.search(r'"frame"|"coverage_output"|"icrs"|"mask"', l): print('   %5d %s' % (i,l.strip()[:110]))
