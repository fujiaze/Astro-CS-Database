import json, glob
base='artifacts/ci/48ceee59fe52/20260913T210003Z-766d84ce/checks'
for cid in ['UT-BACKEND','KNOWN-FAILURES-BASELINE-CHECK','UT-CLI','UT-CPU-AVX512','TRACEABILITY-MATRIX','CON-TRACEABILITY','CTEST-REGISTRATION']:
    try:
        d=json.load(open(base+'/'+cid+'.json',encoding='utf-8'))
    except Exception as e:
        print('##', cid, 'READ-ERR', e); continue
    print('##', cid, '=>', d.get('verdict'), '| exit', d.get('exit_code'))
    st=(d.get('stdout_tail') or '')[-1200:]
    se=(d.get('stderr_tail') or '')[-600:]
    print('  STDOUT_TAIL:', st.replace('\n','\n    ')[:1200])
    if se: print('  STDERR_TAIL:', se.replace('\n','\n    ')[:600])
    print()
