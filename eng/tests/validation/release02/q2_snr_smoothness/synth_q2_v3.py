#!/usr/bin/env python3
"""Q2 synthetic v3.

Truth: S(x,y) order-2 poly; per-frame b_k(x,y) order-2 poly with different
quadratic parts; normalized frames z_k = S + b_k + noise(sigma_raw/g).
Residual g_k = b_k - c_k.  Mosaic residual R(p)=sum_{k in S(p)} w_k g_k / W_S.

Formulations
  (a) ref gauge      : joint fit z_k = B(x) + delta_k(x), delta_ref=0 (B global
                       order-2 poly, delta_k plane)  [production sky_plane]
  (b) global mean-0  : (a) then shift so global weighted mean of g_k = 0
  (c) excl-self GS   : c_k = P_k(z_k - r_k), r_k = sum_{j!=k} w_j g_j / W_-k
  (d) incl-self GS   : r_k = sum_j w_j g_j / W
Gauge fixing each sweep prevents the common-mode random walk.
"""
import numpy as np, json, os, sys

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 512, 256
X, Y = np.meshgrid(np.arange(W), np.arange(H))
U = (X - 256.0)/256.0; V = (Y - 128.0)/128.0

def cols2(u, v, order=2):
    c = [np.ones_like(u), u, v]
    if order >= 2: c += [u*u, u*v, v*v]
    return c

S = 1000.0 + 3.0*U + 1.5*V + 0.8*U*U + 0.5*U*V - 0.4*V*V
SLABS = [('A',0,192),('B',128,320),('C',256,448),('D',384,512)]
NAMES = [s[0] for s in SLABS]
MASK = {n: ((X>=x0)&(X<x1)) for n,x0,x1 in SLABS}
BOUND = [128,192,256,320,384,448]
BCOEF = {'A':[0.0,2.0,-1.0,1.0,0.5,-0.7],'B':[8.0,-1.5,1.2,-0.8,1.4,0.3],
         'C':[-14.0,1.0,0.4,1.3,-0.9,0.6],'D':[20.0,-0.5,-1.8,0.6,0.2,1.1]}
def peval(c, order=2):
    c = list(c)+[0.0]*(6-len(c))
    v = c[0]+c[1]*U+c[2]*V
    if order>=2: v = v+c[3]*U*U+c[4]*U*V+c[5]*V*V
    return v
B_TRUE = {n: peval(BCOEF[n]) for n in NAMES}
GAIN={'A':1.0,'B':2.0,'C':0.8,'D':1.0}
SIG_RAW={'A':1.0,'B':2.6,'C':2.0,'D':5.0}
SIG_NORM={n:SIG_RAW[n]/GAIN[n] for n in NAMES}
W_RAW={n:1.0/SIG_RAW[n]**2 for n in NAMES}
W_NORM={n:1.0/SIG_NORM[n]**2 for n in NAMES}
W_EQ={n:1.0 for n in NAMES}
RNG = np.random.default_rng(20260219)
NOISE = {n: RNG.normal(0.0, SIG_NORM[n], (H,W)) for n in NAMES}
def make_z(scale):
    return {n: S + B_TRUE[n] + scale*NOISE[n] for n in NAMES}
Z = make_z(1.0)

# ------------------------------------------------------------ fit machinery
def design(m, order): return np.stack(cols2(U[m], V[m], order), axis=1)
def wls(m, target, w, order):
    A = design(m, order); ww = w*np.ones(A.shape[0])
    coef = np.linalg.solve((A*ww[:,None]).T@A, (A*ww[:,None]).T@target[m])
    cc = cols2(U[m], V[m], order)
    return sum(coef[i]*cc[i] for i in range(len(cc)))
def wls_coef(m, target, w, order):
    A = design(m, order); ww = w*np.ones(A.shape[0])
    return np.linalg.solve((A*ww[:,None]).T@A, (A*ww[:,None]).T@target[m])
def eval_poly(coef, order):
    cc = cols2(U, V, order)
    return sum(coef[i]*cc[i] for i in range(len(cc)))
def wls_full(target, wmask, order):
    m = wmask > 0
    A = design(m, order); ww = wmask[m]
    coef = np.linalg.solve((A*ww[:,None]).T@A, (A*ww[:,None]).T@target[m])
    cc = cols2(U, V, order)
    return sum(coef[i]*cc[i] for i in range(len(cc)))

