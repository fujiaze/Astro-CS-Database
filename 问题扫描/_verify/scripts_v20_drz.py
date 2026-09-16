
lines=open('lib/drizzle/src/module_entry.cpp',encoding='utf-8',errors='ignore').read().split('\n')
print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(325,378)))
print('---- drz_cfg_free ----')
for i,l in enumerate(lines):
    if 'drz_cfg_free' in l and i<330:
        print('\n'.join('%d: %s' % (j+1, lines[j]) for j in range(i, i+12)))
