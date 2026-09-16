
snr=open('lib/snr_estimator/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- snr 763-776 ---')
print('\n'.join('%d: %s' % (i+1, snr[i].rstrip()[:100]) for i in range(762,776)))
p=open('cli/parser.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('--- parser 66-100 (kHelp) ---')
print('\n'.join('%d: %s' % (i+1, p[i].rstrip()[:96]) for i in range(65,100)))
