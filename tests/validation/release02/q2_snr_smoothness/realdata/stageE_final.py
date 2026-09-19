#!/usr/bin/env python3
"""Q2 stage E: independent wide-strip cross-check, cross-axis null, global offset, final JSON+log."""
import numpy as np, json, os, warnings, datetime
warnings.filterwarnings('ignore')
from astropy.io import fits

ROOT='/workspace/Astro CS Database'; L4=ROOT+'/run/RELEASE-02/L4-rebuild'
OUT=ROOT+'/run/RELEASE-02/q2-snr-smooth/realdata'
loci=json.load(open(OUT+'/loci.json'))
P={'p3_r_vis':L4+'/p3_r_vis/output_phase3.fits',
   'p3_r_vis_fixed':L4+'/p3_r_vis_fixed/output_phase3.fits',
   'p3_upmfix':L4+'/p3_upmfix/output_phase3.fits'}
D={}; C={}
for k,p in P.items():
    with fits.open(p,memmap=True) as h:
        D[k]=np.asarray(h[0].data,dtype=np.float64)
        C[k]=np.asarray(h[1].data,dtype=np.float64)
nmap=np.load(OUT+'/nmap.npy')

# ---------------- global offset vs OLD ----------------
print('=== GLOBAL OFFSET vs p3_r_vis (OLD) ===')
common=np.isfinite(D['p3_r_vis'])&np.isfinite(D['p3_r_vis_fixed'])&np.isfinite(D['p3_upmfix'])&(C['p3_r_vis']>0.5)
print('common finite+covered px:',int(common.sum()))
offset={}
for k in ('p3_r_vis_fixed','p3_upmfix'):
    diff=(D[k]-D['p3_r_vis'])[common]
    blk=(D[k]-D['p3_r_vis'])
    B=np.nanmedian(blk.reshape(16,256,16,256).transpose(0,2,1,3).reshape(16,16,256*256),axis=2)
    rec=dict(median=float(np.median(diff)),p16=float(np.percentile(diff,16)),p84=float(np.percentile(diff,84)),
             mean=float(diff.mean()),block_median_min=float(np.nanmin(B)),block_median_max=float(np.nanmax(B)),
             block_median_med=float(np.nanmedian(B)))
    offset[k]=rec
    print('  %-15s median %+.6g  p16 %+.6g  p84 %+.6g  | block-median spread [%.4g, %.4g] (med %.4g)'%(
        k,rec['median'],rec['p16'],rec['p84'],rec['block_median_min'],rec['block_median_max'],rec['block_median_med']))
print('  OLD global covered median: %.6g'%float(np.nanmedian(D['p3_r_vis'][C['p3_r_vis']>0.5])))
for k in ('p3_r_vis_fixed','p3_upmfix'):
    print('  %s global covered median: %.6g'%(k,float(np.nanmedian(D[k][C[k]>0.5]))))

# ---------------- independent wide-strip estimator (no search) ----------------
def ws_y(d,yb,x,hw=50,gap=6,half=10):
    a=d[yb-gap-half:yb-gap, x-hw:x+hw]; b=d[yb+gap:yb+gap+half, x-hw:x+hw]
    if not (np.isfinite(a).any() and np.isfinite(b).any()): return np.nan
    return float(np.nanmedian(b)-np.nanmedian(a))
def ws_x(d,xb,y,hw=50,gap=6,half=10):
    a=d[y-hw:y+hw, xb-gap-half:xb-gap]; b=d[y-hw:y+hw, xb+gap:xb+gap+half]
    if not (np.isfinite(a).any() and np.isfinite(b).any()): return np.nan
    return float(np.nanmedian(b)-np.nanmedian(a))
def med(v):
    v=np.asarray([x for x in v if np.isfinite(x)],float)
    return None if v.size==0 else dict(n=int(v.size),median=float(np.median(v)),
        p16=float(np.percentile(v,16)),p84=float(np.percentile(v,84)))

xs=np.arange(750,1951,25); yv=np.arange(400,3501,50)
SPEC={'H1top':('y',xs,lambda x:loci['H1top']['intercept']+loci['H1top']['slope']*x),
      'H1bot':('y',xs,lambda x:loci['H1bot']['intercept']+loci['H1bot']['slope']*x),
      'V-right':('x',yv,lambda y:loci['V-right']['intercept']+loci['V-right']['slope']*y)}
BGREF={'H1top':1.03542e13,'H1bot':1.07818e13,'V-right':1.01664e13}

print()
print('=== INDEPENDENT wide-strip estimator (10px strips, 6px gap, no search), J = below/right - above/left ===')
wres={}
for bn,(ax,smp,f) in SPEC.items():
    for shift in (0,-150,150):
        for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
            if ax=='y': v=[ws_y(D[prod],int(round(f(s)+shift)),s) for s in smp]
            else:       v=[ws_x(D[prod],int(round(f(s)+shift)),s) for s in smp]
            st=med(v); key='%s|%s|shift%+d'%(bn,prod,shift)
            if st: st['pct_of_ref_old_bg']=100*st['median']/BGREF[bn]
            wres[key]=st
            print('  %-8s %-15s %+5d  n=%3s  J=%+13.5g  p16..p84 %+.4g..%+.4g  %+7.4f%%ref'%(
                bn,prod,shift,st['n'] if st else '-',st['median'] if st else float('nan'),
                st['p16'] if st else float('nan'),st['p84'] if st else float('nan'),
                st['pct_of_ref_old_bg'] if st else float('nan')))
    print('-'*110)

