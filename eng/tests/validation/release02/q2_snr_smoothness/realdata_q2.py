#!/usr/bin/env python3
"""Q2 real data: re-fit formulations on production control samples and measure
the residual-field jump across real coverage-subset boundaries."""
import numpy as np, json
from scipy.spatial import cKDTree
BASE='run/RELEASE-02/L4-rebuild'; OUT='run/RELEASE-02/q2-snr-smooth'
d=json.load(open(f'{BASE}/bitref_16w/p2_samples.json'))
ctr=d['controls']; obs=d['observations']; fids=d['frame_ids']
N=len(ctr)
ra=np.array([c['ra_deg'] for c in ctr]); dec=np.array([c['dec_deg'] for c in ctr])
ra0=float(np.median(ra)); dec0=float(np.median(dec))
xi=(ra-ra0)*np.cos(np.radians(dec0)); eta=dec-dec0
sx=float(np.std(xi)*3); sy=float(np.std(eta)*3)
U=(xi/sx); V=(eta/sy)
fidx={f:k for k,f in enumerate(fids)}
ci=np.array([o['control_id'] for o in obs]); fi=np.array([fidx[o['frame_id']] for o in obs])
val=np.array([o['value'] for o in obs]); sig=np.array([o['uncertainty'] for o in obs])
wobs=1.0/sig**2
ncov=np.bincount(ci,minlength=N)
NF=len(fids)
print('controls',N,'obs',len(obs),'frames',NF,'coverage median',int(np.median(ncov)))
Uo=U[ci]; Vo=V[ci]
def ncoef(o): return 3+(3 if o>=2 else 0)+(4 if o>=3 else 0)
def des(u,v,o):
    c=[np.ones_like(u),u,v]
    if o>=2: c+=[u*u,u*v,v*v]
    if o>=3: c+=[u**3,u*u*v,u*v*v,v**3]
    return np.stack(c,axis=1)
ORD=3; D=des(Uo,Vo,ORD); P=ncoef(ORD)
coef=np.zeros((NF,P))
for k in range(NF):
    m=(fi==k); A=D[m]; ww=wobs[m]
    coef[k]=np.linalg.solve((A*ww[:,None]).T@A,(A*ww[:,None]).T@val[m])
def ceval(cf):
    return np.einsum('ij,ij->i',D,cf[fi])
def Rfield(cf):
    res=val-ceval(cf)
    num=np.bincount(ci,weights=wobs*res,minlength=N); den=np.bincount(ci,weights=wobs,minlength=N)
    return num/np.maximum(den,1e-300)
def gs(alpha=0.5,niter=60,exclude=True):
    cf=coef.copy()
    for it in range(niter):
        res=val-ceval(cf)
        num=np.bincount(ci,weights=wobs*res,minlength=N); den=np.bincount(ci,weights=wobs,minlength=N)
        new=cf.copy()
        for k in range(NF):
            m=(fi==k)
            if exclude:
                nk=np.bincount(ci[m],weights=wobs[m]*res[m],minlength=N)
                dk=np.bincount(ci[m],weights=wobs[m],minlength=N)
                ne=num-nk; de=den-dk
            else:
                ne=num; de=den
            ok=de[ci[m]]>0
            if ok.sum()<P: continue
            tgt=val[m]-np.where(ok,ne[ci[m]]/np.maximum(de[ci[m]],1e-300),0.0)
            A=D[m][ok]; ww=wobs[m][ok]
            new[k]=np.linalg.solve((A*ww[:,None]).T@A,(A*ww[:,None]).T@tgt[ok])
        cf=(1-alpha)*cf+alpha*new
    return cf
print('GS (c) exclude-self ...'); cf_c=gs(0.5,60,True)
print('GS (d) include-self ...'); cf_d=gs(0.5,60,False)
# ---- (a) joint global B(order2) + per-frame plane ----
nB=ncoef(2); nd=3; ncol=nB+nd*(NF-1)
H=np.zeros((ncol,ncol)); rh=np.zeros(ncol)
Db=des(Uo,Vo,2)
for k in range(NF):
    m=(fi==k)
    A=np.zeros((int(m.sum()),ncol)); A[:,:nB]=Db[m]
    if k!=0:
        j=k-1; c0=nB+nd*j
        A[:,c0]=1.0; A[:,c0+1]=Uo[m]; A[:,c0+2]=Vo[m]
    ww=wobs[m]; H+=(A*ww[:,None]).T@A; rh+=(A*ww[:,None]).T@val[m]
th=np.linalg.solve(H,rh)
def ceval_a(th):
    ce=Db@th[:nB]
    for k in range(1,NF):
        m=(fi==k); c0=nB+nd*(k-1)
        ce[m]=ce[m]+th[c0]+th[c0+1]*Uo[m]+th[c0+2]*Vo[m]
    return ce
res_a=val-ceval_a(th)
num=np.bincount(ci,weights=wobs*res_a,minlength=N); den=np.bincount(ci,weights=wobs,minlength=N)
Ra=num/np.maximum(den,1e-300)
gmean=float(np.sum(wobs*res_a)/np.sum(wobs)); Rb=Ra+gmean
Rraw=np.bincount(ci,weights=wobs*val,minlength=N)/np.bincount(ci,weights=wobs,minlength=N)
# ---- boundary metric ----
Pp=np.stack([U*sx*60,V*sy*60],1)
tree=cKDTree(Pp); dd,ii=tree.query(Pp,k=5)
spacing=float(np.median(dd[:,1])); thr=2.0*spacing
E={}
for i in range(N):
    if ncov[i]==0: continue
    for q in range(1,5):
        j=ii[i,q]
        if dd[i,q]>thr or ncov[j]==0: continue
        E[(min(i,j),max(i,j))]=dd[i,q]
E=[(e,dd_) for e,dd_ in E.items()]
dist=np.array([dd_ for _,dd_ in E]); bnd=np.array([ncov[a]!=ncov[b] for (a,b),_ in E])
def metric(R):
    dR=np.array([abs(R[a]-R[b]) for (a,b),_ in E])/dist
    b=float(np.median(dR[bnd])); it=float(np.median(dR[~bnd]))
    return {'boundary_ADU_per_arcmin':b,'interior_ADU_per_arcmin':it,'ratio':b/max(it,1e-30),
            'boundary_n':int(bnd.sum()),'interior_n':int((~bnd).sum())}
out={'n_controls':int(N),'n_obs':len(obs),'n_frames':NF,'ncov_median':int(np.median(ncov)),
     'median_spacing_arcmin':spacing,'order':ORD}
print('edges',len(E),'spacing %.2f arcmin  boundary edges %d'%(spacing,bnd.sum()))
for tag,R in (('raw_no_correction',Rraw),('a_ref_gauge',Ra),('b_global_mean0',Rb),
              ('c_excl_self',Rfield(cf_c)),('d_incl_self',Rfield(cf_d))):
    m=metric(R); out[tag]=m
    print('%-20s boundary=%.4e interior=%.4e ratio=%.2f (ADU/arcmin)'%(
        tag,m['boundary_ADU_per_arcmin'],m['interior_ADU_per_arcmin'],m['ratio']))
json.dump(out,open(f'{OUT}/realdata_formulation.json','w'),indent=1)
print('wrote realdata_formulation.json')
