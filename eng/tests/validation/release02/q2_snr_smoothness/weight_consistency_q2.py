#!/usr/bin/env python3
"""Q2 item 5: weight<->correction self-consistency under a changed stack weight."""
import numpy as np, json, os
HERE=os.path.dirname(os.path.abspath(__file__))
src=open(os.path.join(HERE,'synth_q2_v3.py')).read().split("res={}")[0]
exec(src)   # defines S, Z, NAMES, B_TRUE, W_NORM, and all fit helpers
out={}
wmap5={n:np.ones((H,W)) for n in NAMES}; wmap5['B']=np.where(X>256,5.0,1.0)
ws5={n:W_NORM[n]*wmap5[n] for n in NAMES}
d,_=joint_ref_fit(W_NORM,Z,B_order=2,d_order=1)
Ra,_=resid_field(d,W_NORM,Z); ca_g={n:d[n]+Ra for n in NAMES}
cc,_=gauss_seidel(W_NORM,Z,order=2,exclude_self=True,gauge='poly')
Rc,_=resid_field(cc,W_NORM,Z); cc_g={n:cc[n]+Rc for n in NAMES}
def rep(tag, cf, ws):
    st=stack_field(cf,ws,Z); s=steps_of(st)
    R,_=resid_field(cf,W_NORM,Z); rs=steps_of(R)
    spread=np.std([B_TRUE[n]-cf[n] for n in NAMES],axis=0)
    print('  %-26s stackSteps'%tag,' '.join('%+.4f'%s[str(b)] for b in BOUND),
          ' maxabs=%.4f  spread=%.3f'%(mx(s), float(np.nanmedian(spread))))
    out[tag]={'stack_steps':s,'stack_maxstep':mx(s),'spread':float(np.nanmedian(spread))}
print('=== N6: changed stack weight (B x5 for x>256) ===')
print('-- after final global gauge computed with BASE weights --')
rep('a_ref+finalgauge,mappedW', ca_g, ws5)
rep('c_excl+finalgauge,mappedW', cc_g, ws5)
print('-- base weights (reference) --')
rep('a_ref,baseW', d, W_NORM)
rep('c_excl,baseW', cc, W_NORM)
print('-- final gauge recomputed WITH the mapped weights (fully self-consistent) --')
wsb=W_NORM
dA,_=joint_ref_fit(wsb,Z,B_order=2,d_order=1)
# gauge using the actual stack weights
def gauge_with(cf, ws):
    num=np.zeros((H,W)); den=np.zeros((H,W))
    for n in NAMES:
        m=MASK[n]; wv=ws[n] if np.isscalar(ws[n]) else ws[n][m]
        num[m]+=wv*(Z[n][m]-cf[n][m]); den[m]+=(ws[n] if np.isscalar(ws[n]) else ws[n][m])
    R=num/np.maximum(den,1e-300)
    return {n:cf[n]+R for n in NAMES}
rep('a_ref+gauge(mappedW),mappedW', gauge_with(d, ws5), ws5)
rep('c_excl+gauge(mappedW),mappedW', gauge_with(cc, ws5), ws5)
with open(os.path.join(HERE,'weight_consistency.json'),'w') as f: json.dump(out,f,indent=1)