# ---------------- cross-axis null: look for a step on the OTHER axis at the same locus ----------------
print()
print('=== CROSS-AXIS NULL (other-axis step at the same locus, +/-8 search) ===')
K,GAP=25,1
def Jy(d,y0,xlo,xhi):
    rL=np.arange(y0-K,y0-GAP); rR=np.arange(y0+GAP+1,y0+K+1)
    L=d[rL[0]:rL[-1]+1,xlo:xhi]; R=d[rR[0]:rR[-1]+1,xlo:xhi]
    ok=np.isfinite(L).all(0)&np.isfinite(R).all(0)
    if ok.sum()<5: return np.nan
    L=L[:,ok];R=R[:,ok]; AL=np.vstack([np.ones_like(rL,float),rL]).T; p=np.linalg.pinv(AL).T
    eL=L.T@p; eR=R.T@p
    return float(np.median((eR[:,0]+eR[:,1]*y0)-(eL[:,0]+eL[:,1]*y0)))
def Jx(d,x0,ylo,yhi):
    cL=np.arange(x0-K,x0-GAP); cR=np.arange(x0+GAP+1,x0+K+1)
    L=d[ylo:yhi,cL[0]:cL[-1]+1]; R=d[ylo:yhi,cR[0]:cR[-1]+1]
    ok=np.isfinite(L).all(1)&np.isfinite(R).all(1)
    if ok.sum()<5: return np.nan
    L=L[ok];R=R[ok]; AL=np.vstack([np.ones_like(cL,float),cL]).T; p=np.linalg.pinv(AL).T
    eL=L@p; eR=R@p
    return float(np.median((eR[:,0]+eR[:,1]*x0)-(eL[:,0]+eL[:,1]*x0)))
nullres={}
for bn,(ax,smp,f) in SPEC.items():
    for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
        v=[]
        for s in smp:
            if ax=='y':
                yb=int(round(f(s))); best=0.0
                for xx in range(s-8,s+9):
                    j=Jx(D[prod],xx,yb-8,yb+8)
                    if np.isfinite(j) and abs(j)>abs(best): best=j
                v.append(best)
            else:
                xb=int(round(f(s))); best=0.0
                for yy in range(s-8,s+9):
                    j=Jy(D[prod],yy,xb-8,xb+8)
                    if np.isfinite(j) and abs(j)>abs(best): best=j
                v.append(best)
        st=med(v); nullres['%s|%s'%(bn,prod)]=st
        print('  %-8s %-15s n=%3s  |J|median=%+11.5g  p16..p84 %+.4g..%+.4g'%(
            bn,prod,st['n'] if st else '-',st['median'] if st else float('nan'),
            st['p16'] if st else float('nan'),st['p84'] if st else float('nan')))

# ---------------- final assembly ----------------
stageC=json.load(open(OUT+'/stageC_steps.json'))
p2=json.load(open(OUT+'/p2_samples_summary.json'))
final=dict(
  schema='ASTROCS-Q2-REALDATA-SEAM-v1',
  generated_utc=datetime.datetime.utcnow().isoformat()+'Z',
  task='RELEASE-02 Q2 real-data measurement (nmap / tilted seam loci / seam steps / p2 sample structure)',
  products=dict(OLD_normalisation_reference=P['p3_r_vis'],
                NEW_bitref_16w=P['p3_r_vis_fixed'],
                upmfix_out=P['p3_upmfix']),
  nmap=dict(method='stage10_nmap.py method, 49 frames norm/*/calibrated_*.fts WCS -> p3 TAN grid coverage count',
            shape=[4096,4096], dtype='int16', used_frames=49, skipped=0,
            hist={str(k):v for k,v in zip(*[x.tolist() for x in np.unique(nmap,return_counts=True)])},
            n_gt0=int((nmap>0).sum()), n_eq0=int((nmap==0).sum()),
            saved=OUT+'/nmap.npy'),
  loci=loci,
  loci_notes=dict(H1top='x in [700,2000]; first y in [1100,1300] with n>=7.5 (rising), sub-pixel interp, robust linear fit',
                  H1bot='x in [700,2000]; first y in [1350,1500] with n<=8.5 (falling)',
                  Vright='y in [300,3600]; first x in [2050,2250] with n<=8.5 (falling)'),
  measurement_primary=dict(
     estimator='stage16_locus.py Jy/Jx: per-row/col linear extrapolation over K=25px on each side (gap=1), evaluated at the boundary, then median over samples; +/-8px search for max |J|',
     convention='J = below(right) side minus above(left) side',
     K=25, gap=1, search_px=8,
     samples=dict(H1top='x=750..1950 step 25 (49)', H1bot='x=750..1950 step 25 (49)', Vright='y=400..3500 step 50 (63)'),
     bg_denominator=dict(caliber='OLD=p3_r_vis regional median (out-of-band side); local variant = OLD nanmedian over the exact J measurement window',
                         refs=stageC['bg_ref_old_regional'],
                         used_ref=BGREF),
     results=stageC['measurements']),
  measurement_independent_wide_strip=dict(
     estimator='10px strip on each side, 6px gap, NO search, nanmedian difference, then median over samples',
     bg_denominator_ref=BGREF, results=wres),
  cross_axis_null=nullres,
  global_offset_vs_old=offset,
  p2_samples=p2,
)
with open(OUT+'/realdata_seam.json','w') as f: json.dump(final,f,indent=1)
print()
print('saved',OUT+'/realdata_seam.json')
