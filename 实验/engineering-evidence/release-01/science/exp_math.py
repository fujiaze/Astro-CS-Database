
import numpy as np, json, math
rng = np.random.default_rng(20260917)
out = {}

# E1: MAD->sigma and Sn normalization (Monte Carlo, N=2e6 Gaussian)
N = 4_000_000
x = rng.standard_normal(N)
mad = np.median(np.abs(x - np.median(x)))
sn = 1.1926 * np.median([np.median(np.abs(x[i]-x)) for i in range(0, 500, 1)])  # rough placeholder
out['E1'] = {'mad': float(mad), 'mad_to_sigma': float(1.0/mad), 'target': 1.482602218505602}

# Sn estimator (Rousseeuw-Croux 1993): Sn = 1.1926 * median_i ( median_j |x_i - x_j| )
M = 2000
xs = rng.standard_normal(M)
d = np.abs(xs[:,None]-xs[None,:])
inner = np.median(d, axis=1)
sn_raw = np.median(inner)
out['E1']['sn_raw_median'] = float(sn_raw)
out['E1']['sn_to_sigma'] = float(1.0/sn_raw)
out['E1']['pixinsight_sn_const'] = 2.03636

# E2: Moffat4 beta=4 discrete profile: relation FWHM = f * sigma, and A_NEA
def moffat4_profile(sigma, half):
    j,i = np.mgrid[-half:half+1, -half:half+1]
    r2 = i.astype(float)**2 + j.astype(float)**2
    v = (1.0 + r2/(2.0*sigma*sigma))**(-4.0)
    return v
sig = 3.0
v = moffat4_profile(sig, 200)
v = v/v.sum()
A_NEA = 1.0/np.sum(v**2)
# radial half-max: solve on a fine 1D profile
r = np.linspace(0, 60, 200000)
I = (1.0 + r**2/(2*sig**2))**(-4.0)
rh = np.interp(0.5, I[::-1], r[::-1])
out['E2'] = {'fwhm_over_sigma_computed': float(2*rh/sig), 'astrocs_factor': 1.230310,
             'A_NEA_px2': float(A_NEA), 'A_NEA_analytic_ratio': float(A_NEA/(2*math.pi*sig*sig))}

# E3: optimal extraction + inverse-variance combination; w = SNR^2/Fref^2
# 1-D Gaussian PSF (analytic Horne) on two frames with different sigma_sky
def horne_sigmaF(F, fwhm, sigma_sky, gain=None, rn=None, half=60, n_pix_sky=None):
    s = fwhm/2.3548200450309493  # Gaussian
    j,i = np.mgrid[-half:half+1, -half:half+1]
    r2 = i.astype(float)**2 + j.astype(float)**2
    P = np.exp(-r2/(2*s*s)); P /= P.sum()
    var_i = sigma_sky**2
    if gain: var_i = var_i + F*P/gain + (rn/gain)**2
    varF = 1.0/np.sum(P**2/var_i)
    return math.sqrt(varF), P
frames = [{'F':1000.0,'fwhm':3.0,'sig':5.0},{'F':1000.0,'fwhm':4.0,'sig':8.0}]
res=[]
for fr in frames:
    sF,P = horne_sigmaF(fr['F'], fr['fwhm'], fr['sig'])
    res.append({'sigma_F':sF,'snr_F':fr['F']/sF,'w':1.0/sF**2})
Fref = 1000.0
out['E3'] = {'frames':res,
  'w_eq_snr2_over_Fref2':[ (r['w'] - (r['snr_F']**2)/Fref**2)/r['w'] for r in res],
  'snr_comb_sq': float(sum(r['snr_F']**2 for r in res)),
  'snr_comb_direct_sq': float((sum(fr['F']/r['sigma_F']**2 for fr,r in zip(frames,res))**2)/sum(1/r['sigma_F']**2 for r in res))}

# E4: sky-noise sensitivity of SNR_F (fixed F, fwhm) vs sigma_sky
sky = [1,2,5,10,20,40]
snrs=[]
for sk in sky:
    sF,_ = horne_sigmaF(1000.0,3.0,float(sk))
    snrs.append(1000.0/sF)
out['E4'] = {'sigma_sky':sky,'snr_F':snrs,'ratio_vs_sky':[snrs[i]/snrs[0] for i in range(len(sky))]}

# E6: PSFSW 'noise' component = MAD of common-star fluxes, vs sky noise
# common star set spanning 2 dex in flux (magnitude range), measure per-frame MAD as sigma_sky grows
fluxes_true = 10**rng.uniform(2,4,size=200)   # 100..10000 ADU spread
for sk in [2.0, 8.0, 32.0]:
    # measurement error per star ~ sky_noise * sqrt(A_NEA); A_NEA ~ few px^2
    A_NEA = 10.0
    meas = fluxes_true + rng.normal(0, sk*math.sqrt(A_NEA), size=fluxes_true.size)
    mad_flux = 1.482602218505602*np.median(np.abs(meas-np.median(meas)))
    out.setdefault('E6',{})[f'sky_{sk}'] = {'mad_flux': float(mad_flux),
        'intrinsic_mad_flux': float(1.482602218505602*np.median(np.abs(fluxes_true-np.median(fluxes_true))))}
print(json.dumps(out, indent=1))

