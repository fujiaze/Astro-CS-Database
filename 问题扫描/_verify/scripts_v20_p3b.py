
lines=open('lib/phase3_session/p3_session.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- 133-160 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(132,160)))
print('--- 368-384 ---')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(367,384)))
