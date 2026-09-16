
ma=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, ma[i].rstrip()[:98]) for i in range(5259,5300)))
