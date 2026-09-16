
spec = [('lib/hips/src/module_entry.cpp',300,340),('lib/hips/src/module_entry.cpp',425,445),
        ('lib/drizzle/src/module_entry.cpp',380,400),('lib/gaia_xpsd_client/src/module_entry.c',255,275),
        ('lib/snr_estimator/src/module_entry.cpp',376,400)]
for f,a,b in spec:
    lines=open(f,encoding='utf-8',errors='ignore').read().split('\n')
    print('=== %s %d-%d ===' % (f,a,b))
    print('\n'.join('%d: %s' % (i+1, lines[i]) for i in range(a-1,b)))
    print()
