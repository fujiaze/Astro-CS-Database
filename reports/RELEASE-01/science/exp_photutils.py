
import numpy as np, math, json
from photutils.aperture import CircularAperture, CircularAnnulus, aperture_photometry, ApertureStats
from photutils.background import Background2D, MedianBackground
from astropy.stats import sigma_clipped_stats, mad_std
rng = np.random.default_rng(7)
out={}

# Synthetic Moffat4 beta=4 PSF image, F=5000 ADU, sigma=1.5 px (FWHM=1.845), sky=10 ADU
F=5000.0; sig=1.5; sky=10.0; sky_rms=2.0
n=121; c=60
yy,xx=np.mgrid[0:n,0:n]
r2=(xx-c)**2+(yy-c)**2
P=(1.0+r2/(2*sig*sig))**(-4.0); P/=P.sum()
img = F*P + sky + rng.normal(0,sky_rms,(n,n))

r_ap=3.0  # pixels
aper=CircularAperture((c,c), r=r_ap)
ann=CircularAnnulus((c,c), r_in=6, r_out=9)
# local background from annulus median
bkg_stats=ApertureStats(img, ann)
bkg=bkg_stats.median
# photutils aperture photometry with exact method
phot=aperture_photometry(img, aper, method='exact')
flux_ap=float(phot['aperture_sum'][0]) - bkg*aper.area
# error: sky rms over aperture area (photutils convention)
err_ap=sky_rms*math.sqrt(aper.area)
out['E5_photutils']={'aperture_flux':flux_ap,'bkg_median':float(bkg),
   'aperture_snr_photutils':flux_ap/err_ap,'aperture_area':float(aper.area)}

# AstroCS analytic aperture (snr_science.cpp): f_in=1-(1+r^2/(2 sig^2))^-3 ; n_pix=pi r^2; n_sky=ann area
def astrocs_aperture(F,fwhm,sigma_sky,r,n_sky):
    sigma=fwhm/1.230310
    u=1.0+(r*r)/(2*sigma*sigma)
    f_in=1.0-1.0/(u*u*u)
    n_pix=math.pi*r*r
    s_ap=F*f_in
    var=n_pix*sigma_sky*sigma_sky*(1.0+n_pix/n_sky)
    return s_ap, s_ap/math.sqrt(var), f_in
fwhm=sig*1.230310
s_ap,snr_ap,f_in=astrocs_aperture(F,fwhm,sky_rms,r_ap,math.pi*(9**2-6**2))
out['E5_astrocs']={'f_in':f_in,'s_ap':s_ap,'snr_ap':snr_ap,
   'photutils_over_astrocs_flux':flux_ap/s_ap,'photutils_over_astrocs_snr':(flux_ap/err_ap)/snr_ap}

# AstroCS Horne optimal SNR for the same source
def horne(F,fwhm,sigma_sky,half=60):
    s=fwhm/1.230310
    j,i=np.mgrid[-half:half+1,-half:half+1]
    r2=i.astype(float)**2+j.astype(float)**2
    P=(1.0+r2/(2*s*s))**(-4.0); P/=P.sum()
    varF=1.0/np.sum(P**2/sigma_sky**2)
    sF=math.sqrt(varF); return F/sF,sF
snr_h,sF_h=horne(F,fwhm,sky_rms)
out['E5_horne']={'snr_optimal':snr_h,'sigma_F':sF_h}

# Background2D vs AstroCS-like plane on a gradient field
bg_img = 20.0 + 0.02*xx + 0.01*yy + rng.normal(0,1.0,(n,n))
b2d=Background2D(bg_img,(16,16),filter_size=(3,3),bkg_estimator=MedianBackground())
out['E5_bg2d']={'median_bg_center':float(b2d.background[60,60]),
   'true_center':float(20.0+0.02*60+0.01*60)}
print(json.dumps(out,indent=1))

