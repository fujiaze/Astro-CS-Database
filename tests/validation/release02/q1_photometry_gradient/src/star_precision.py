#!/usr/bin/env python3
"""Q1.2  How precisely can stars pin down the single scalar a_k, and how does that
error leak into the frame-to-frame background step (seam)?

Model:  measured star flux  F_k,i = a_k * F_i * (1 + eta_i),  eta ~ N(0, sigma_ap/F_i)
        a_hat_k = median_i( F_k,i / F_ref,i )   (Phase1-style robust ratio)
Aperture noise sigma_ap is measured empirically from the actual synthetic PSF+noise.
"""
import json, math, os
import numpy as np
rng=np.random.default_rng(7)
OUT=os.path.dirname(os.path.abspath(__file__))
W=H=512; NOISE=5.0; S0=1000.0
# star flux distribution identical in shape to synth_core
sflux=10**rng.uniform(2.0,4.3,400)
# empirical aperture noise for a PSF sigma=2, r_ap=3, annulus 6-9
gy,gx=np.mgrid[-9:10,-9:10]; rr=np.hypot(gx,gy)
ap=rr<=3.0; ann=(rr>=6)&(rr<=9); Nap=ap.sum()
sig_ap = NOISE*math.sqrt(Nap + Nap*Nap/max(ann.sum(),1)*1.0)   # aperture + bg-median term
# verify with a direct Monte Carlo
nsamp=[]
for _ in range(2000):
    sub=rng.normal(0,NOISE,(19,19)); nsamp.append(sub[ap].sum()-sub[ann].mean()*Nap)
sig_ap_mc=float(np.std(nsamp))
def med_err_factor(n):   # asymptotic std of median / std of mean
    return 1.2533 if n>=5 else float('nan')

def mc_eps(N, snr_floor=None, nrep=400, weighting='median'):
    """Return rms and p95 of the relative a_k error for N stars."""
    eps=[]
    for _ in range(nrep):
        idx=rng.choice(len(sflux),size=N,replace=False)
        F=sflux[idx]*0.9                     # true a for this frame
        sig=sig_ap_mc*np.ones(N)
        if snr_floor is not None:            # optional extra noise to force a SNR floor
            sig=np.maximum(sig, F/snr_floor)
        Fm=F+rng.normal(0,sig,N)
        Fr=sflux[idx]                        # reference frame, noiseless
        rho=Fm/Fr
        if weighting=='median': est=np.median(rho)
        else:
            w=1.0/(sig/Fr)**2; est=np.sum(w*rho)/np.sum(w)
        eps.append(est/0.9-1.0)
    e=np.array(eps)
    return float(np.std(e)), float(np.percentile(np.abs(e),95)), float(np.median(np.abs(e)))

rows=[]
print(f"empirical aperture noise sigma_ap = {sig_ap_mc:.2f} ADU  (analytic {sig_ap:.2f})")
print(f"{'N_star':>7} {'eps_rms':>10} {'eps_p95abs':>11} {'eps_medabs':>11} {'seam_step_ADU(p95)':>19}")
for N in [1,3,5,10,20,50,100,200,400]:
    rms,p95,mabs=mc_eps(N,nrep=600)
    rows.append(dict(N=N,eps_rms=rms,eps_p95abs=p95,eps_medabs=mabs,seam_step_p95=p95*S0))
    print(f"{N:>7} {rms:>10.5f} {p95:>11.5f} {mabs:>11.5f} {p95*S0:>19.2f}")
# SNR floor study
rows_snr=[]
print("\nSNR floor (N=50):")
print(f"{'SNR_floor':>10} {'eps_rms':>10} {'seam_step_p95':>15}")
for snr in [5,10,20,50,100,300]:
    rms,p95,mabs=mc_eps(50,snr_floor=snr,nrep=600)
    rows_snr.append(dict(snr=snr,eps_rms=rms,seam_step_p95=p95*S0))
    print(f"{snr:>10} {rms:>10.5f} {p95*S0:>15.2f}")
# inverse-variance weighting vs median (which is better?)
print("\nweighting comparison (N=20, faint-heavy sample):")
rows_w=[]
for wname in ('median','invvar'):
    rms,p95,mabs=mc_eps(20,weighting=wname,nrep=600)
    rows_w.append(dict(weighting=wname,eps_rms=rms,seam_step_p95=p95*S0))
    print(f"  {wname:>8}: eps_rms={rms:.5f}  seam_p95={p95*S0:.2f} ADU")
# negative control: no noise -> error must be 0
eps=[]
for _ in range(200):
    idx=rng.choice(len(sflux),size=20,replace=False)
    rho=(0.9*sflux[idx])/sflux[idx]
    eps.append(np.median(rho)/0.9-1.0)
print(f"\nNEGATIVE CONTROL (zero star noise, N=20): eps_rms={np.std(eps):.2e} (must be ~0)")
json.dump(dict(sig_ap_mc=sig_ap_mc,sig_ap_analytic=sig_ap,S0=S0,noise=NOISE,
               nstar=rows,snr=rows_snr,weighting=rows_w,neg_control_rms=float(np.std(eps))),
          open(os.path.join(OUT,'star_precision.json'),'w'),indent=1)
print("wrote star_precision.json")
