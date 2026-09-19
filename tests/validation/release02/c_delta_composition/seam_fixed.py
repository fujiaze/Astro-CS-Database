
import numpy as np, json, os
from astropy.io import fits
ROOT='/workspace/Astro CS Database'
P={'OLD':ROOT+'/run/RELEASE-02/L4-rebuild/p3_r_vis_fixed/output_phase3.fits',
   'NEW':ROOT+'/run/RELEASE-02/L4-rebuild/p3_upmfix/output_phase3.fits'}
D={}
for lbl,p in P.items():
    with fits.open(p,memmap=False) as h:
        print(lbl,'n_hdu',len(h))
        for i,hd in enumerate(h):
            if hd.data is None: print('  hdu',i,hd.name,'no data'); continue
            a=np.asarray(hd.data)
            print('  hdu',i,hd.name,a.dtype,a.shape,'finite',int(np.isfinite(a).sum()),
                  'min %.4g max %.4g med %.4g'%(np.nanmin(a),np.nanmax(a),np.nanmedian(a)))
        sig=np.asarray(h[0].data,dtype=np.float64); cov=np.asarray(h[1].data,dtype=np.float64)
    D[lbl]=sig
    if lbl=='OLD': covref=cov
print()
# fixed-coordinate reproduction of parent numbers
def step_x(d,pos,ylo,yhi,w=20):
    out=[]
    for y in range(ylo,yhi):
        l=np.median(d[y,pos-w:pos]); r=np.median(d[y,pos:pos+w])
        if np.isfinite(l) and np.isfinite(r): out.append(r-l)
    return np.array(out)
def step_y(d,pos,xlo,xhi,w=20):
    out=[]
    for x in range(xlo,xhi):
        l=np.median(d[pos-w:pos,x]); r=np.median(d[pos:pos+w,x])
        if np.isfinite(l) and np.isfinite(r): out.append(r-l)
    return np.array(out)
bgx=np.nanmedian(D['OLD'][300:3600,2145-120:2145+120])
bgy1=np.nanmedian(D['OLD'][1185-120:1185+120,900:1900])
bgy2=np.nanmedian(D['OLD'][1392-120:1392+120,700:2000])
print('OLD local bg: x2145 %.4g  y1185 %.4g  y1392 %.4g'%(bgx,bgy1,bgy2))
for lbl in ('OLD','NEW'):
    d=D[lbl]
    s1=step_x(d,2145,300,3600); s2=step_y(d,1185,900,1900); s3=step_y(d,1392,700,2000)
    print('%s: x2145 %+.4g (%.3f%%) n=%d | y1185 %+.4g (%.3f%%) | y1392 %+.4g (%.3f%%)'%(
        lbl,np.median(s1),100*np.median(s1)/bgx,s1.size,
        np.median(s2),100*np.median(s2)/bgy1,
        np.median(s3),100*np.median(s3)/bgy2))
np.save('/dev/shm/astrocs_cd/do.npy',D['OLD'].astype(np.float32))
np.save('/dev/shm/astrocs_cd/dn.npy',D['NEW'].astype(np.float32))
np.save('/dev/shm/astrocs_cd/cov.npy',(covref>0.5))
print('saved')
