
lines=open('lib/calibration/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
for n in (1160, 1208, 1285, 1333, 594, 616):
    print('--- %d ---' % n)
    print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(n-1, n+13)))
    print()
print('=== cfg_fill head ===')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(575, 600)))