def joint_ref_fit(weights, Zin, ref='A', B_order=2, d_order=1):
    nB=len(cols2(np.array([0.0]),np.array([0.0]),B_order)); nd=len(cols2(np.array([0.0]),np.array([0.0]),d_order))
    nonref=[n for n in NAMES if n!=ref]; ncol=nB+nd*len(nonref)
    Hm=np.zeros((ncol,ncol)); rh=np.zeros(ncol)
    for n in NAMES:
        m=MASK[n]; Ab=np.stack(cols2(U[m],V[m],B_order),axis=1)
        A=np.zeros((Ab.shape[0],ncol)); A[:,:nB]=Ab
        if n!=ref:
            j=nonref.index(n); c0=nB+nd*j
            dc=cols2(U[m],V[m],d_order)
            for q in range(nd): A[:,c0+q]=dc[q]
        w=weights[n]*np.ones(A.shape[0])
        Hm+=(A*w[:,None]).T@A; rh+=(A*w[:,None]).T@Zin[n][m]
    th=np.linalg.solve(Hm,rh)
    Bf=sum(th[i]*cols2(U,V,B_order)[i] for i in range(nB))
    delta={}
    for n in NAMES:
        if n==ref: delta[n]=np.zeros((H,W))
        else:
            j=nonref.index(n); c0=nB+nd*j
            delta[n]=sum(th[c0+q]*cols2(U,V,d_order)[q] for q in range(nd))
    return delta, Bf

def gauss_seidel(wref, Zin, order=2, exclude_self=True, niter=150, tol=1e-13, gauge='poly', alpha=0.5):
    c={n: np.zeros((H,W)) for n in NAMES}; hist=[]
    for it in range(niter):
        res={n: Zin[n]-c[n] for n in NAMES}
        newc={}
        for n in NAMES:
            m=MASK[n]; num=np.zeros((H,W)); den=np.zeros((H,W))
            for j in NAMES:
                if exclude_self and j==n: continue
                mj=MASK[j]; num[mj]+=wref[j]*res[j][mj]; den[mj]+=wref[j]
            tm=m&(den>0)
            if tm.sum()<6: newc[n]=c[n]; continue
            tgt=Zin[n]-np.where(den>0, num/np.maximum(den,1e-300), 0.0)
            coef=wls_coef(tm,tgt,wref[n],order)
            newc[n]=eval_poly(coef,order)   # evaluate model over full grid (extrapolate)
        c={n:(1.0-alpha)*c[n]+alpha*newc[n] for n in NAMES}   # damped: naive alpha=1
        # has eigenvalue -1 on bipartite (chain) coverage graphs -> period-2 oscillation
        # gauge fixing: remove the common (weighted) field from all residuals
        R,gm=resid_field(c,wref,Zin)
        if gauge=='poly':
            gfit=wls_full(np.nan_to_num(R, nan=0.0), (gm>0).astype(float), order)
        elif gauge=='scalar':
            gfit=np.full((H,W), np.nanmean(R[gm>0]))
        else:
            gfit=np.zeros((H,W))
        c={n: c[n]+gfit for n in NAMES}
        chg=float(np.nanmax(np.abs(gfit)))
        hist.append(chg)
        if chg<tol: break
    return c, hist

def resid_field(c, wref, Zin):
    num=np.zeros((H,W)); den=np.zeros((H,W))
    for n in NAMES:
        m=MASK[n]; num[m]+=wref[n]*(B_TRUE[n][m]-c[n][m]); den[m]+=wref[n]
    return num/np.maximum(den,1e-300), den

def stack_field(c, ws, Zin):
    num=np.zeros((H,W)); den=np.zeros((H,W))
    for n in NAMES:
        m=MASK[n]; wv=ws[n] if np.isscalar(ws[n]) else ws[n][m]
        num[m]+=wv*(Zin[n][m]-c[n][m]); den[m]+=(ws[n] if np.isscalar(ws[n]) else ws[n][m])
    return np.where(den>0, num/np.maximum(den,1e-300), np.nan)

def step(f, xb, half=16, gap=4):
    return float(np.nanmedian(f[:,xb+gap:xb+half])-np.nanmedian(f[:,xb-half:xb-gap]))
