
import json, numpy as np, sys
for lbl,p in (('OLD','run/RELEASE-02/L4-rebuild/bitref_16w/p2_upm_model.bin'),
              ('NEW','run/RELEASE-02/L4-rebuild/upmfix_out/p2_upm_model.bin')):
    d=json.load(open(p))
    print('===',lbl,'keys',list(d.keys()))
    for k,v in d.items():
        if isinstance(v,list):
            print('  %s: len=%d type0=%s'%(k,len(v),type(v[0]).__name__ if v else 'empty'))
        elif isinstance(v,dict):
            print('  %s: dict keys %s'%(k,list(v.keys())[:10]))
        else:
            print('  %s: %r'%(k,v))
    if lbl=='NEW':
        C=d['C']; M=d.get('M')
        print('  n frames C',len(C))
        print('  C[0] len',len(C[0]),'C[1] len',len(C[1]))
        print('  M type',type(M),'len',len(M) if M is not None else None)
        if M is not None: print('  M sample',M[:3] if isinstance(M,list) else M)
        # magnitudes
        allc=[]
        for fr in C:
            for cid,val in fr: allc.append(val)
        allc=np.array(allc)
        print('  C nonzero count',len(allc),'min %.4g max %.4g med %.4g'%(allc.min(),allc.max(),np.median(allc)))
        if M is not None:
            mv=np.array([x[1] if isinstance(x,list) else x for x in M],dtype=float) if isinstance(M,list) else None
            if mv is not None: print('  M: min %.4g max %.4g med %.4g'%(mv.min(),mv.max(),np.median(mv)))
