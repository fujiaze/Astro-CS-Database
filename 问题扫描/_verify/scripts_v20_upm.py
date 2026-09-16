
lines=open('lib/phase2/src/upm.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- 1285-1350 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(1284,1350)))
