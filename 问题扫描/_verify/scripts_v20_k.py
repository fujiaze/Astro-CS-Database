
import re
lines=open('lib/core/src/module_adapters.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(1390,1445)))
