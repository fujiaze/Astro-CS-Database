import sys, numpy as np
sys.path.insert(0,'run/RELEASE-01/e2e')
from fits_probe import read_fits
for g in ('m42','gc'):
    for kind in ('full','vis'):
        p='run/RELEASE-01/e2e/l4/p3_%s_%s/output_phase3.fits'%(g,kind)
        h,d,a=read_fits(p)
        a=a.reshape(d[1],d[0])
        nan=~np.isfinite(a)
        finite=np.where(nan,np.nan,a)
        med=np.nanmedian(finite,axis=1)
        dif=np.diff(med); sc=np.nanmedian(np.abs(dif))+1e-30; j=np.abs(dif)/sc
        idx=np.argsort(np.nan_to_num(j,nan=-1))[-5:]
        print('%-8s %-4s dims=%s nan=%.4f%% zero=%.4f%% finite_med=%.4g'%(g,kind,d,100*nan.mean(),100*(a==0).mean(),np.nanmedian(finite)))
        print('        top row-jumps:', sorted((int(i),round(float(j[i]),1)) for i in idx))