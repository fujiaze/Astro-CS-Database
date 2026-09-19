
import numpy as np, json, os
from astropy.io import fits
ROOT='/workspace/Astro CS Database'
P={'OLD':ROOT+'/run/RELEASE-02/L4-rebuild/p3_r_vis/output_phase3.fits',
   'NEW':ROOT+'/run/RELEASE-02/L4-rebuild/p3_upmfix/output_phase3.fits'}
D={}
for lbl,p in P.items():
    with fits.open(p,memmap=False) as h:
        D[lbl]=np.asarray(h[0].data,dtype=np.float64)
def band_x(d,y,pos,w=20):
    l=d[y,pos-w:pos]; r=d[y,pos:pos+w]
    if np.isfinite(l).sum()<w*0.5 or np.isfinite(r).sum()<w*0.5: return np.nan
    return float(np.median(r)-np.median(l))
def band_y(d,x,pos,w=20):
    l=d[pos-w:pos,x]; r=d[pos:pos+w,x]
    if np.isfinite(l).sum()<w*0.5 or np.isfinite(r).sum()<w*0.5: return np.nan
    return float(np.median(r)-np.median(l))
# linear-fit step (stage16 Jx/Jy)
def Jx(d,x0,ylo,yhi,K=25,gap=1):
    cL=np.arange(x0-K,x0-gap); cR=np.arange(x0+gap+1,x0+K+1)
    L=d[ylo:yhi,cL[0]:cL[-1]+1]; R=d[ylo:yhi,cR[0]:cR[-1]+1]
    ok=np.isfinite(L).all(1)&np.isfinite(R).all(1)
    if ok.sum()<5: return np.nan
    L=L[ok];R=R[ok]
    AL=np.vstack([np.ones_like(cL,float),cL]).T; AR=np.vstack([np.ones_like(cR,float),cR]).T
    eL=L@np.linalg.pinv(AL).T; eR=R@np.linalg.pinv(AR).T
    return float(np.median((eR[:,0]+eR[:,1]*x0)-(eL[:,0]+eL[:,1]*x0)))
def Jy(d,y0,xlo,xhi,K=25,gap=1):
    rL=np.arange(y0-K,y0-gap); rR=np.arange(y0+gap+1,y0+K+1)
    L=d[rL[0]:rL[-1]+1,xlo:xhi]; R=d[rR[0]:rR[-1]+1,xlo:xhi]
    ok=np.isfinite(L).all(0)&np.isfinite(R).all(0)
    if ok.sum()<5: return np.nan
    L=L[:,ok];R=R[:,ok]
    AL=np.vstack([np.ones_like(rL,float),rL]).T; AR=np.vstack([np.ones_like(rR,float),rR]).T
    eL=L.T@np.linalg.pinv(AL).T; eR=R.T@np.linalg.pinv(AR).T
    return float(np.median((eR[:,0]+eR[:,1]*y0)-(eL[:,0]+eL[:,1]*y0)))
bgx=np.nanmedian(D['OLD'][300:3600,2145-120:2145+120])
bgy1=np.nanmedian(D['OLD'][1185-120:1185+120,900:1900])
bgy2=np.nanmedian(D['OLD'][1392-120:1392+120,700:2000])
print('OLD local bg: x2145 %.4g  y1185 %.4g  y1392 %.4g'%(bgx,bgy1,bgy2))
print()
print('=== A. fixed coordinate (parent method), band w=20 ===')
for lbl in ('OLD','NEW'):
    d=D[lbl]
    s1=np.array([band_x(d,y,2145) for y in range(300,3600)]); s1=s1[np.isfinite(s1)]
    s2=np.array([band_y(d,x,1185) for x in range(900,1900)]); s2=s2[np.isfinite(s2)]
    s3=np.array([band_y(d,x,1392) for x in range(700,2000)]); s3=s3[np.isfinite(s3)]
    print('%s: x2145 %+.4g (%.3f%%) | y1185 %+.4g (%.3f%%) | y1392 %+.4g (%.3f%%)'%(
        lbl,np.median(s1),100*np.median(s1)/bgx,np.median(s2),100*np.median(s2)/bgy1,np.median(s3),100*np.median(s3)/bgy2))
print()
print('=== B. tilt-corrected: search +-20px for max |J|, along nominal boundary ===')
for lbl in ('OLD','NEW'):
    d=D[lbl]
    # vertical x~2145
    v1=[]
    for y in range(400,3501,100):
        best=(0,None)
        for xx in range(2145-20,2145+21):
            j=Jx(d,xx,y-50,y+50)
            if np.isfinite(j) and abs(j)>abs(best[0]): best=(j,xx)
        v1.append(best[0])
    v1=np.array([v for v in v1 if np.isfinite(v)])
    # horizontal y~1185
    v2=[]
    for x in range(750,1951,50):
        best=(0,None)
        for yy in range(1185-20,1185+21):
            j=Jy(d,yy,x-50,x+50)
            if np.isfinite(j) and abs(j)>abs(best[0]): best=(j,yy)
        v2.append(best[0])
    v2=np.array([v for v in v2 if np.isfinite(v)])
    # horizontal y~1392
    v3=[]
    for x in range(750,1951,50):
        best=(0,None)
        for yy in range(1392-20,1392+21):
            j=Jy(d,yy,x-50,x+50)
            if np.isfinite(j) and abs(j)>abs(best[0]): best=(j,yy)
        v3.append(best[0])
    v3=np.array([v for v in v3 if np.isfinite(v)])
    print('%s: x2145 %+.4g (%.3f%%) p16..p84 %+.3g..%+.3g n=%d'%(lbl,np.median(v1),100*np.median(v1)/bgx,np.percentile(v1,16),np.percentile(v1,84),v1.size))
    print('      y1185 %+.4g (%.3f%%) p16..p84 %+.3g..%+.3g n=%d'%(np.median(v2),100*np.median(v2)/bgy1,np.percentile(v2,16),np.percentile(v2,84),v2.size))
    print('      y1392 %+.4g (%.3f%%) p16..p84 %+.3g..%+.3g n=%d'%(np.median(v3),100*np.median(v3)/bgy2,np.percentile(v3,16),np.percentile(v3,84),v3.size))
json.dump({'bg':[float(bgx),float(bgy1),float(bgy2)]},open('run/RELEASE-02/c-delta/seam_meta.json','w'),indent=1)
