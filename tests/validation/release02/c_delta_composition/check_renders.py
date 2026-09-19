
import numpy as np
from astropy.io import fits
ROOT='/workspace/Astro CS Database'
for lbl,p in (('p3_r_vis',ROOT+'/run/RELEASE-02/L4-rebuild/p3_r_vis/output_phase3.fits'),
              ('p3_r_vis_fixed',ROOT+'/run/RELEASE-02/L4-rebuild/p3_r_vis_fixed/output_phase3.fits'),
              ('p3_upmfix',ROOT+'/run/RELEASE-02/L4-rebuild/p3_upmfix/output_phase3.fits')):
    with fits.open(p,memmap=False) as h:
        a=np.asarray(h[0].data,dtype=np.float64)
    print('%-16s med %.4g  p16 %.4g p84 %.4g  finite %d'%(lbl,np.nanmedian(a),np.nanpercentile(a,16),np.nanpercentile(a,84),int(np.isfinite(a).sum())))
