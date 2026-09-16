
for f in ['lib/calibration/CMakeLists.txt','lib/cosmetic/CMakeLists.txt','lib/hips/CMakeLists.txt','lib/drizzle/CMakeLists.txt']:
    t=open(f,encoding='utf-8',errors='ignore').read().split('\n')
    print('=== %s ===' % f)
    for i,l in enumerate(t,1):
        if 'compile_options' in l or 'Wextra' in l or 'Wall' in l or 'Wno' in l:
            print('\n'.join('%d: %s' % (j+1, t[j]) for j in range(i-1, min(i+4,len(t)))))
            print()
