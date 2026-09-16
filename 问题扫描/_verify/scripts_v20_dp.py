
import re
lines=open('lib/calibration/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for i,l in enumerate(lines,1):
    if 'decode_plane' in l:
        print('--- %d: %s' % (i,l.strip()[:120]))
        if i>600 and i<680:
            pass
print()
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(640,682)))
