#!/usr/bin/env python3
"""Q1.5c Real-data MULTIPLICATIVE spatial residual, measured from STARS.
Stars are point sources: a multiplicative response shows up as a spatially
varying flux ratio between frames; an additive sky gradient does NOT (local
background subtraction removes it).  This breaks the background degeneracy.

usage: star_residual.py <norm_group_dir> [...]
"""
import json, os, sys, math, glob
import numpy as np
from astropy.io import fits
from astropy.wcs import WCS
OUT=os.path.dirname(os.path.abspath(__file__))
# 几何/SIP **取自产品**（逐帧 HiPS 清单的加性 geometry）的唯一读取实现 = q3lib
# （同 release02 内的共享库）；此处 import 它以免复制第二份读法。
_Q3LIB = os.path.join(os.path.dirname(os.path.dirname(OUT)), 'q3_additive_truth', 'src')
if _Q3LIB not in sys.path:
    sys.path.insert(0, _Q3LIB)
import q3lib

def pp(v):
    v=np.asarray(v); v=v[np.isfinite(v)]
    return float(np.percentile(v,95)-np.percentile(v,5)) if v.size else float('nan')
def pb(x,y,deg=2):
    return np.column_stack([(x**i)*(y**j) for i in range(deg+1) for j in range(deg+1-i)])

def _frame_dir_map(g):
    """group → {亮场 stem（`@` 形态）: 帧产品目录}，来源 = 产品面 p1_products.json#frames[]
    （帧集取自产品；`@`→`_` 映射规则见 q3lib.frames 的说明）。"""
    pj = os.path.join(g, 'p1_products.json')
    m = {}
    for fr in json.load(open(pj)).get('frames', []):
        stem = os.path.basename(fr['input_light']).rsplit('.', 1)[0]
        m[stem] = fr['hips_path']
    return m

def load_group(g):
    """每帧星表 + 天球坐标。

    · `p1_flux.json` 是**中间产物**（工作区）⇒ 路径取自运行级记账面
      <g>/run_context.json#work_dir（**要求 keep_intermediates=true**，缺省即 true）。
    · WCS **取自产品**（`AGENTS §13.1`）：逐帧 HiPS 清单的加性 `geometry`
      （经 q3lib.load_geometry）。旧实现读 <g>/calibrated_<file>.fts 的头 ——
      该文件是中间产物，已移入工作区、产品面内不再存在。
    · group → 帧目录映射经 p1_products.json#frames[] 的 `input_light`；**映射不到即
      报错，不猜**（不做 `_`→`@` 反向替换）。
    """
    work = q3lib.load_work_dir(g)
    F=json.load(open(os.path.join(work,'p1_flux.json')))['frames']
    fmap=_frame_dir_map(g)
    out=[]
    for fr in F:
        stem = fr['file'].replace('cleaned_','').rsplit('.fts',1)[0]
        fdir = fmap.get(stem)
        if fdir is None:
            raise KeyError('帧不在产品清单 frames[] 中（不猜几何来源）: ' + fr['file'])
        geo = q3lib.load_geometry(fdir)
        hdr = dict(CTYPE1=geo['ctype1'], CTYPE2=geo['ctype2'],
                   CRPIX1=geo['crpix1'], CRPIX2=geo['crpix2'],
                   CRVAL1=geo['crval1'], CRVAL2=geo['crval2'],
                   CD1_1=geo['cd11'], CD1_2=geo['cd12'],
                   CD2_1=geo['cd21'], CD2_2=geo['cd22'],
                   NAXIS=2, NAXIS1=4096, NAXIS2=4096, RADESYS='ICRS')
        w=WCS(hdr)
        s=geo.get('sip')
        if s:
            from astropy.wcs import Sip
            def _sq(v):
                a=np.array(v, dtype=np.float64); k=int(round(len(a)**0.5))
                return a.reshape(k,k)
            w.sip = Sip(_sq(s['a']), _sq(s['b']), _sq(s['ap']), _sq(s['bp']), w.wcs.crpix)
        res=[r for r in fr['results'] if r.get('valid') and r.get('flux',0)>0 and np.isfinite(r.get('flux',np.nan))]
        x=np.array([r['x'] for r in res]); y=np.array([r['y'] for r in res])
        f=np.array([r['flux'] for r in res]); fe=np.array([r['flux_error'] for r in res])
        snr=np.array([r['snr'] for r in res])
        ra,dec=w.all_pix2world(x,y,0)
        out.append(dict(name=fr['file'],ra=ra,dec=dec,f=f,fe=fe,snr=snr,n=len(res)))
    return out

