
lines=open('lib/phase3_session/p3_session.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- 385-415 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(384,415)))
print()
import re
print('--- opath definition ---')
for i,l in enumerate(lines,1):
    if 'opath' in l: print(i, l.strip()[:120])
