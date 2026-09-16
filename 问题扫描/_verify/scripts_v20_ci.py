
lines=open('ci/run.py',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(1158,1180)))
print('--- 1205-1235 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(1204,1235)))