def match(A,B,tol=1.5/3600.0):
    # nearest neighbour in sky (small field: plain O(N*M) too big -> use sorting on ra)
    oa=np.argsort(A['ra']); ob=np.argsort(B['ra'])
    ra=A['ra'][oa]; rb=B['ra'][ob]
    ia=[];ib=[]
    j0=0
    for i in range(len(ra)):
        while j0<len(rb) and rb[j0]<ra[i]-tol: j0+=1
        j=j0
        best=-1; bd=1e9
        while j<len(rb) and rb[j]<ra[i]+tol:
            dd=(rb[j]-ra[i])**2+(B['dec'][ob[j]]-A['dec'][oa[i]])**2
            if dd<bd: bd=dd; best=j
            j+=1
        if best>=0 and bd<tol*tol: ia.append(oa[i]); ib.append(ob[best])
    return np.array(ia),np.array(ib)

rows=[]
for g in sys.argv[1:]:
    if not os.path.isdir(g): continue
    frames=load_group(g)
    print(f"=== {g.split('/')[-1]} : {len(frames)} frames, nsrc={[f['n'] for f in frames]}")
    for i in range(len(frames)):
        for j in range(i+1,len(frames)):
            A,B=frames[i],frames[j]
            ia,ib=match(A,B)
            if len(ia)<200: continue
            # only bright, well-measured stars
            m=(A['snr'][ia]>100)&(B['snr'][ib]>100)&(A['f'][ia]>0)&(B['f'][ib]>0)
            ia,ib=ia[m],ib[m]
            if len(ia)<100: continue
            ratio=A['f'][ia]/B['f'][ib]
            rel_err=np.hypot(A['fe'][ia]/A['f'][ia],B['fe'][ib]/B['f'][ib])
            r0=float(np.median(ratio))
            rel=ratio/r0-1.0
            x=(A['ra'][ia]-np.median(A['ra'][ia]))/max(np.ptp(A['ra'][ia])/2,1e-9)
            y=(A['dec'][ia]-np.median(A['dec'][ia]))/max(np.ptp(A['dec'][ia])/2,1e-9)
            X=pb(x,y,2); c,*_=np.linalg.lstsq(X,rel,rcond=None); fit=X@c; res=rel-fit
            # noise floor: weighted mean of per-star relative errors
            nf=float(np.median(rel_err))
            # chi2 of the global-ratio model
            chi2_global=float(np.mean((rel/rel_err)**2))
            chi2_flat=float(np.mean((res/rel_err)**2))
            rows.append(dict(group=os.path.basename(g),i=i,j=j,n=int(len(ia)),r0=r0,
                             rel_pp=pp(rel), fit_pp=pp(fit), res_pp=pp(res),
                             noise_pp=3.3*math.sqrt(2)*nf, nf_rel=nf,
                             chi2_global=chi2_global, chi2_spatial=chi2_flat,
                             fit_coef=c.tolist()))
            print(f"  pair {i}-{j}: n={len(ia)} global_ratio={r0:.4f} rel_pp={pp(rel)*100:7.3f}% "
                  f"coherent_pp={pp(fit)*100:7.3f}% resid_pp={pp(res)*100:7.3f}% "
                  f"noise_floor_pp={3.3*math.sqrt(2)*nf*100:6.3f}% chi2_glob={chi2_global:8.1f} chi2_spat={chi2_flat:8.1f}")
if rows:
    import statistics
    print("\n--- aggregate over pairs ---")
    for k in ['n','r0','rel_pp','fit_pp','res_pp','noise_pp','chi2_global','chi2_spatial']:
        v=np.array([r[k] for r in rows]); print(f"  {k:>12}: median={np.median(v):.5g}")
json.dump(rows,open(os.path.join(OUT,'star_residual.json'),'w'),indent=1)
print("wrote star_residual.json")
