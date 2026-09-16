
spec = [('lib/hips/src/module_entry.cpp',609),('lib/drizzle/src/module_entry.cpp',540),
        ('lib/gaia_xpsd_client/src/module_entry.c',499),('lib/phase2/src/upm.cpp',69),
        ('lib/phase2/src/upm.cpp',1298),('lib/backend_host/profile_gen_v2.cpp',337),
        ('lib/phase1/noise/snr_frame_science.cpp',69),('lib/calibration/src/module_entry.cpp',619)]
for f,n in spec:
    lines=open(f,encoding='utf-8',errors='ignore').read().split('\n')
    print('=== %s:%d ===' % (f,n))
    print('\n'.join(lines[n-3:n+9]))
    print()
