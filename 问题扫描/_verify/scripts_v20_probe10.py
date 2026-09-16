
import re
u=open('lib/phase2/src/upm.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- 214-250 ---')
print('\n'.join('%d: %s' % (i+1, u[i].rstrip()[:96]) for i in range(213,250)))
print('--- field use lines 500-510/625-635/865-875/950-960/1040-1050 ---')
for rng in [(500,512),(625,636),(865,876),(950,962),(1040,1052)]:
    for i in range(rng[0]-1, rng[1]):
        if re.search(r'cfg\.|cfg->', u[i]): print('   %5d %s' % (i+1, u[i].strip()[:92]))