def steps_of(f): return {str(b): step(f,b) for b in BOUND}
def mx(d): return float(max(abs(v) for v in d.values()))
def summ(c, wref, ws, Zin):
    R,_=resid_field(c,wref,Zin); st=stack_field(c,ws,Zin)
    spread=np.std([B_TRUE[n]-c[n] for n in NAMES],axis=0)
    return {'resid_steps':steps_of(R),'resid_maxstep':mx(steps_of(R)),
            'resid_rms':float(np.sqrt(np.nanmean((R-np.nanmedian(R))**2))),
            'common_spread_med':float(np.nanmedian(spread)),
            'stack_steps':steps_of(st),'stack_maxstep':mx(steps_of(st)),
            'stack_minus_S_rms':float(np.sqrt(np.nanmean((st-S)**2)))}

def run(tag, wfit, Zin, order=2, B_order=2, d_order=1, ws=None, final_gauge=False, gauge='poly'):
    if ws is None: ws=wfit
    d,_=joint_ref_fit(wfit,Zin,B_order=B_order,d_order=d_order)
    ca=d
    gk={n: B_TRUE[n]-d[n] for n in NAMES}
    gscal=sum(float(np.sum(wfit[n]*gk[n][MASK[n]])) for n in NAMES)/sum(float(np.sum(wfit[n]*MASK[n])) for n in NAMES)
    cb={n: d[n]+gscal for n in NAMES}
    cc,hc=gauss_seidel(wfit,Zin,order=order,exclude_self=True,gauge=gauge)
    cd,hd=gauss_seidel(wfit,Zin,order=order,exclude_self=False,gauge=gauge)
    if final_gauge:
        for src in (cc,cd):
            R,_=resid_field(src,wfit,Zin)
            for n in NAMES: src[n]=src[n]+R
    out={'tag':tag,'gauge_shift_ADU':gscal,'iters_excl':len(hc),'iters_incl':len(hd)}
    out['a_ref']=summ(ca,wfit,ws,Zin); out['b_global_mean0']=summ(cb,wfit,ws,Zin)
    out['c_excl_self']=summ(cc,wfit,ws,Zin); out['d_incl_self']=summ(cd,wfit,ws,Zin)
    return out

def show(tag,sc):
    print('== %s  gauge_shift=%+.4f  iters %d/%d'%(tag,sc['gauge_shift_ADU'],sc['iters_excl'],sc['iters_incl']))
    for f in ['a_ref','b_global_mean0','c_excl_self','d_incl_self']:
        x=sc[f]
        print('   %-16s spread=%9.2e residRMS=%9.2e residMaxStep=%9.4f stackMaxStep=%9.4f'%(
            f,x['common_spread_med'],x['resid_rms'],x['resid_maxstep'],x['stack_maxstep']))
        print('        resid_steps',' '.join('%+.4f'%x['resid_steps'][str(b)] for b in BOUND))

res={}
# ---- item 2 weight order ----
order_raw=sorted(NAMES,key=lambda n:-W_RAW[n]); order_norm=sorted(NAMES,key=lambda n:-W_NORM[n])
def kendall(a,b):
    c=dsc=0
    for i in range(len(a)):
        for j in range(i+1,len(a)):
            s=np.sign(a[i]-a[j])*np.sign(b[i]-b[j])
            c+= s>0; dsc+= s<0
    return (c-dsc)/(c+dsc)
res['weight_order']={'frames':NAMES,'gain':GAIN,'sigma_raw':SIG_RAW,
    'sigma_norm':{n:SIG_NORM[n] for n in NAMES},'w_raw':W_RAW,'w_norm':W_NORM,
    'rank_raw':order_raw,'rank_norm':order_norm,
    'kendall_tau':float(kendall([W_RAW[n] for n in NAMES],[W_NORM[n] for n in NAMES])),
    'order_flipped':bool(order_raw!=order_norm),
    'A_over_B_raw':float(W_RAW['A']/W_RAW['B']),'A_over_B_norm':float(W_NORM['A']/W_NORM['B'])}
print('=== item2 weight ordering ==='); print(json.dumps(res['weight_order'],indent=1))

# ---- noise-free formulation comparison ----
print('\n=== NF: NOISE-FREE, order-2 models, SNR weights ===')
Z0=make_z(0.0)
nf=run('NF',W_NORM,Z0,order=2,B_order=2,d_order=1,ws=W_NORM,gauge='poly')
show('NF',nf); res['NF']=nf

