#!/usr/bin/env python3
"""Q2 item 3: does SNR weighting of the gradient fit matter, and does a low-SNR
frame pollute the common gradient field / raise the high-SNR frame's photometry?

Two overlapping frames H (sigma_norm=1) and L (sigma_norm=10) share a common
order-2 background.  L carries a *systematic* additive artifact (flat residual),
which is what inverse-variance weighting is supposed to suppress.  We estimate
the common field over the overlap by weighted order-2 least squares and measure
  bias  = E[common] - truth          (artifact leakage)
  rmse  = sqrt(bias^2 + variance)    (Monte-Carlo over noise)
for three weight choices: SNR-correct (1/sigma^2), equal, inverted.
"""
import numpy as np, json, os
OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 256, 128
X, Y = np.meshgrid(np.arange(W), np.arange(H))
U = (X-128.0)/128.0; V = (Y-64.0)/64.0
def cols(u, v, order=2):
    c=[np.ones_like(u),u,v]
    if order>=2: c+=[u*u,u*v,v*v]
    return c
S  = 1000.0 + 2.0*U + 1.0*V + 0.5*U*U - 0.3*U*V
bH = 1.0 + 0.8*U - 0.5*V + 0.2*U*U + 0.1*U*V
bL = -2.0 + 0.3*U + 0.9*V - 0.4*U*U + 0.2*V*V
mH = (X < 200); mL = (X >= 100)
ov = mH & mL
# systematic artifact in L (uncorrected flat residual), inside the overlap
ART = 20.0*np.exp(-(((X-150.0)**2 + (Y-64.0)**2)/(2*30.0**2)))
SIG = {'H':1.0, 'L':10.0}
def wts(kind):
    if kind=='snr':   return {'H':1/SIG['H']**2, 'L':1/SIG['L']**2}
    if kind=='equal': return {'H':1.0, 'L':1.0}
    if kind=='inv':   return {'H':SIG['H']**2, 'L':SIG['L']**2}   # deliberately wrong
rng = np.random.default_rng(7)

def estimate(nH, nL, w, with_art=True):
    zH = S + bH + nH
    zL = S + bL + (ART if with_art else 0.0) + nL
    m = ov
    A = np.stack(cols(U[m], V[m], 2), axis=1)
    Wd = (w['H'] + w['L'])*np.ones(A.shape[0])
    Hm = (A*(Wd[:,None])).T @ A
    rh = (A*(w['H']*np.ones(A.shape[0]))[:,None]).T @ zH[m] + (A*(w['L']*np.ones(A.shape[0]))[:,None]).T @ zL[m]
    coef = np.linalg.solve(Hm, rh)
    cc = cols(U, V, 2)
    return sum(coef[i]*cc[i] for i in range(len(cc)))

# ---- systematic-artifact leakage (deterministic, no noise) ----
res = {'setup': {'sigma_norm': SIG, 'overlap_px': int(ov.sum()), 'artifact_peak_ADU': float(ART.max())}}
clean = {}
for kind in ('snr','equal','inv'):
    w = wts(kind)
    g_art = estimate(0*ART, 0*ART, w, with_art=True)
    g_cln = estimate(0*ART, 0*ART, w, with_art=False)
    # bias in the common field induced by L's artifact, measured on the overlap
    bias = float(np.median((g_art - g_cln)[ov]))
    frac = w['L']/(w['H']+w['L'])
    res[kind] = {'w_H': w['H'], 'w_L': w['L'], 'weight_fraction_on_L': float(frac),
                 'measured_bias_ADU': bias,
                 'predicted_bias_ADU': float(frac*20.0),
                 'artifact_leak_pct': float(100*bias/20.0)}
print('=== systematic-artifact leakage into the common gradient field ===')
for k in ('snr','equal','inv'):
    print('  %-6s w_L/(w_H+w_L)=%.4f  measured bias=%+.4f ADU  predicted=%+.4f  leak=%.1f%%'%(
        k, res[k]['weight_fraction_on_L'], res[k]['measured_bias_ADU'],
        res[k]['predicted_bias_ADU'], res[k]['artifact_leak_pct']))

# ---- Monte-Carlo: noise-driven RMSE of the common-field estimate ----
print('\n=== Monte-Carlo noise RMSE of the common field (200 draws, no artifact) ===')
for kind in ('snr','equal','inv'):
    w = wts(kind); errs=[]
    truth = estimate(0*ART, 0*ART, w, with_art=False)
    for _ in range(200):
        nH = rng.normal(0, SIG['H'], (H,W)); nL = rng.normal(0, SIG['L'], (H,W))
        g = estimate(nH, nL, w, with_art=False)
        errs.append(float(np.median(g[ov]) - np.median(truth[ov])))
    errs=np.array(errs)
    res[kind]['mc_bias_ADU'] = float(errs.mean())
    res[kind]['mc_rms_ADU']  = float(np.sqrt(np.mean(errs**2)))
    print('  %-6s MC bias=%+.4f  MC RMS=%.4f ADU'%(kind, errs.mean(), np.sqrt(np.mean(errs**2))))

# analytic variance of a constant common estimate: 1/(w_H+w_L) (if w=1/sigma^2)
print('\n  analytic 1/(w_H+w_L): snr=%.4f equal=%.4f inv=%.4f'%(
    1/(wts('snr')['H']+wts('snr')['L']), 1/(wts('equal')['H']+wts('equal')['L']),
    1/(wts('inv')['H']+wts('inv')['L'])))

# ---- full weight formula incl. parameter covariance (ordering check) ----
print('\n=== full Var(corrected) = [sigma_y^2 + J C_theta J^T]/g^2 : ordering ===')
# per-frame order-2 fit covariance on its own footprint, weights 1/sigma_norm^2
frames = {'H': (mH, SIG['H']), 'L': (mL, SIG['L'])}
G = {'H':1.0, 'L':1.3}   # multiplicative gain (example): high-SNR frame has larger g
# corrected = z/g ; Var = sigma_norm^2 + phi^T C_theta phi  (z is already normalized)
# (the /g^2 of the raw formula is already inside sigma_norm = sigma_raw/g)
full = {}
for k,(m,s) in frames.items():
    A = np.stack(cols(U[m],V[m],2),axis=1); ww = (1/s**2)*np.ones(A.shape[0])
    Ct = np.linalg.inv((A*ww[:,None]).T@A)
    # evaluate at the frame centroid (pixel nearest to the mean of the frame)
    p = np.argmin((X-0.5*(X[m].min()+X[m].max()))**2 + (Y-0.5*(Y[m].min()+Y[m].max()))**2)
    phi = np.array(cols(np.array([U.ravel()[p]]), np.array([V.ravel()[p]]),2)).ravel()
    var_stat = s**2
    var_param = float(phi @ Ct @ phi)
    full[k] = {'sigma_norm2': var_stat, 'param_term': var_param,
               'var_full': var_stat+var_param,
               'w_stat_only': 1/var_stat, 'w_full': 1/(var_stat+var_param)}
    print('  %s: sigma_norm^2=%.4f  J C J^T=%.3e  w(stat only)=%.4f  w(full)=%.4f'%(
        k, var_stat, var_param, 1/var_stat, 1/(var_stat+var_param)))
print('  ordering stat-only:', 'H>L' if full['H']['w_stat_only']>full['L']['w_stat_only'] else 'L>H',
      '| ordering full:', 'H>L' if full['H']['w_full']>full['L']['w_full'] else 'L>H')
res['full_formula'] = full

with open(os.path.join(OUT,'pollution_results.json'),'w') as f: json.dump(res,f,indent=1)
print('\nwrote pollution_results.json')
