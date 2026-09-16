
lines=open('lib/calibration/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(759,790)))
print('--- callers 898-925 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(897,926)))