print('\n=== NF2: NOISE-FREE, production-like B=ord2 + delta=plane, equal weights ===')
nf2=run('NF2',W_EQ,Z0,order=2,B_order=2,d_order=1,ws=W_EQ,gauge='poly')
show('NF2',nf2); res['NF2']=nf2

# ---- noisy formulation comparison ----
print('\n=== N1: NOISY, order-2 models, SNR weights, poly gauge ===')
n1=run('N1',W_NORM,Z,order=2,B_order=2,d_order=1,ws=W_NORM,gauge='poly')
show('N1',n1); res['N1']=n1

print('\n=== N2: NOISY, equal weights ===')
n2=run('N2',W_EQ,Z,order=2,B_order=2,d_order=1,ws=W_EQ,gauge='poly')
show('N2',n2); res['N2']=n2

print('\n=== N3: NOISY, scalar gauge only (shows common-mode drift) ===')
n3=run('N3',W_NORM,Z,order=2,B_order=2,d_order=1,ws=W_NORM,gauge='scalar')
show('N3',n3); res['N3']=n3

# ---- final global-field gauge on all four (noisy) ----
print('\n=== N4: NOISY, final global-field gauge applied to (a) ===')
# recompute a and apply R subtraction
d,_=joint_ref_fit(W_NORM,Z,B_order=2,d_order=1)
Ra,_=resid_field(d,W_NORM,Z)
d_final={n:d[n]+Ra for n in NAMES}
n4=dict(n1); n4['tag']='N4'
n4['a_ref_finalgauge']=summ(d_final,W_NORM,W_NORM,Z)
print('   a_ref_finalgauge residMaxStep=%.4f stackMaxStep=%.4f spread=%.2e'%(
    n4['a_ref_finalgauge']['resid_maxstep'], n4['a_ref_finalgauge']['stack_maxstep'],
    n4['a_ref_finalgauge']['common_spread_med'])); res['N4']=n4

# ---- weight-map invariance ----
wmap={n:np.ones((H,W)) for n in NAMES}; wmap['B']=np.where(X>256,3.0,1.0)
print('\n=== N5: NOISY + spatially varying stack weight (B x3 for x>256) ===')
n5=run('N5',W_NORM,Z,order=2,B_order=2,d_order=1,ws={n:W_NORM[n]*wmap[n] for n in NAMES},gauge='poly')
show('N5',n5); res['N5']=n5
print('  per-boundary stack step change vs N1 (ADU):')
for f in ['a_ref','c_excl_self']:
    d={b: n5[f]['stack_steps'][str(b)]-n1[f]['stack_steps'][str(b)] for b in BOUND}
    print('   %-16s'%f, ' '.join('%s:%+.4f'%(b,d[b]) for b in BOUND))


# ---- N6: does a *changed* stack weight reintroduce a step after the final gauge? ----
print('\n=== N6: final global gauge (base w) then STACK with mapped weights (B x5 for x>256) ===')
wmap5={n:np.ones((H,W)) for n in NAMES}; wmap5['B']=np.where(X>256,5.0,1.0)
ws5={n:W_NORM[n]*wmap5[n] for n in NAMES}
d6,_=joint_ref_fit(W_NORM,Z,B_order=2,d_order=1)
Ra6,_=resid_field(d6,W_NORM,Z); ca_g={n:d6[n]+Ra6 for n in NAMES}
cc6,_=gauss_seidel(W_NORM,Z,order=2,exclude_self=True,gauge='poly')
Rc6,_=resid_field(cc6,W_NORM,Z); cc_g={n:cc6[n]+Rc6 for n in NAMES}
for tag,cf in (('a_ref+finalgauge',ca_g),('c_excl_self+finalgauge',cc_g)):
    st=stack_field(cf,ws5,Z); s=steps_of(st)
    print('   %-24s stack_steps'%tag,' '.join('%+.4f'%s[str(b)] for b in BOUND),' maxabs=%.4f'%mx(s))
for tag,cf in (('a_ref (base w)',d6),('c_excl_self (base w)',cc6)):
    st=stack_field(cf,W_NORM,Z); s=steps_of(st)
    print('   %-24s stack_steps'%tag,' '.join('%+.4f'%s[str(b)] for b in BOUND),' maxabs=%.4f'%mx(s))

with open(os.path.join(OUT,'synth_results_v3.json'),'w') as fo: json.dump(res,fo,indent=1)
print('\nwrote synth_results_v3.json')
