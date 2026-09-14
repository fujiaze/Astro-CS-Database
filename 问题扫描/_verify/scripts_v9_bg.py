import pathlib, re, json, os
repo=pathlib.Path('.')
text=(repo/'docs/architecture/BUILD_GRAPH.md').read_text(encoding='utf-8',errors='ignore') if (repo/'docs/architecture/BUILD_GRAPH.md').exists() else ''
print('BUILD_GRAPH.md exists:', bool(text))
miss=[t for t in ['phase2','astrocs-stage2','calibrated_pair_diag','rejection_cli','P2_ENABLE_OPENMP'] if t not in text]
print('doc missing tokens:', miss)
ct=(repo/'lib/phase2/CMakeLists.txt').read_text(encoding='utf-8',errors='ignore') if (repo/'lib/phase2/CMakeLists.txt').exists() else ''
miss2=[t for t in ['add_library(phase2','add_executable(astrocs-stage2','add_executable(calibrated_pair_diag'] if t not in ct]
miss3=[] if 'target_link_libraries(phase2' in ct else ['link']
print('cmake missing:', miss2+miss3, '| upm.cpp:', (repo/'lib/phase2/src/upm.cpp').exists())
v='FAIL' if (miss or miss2 or miss3 or not (repo/'lib/phase2/src/upm.cpp').exists()) else 'PASS'
print('CON-BUILD-GRAPH STATIC VERDICT:', v)
print()
# EXEC
tb=(repo/'docs/architecture/THREAD_BUDGET_ARCH.md')
f=[]
if not tb.exists(): f.append('EXEC-MISSING-THREADBUDGET')
else:
    t=tb.read_text(encoding='utf-8',errors='ignore')
    for i in ['ARCH-THREAD-001','budget','lease']:
        if i not in t: f.append('EXEC-MISSING-ID:'+i)
    if 'worker' not in t: f.append('EXEC-NO-WORKER-BUDGET')
print('EXEC part1 findings:', f)
