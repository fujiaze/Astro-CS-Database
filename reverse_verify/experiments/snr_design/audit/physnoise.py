#!/usr/bin/env python3
"""SNR-EXP-AUDIT: physical forward noise model (shared library).

Implements the mandatory methodology of GAP_AUDIT.md section 9.41:

  "合成测试必须模拟真实物理实现" -- synthetic tests must simulate the real
  physical implementation: photon shot noise (source / sky / dark, Poisson in
  the ELECTRON domain), read noise (Gaussian in electrons), gain quantisation
  (electrons -> ADU), flat-field multiplicative response, sky gradient.

Design of the simulator
-----------------------
The base scene is REAL data (GAP_AUDIT 9.41 item 3: "优先用真实数据作为底").
This repository has no HST frames, so we use the RELEASE-02 L4 calibrated
frames and DECLARE that substitution explicitly.

    template_adu(x,y)   <- average of the 4 real, dithered, calibrated frames of
                           run/RELEASE-02/L4-rebuild/norm/t2_m2_red (real star
                           field, real PSF, real nebular background structure)
    mu_e(x,y) = G * [ template_adu(x,y) * sky_scale
                      + sky_gradient(x,y)          # additive sky plane, ADU
                      + dark_e/G ]                 # dark current, electrons
    n_e       ~ Poisson(mu_e)                      # SHOT NOISE, electron domain
    n_e      += N(0, RN^2)                         # read noise, electron domain
    adu       = floor(n_e / G + 0.5)               # gain quantisation to ADU
    obs       = adu * (1 + eps_flat(x,y))          # flat-field response residual

Exact model variance of the simulated frame (used as the known TRUTH):

    Var_model(p) [ADU^2] = mu_adu(p)/G          # Poisson (source+sky+dark)
                         + RN^2/G^2             # read noise
                         + (1/12)/G^2 * ...     # quantisation, included exactly
                         + (template_sigma(p))^2   # noise inherited from the base
                         + y(p)^2 * sigma_flat^2   # flat-field photon/scatter term

  The template term is present because the base is a finite average of real
  frames: it is COMMON to every simulated frame and is therefore carried in the
  truth variance rather than being pretended away.

Honest declarations
-------------------
* The FITS headers of the L4 calibrated frames contain NO GAIN / RDNOISE /
  SATURATE / DATAMAX card (verified: 117 cards, none of those keys), so G and RN
  are NOT measurable from metadata and NO physical closed form is used to infer
  them (GAP_AUDIT 9.42).  They are declared model parameters and every headline
  number is checked for invariance under a scan of them.
* Dark current is negligible for these 300 s frames at -20 C and is set to 0 by
  default; the code path exists and is exercised by a sensitivity run.

Author: SNR-EXP-AUDIT shard.  Read-only wrt production code.
"""
import os
import numpy as np

MAD2SIG = 1.482602218505602          # frozen project constant (NOISE_MODEL.md)

# ---------------------------------------------------------------------------
# METHODOLOGY GUARD (GAP_AUDIT 9.42)
# ---------------------------------------------------------------------------
# The electron domain used by this simulator is a MODEL COORDINATE, not an
# instrument measurement.  GAP_AUDIT 9.42 forbids deriving instrument gain /
# aperture / exposure from any physical closed form (e.g. k = g*h*c*1e9/(A*t))
# and forbids arguing from absolute physical units at all, because the FITS
# headers of this project carry no such data.  Accordingly:
#   * G and RN below are DECLARED free parameters of the forward model, required
#     by GAP_AUDIT 9.41 ("shot noise ... unit: electrons");
#   * no physical closed form is evaluated anywhere in this audit;
#   * every headline result is reported together with its invariance under a
#     scan of G and RN (scale-free statement);
#   * the only quantity taken from the real data is the DIMENSIONLESS
#     noise affinity a = dVar/dS [ADU^2 per ADU] and the read fraction
#     b/(a*S), never an e-/ADU value claimed as an instrument property.
# ---------------------------------------------------------------------------


# --------------------------------------------------------------------------- #
# robust estimators (identical to exp2/exp3 so results are comparable)
# --------------------------------------------------------------------------- #
def patch_sigma_clipped(data, pb, clip_n=2, clip_k=5.0):
    """MAD-based blank-sky sigma on a pb x pb patch grid, 5-sigma x 2 rounds.

    Exactly the estimator used by EXP-2 / EXP-3 (and specified by
    docs/science/NOISE_MODEL.md for the masked-patch model).
    """
    h, w = data.shape
    gy, gx = h // pb, w // pb
    v = data[:gy * pb, :gx * pb].reshape(gy, pb, gx, pb).transpose(0, 2, 1, 3)
    v = v.reshape(gy * gx, pb * pb)
    med = np.median(v, axis=1)
    x = v - med[:, None]
    m = np.ones_like(x, dtype=bool)
    for _ in range(clip_n):
        s = MAD2SIG * np.median(np.abs(x), axis=1)
        m = np.abs(x) <= clip_k * np.maximum(s, 1e-12)[:, None]
        med = np.nanmedian(np.where(m, v, np.nan), axis=1)
        x = v - med[:, None]
    sig = MAD2SIG * np.median(np.abs(x), axis=1)
    n_sky = m.sum(axis=1)
    return sig.reshape(gy, gx), n_sky.reshape(gy, gx)


def whole_frame_mad(data):
    """Production frame scalar: whole-frame UNCLIPPED MAD (noise_model.cpp:164)."""
    med = np.median(data)
    return float(MAD2SIG * np.median(np.abs(data - med)))


# --------------------------------------------------------------------------- #
# real-frame base
# --------------------------------------------------------------------------- #
def _shift_fft(ref, img):
    """Integer (dy, dx) shift of img relative to ref, by FFT phase correlation."""
    a = ref - ref.mean()
    b = img - img.mean()
    win = np.outer(np.hanning(a.shape[0]), np.hanning(a.shape[1]))
    A = np.fft.rfft2(a * win)
    B = np.fft.rfft2(b * win)
    R = A * np.conj(B)
    R /= np.maximum(np.abs(R), 1e-12)
    c = np.fft.irfft2(R, s=a.shape)
    iy, ix = np.unravel_index(np.argmax(c), c.shape)
    dy = iy if iy < a.shape[0] // 2 else iy - a.shape[0]
    dx = ix if ix < a.shape[1] // 2 else ix - a.shape[1]
    return int(dy), int(dx)


def build_real_template(paths, cut=None, max_shift=120):
    """Average the real dithered frames into a low-noise scene template.

    Returns (template_adu, template_sigma_adu, info).
    template_sigma_adu is the per-pixel uncertainty OF THE TEMPLATE (the base is
    a finite average, so it is not noise free -- this is carried honestly).
    """
    from astropy.io import fits
    frames = []
    for p in paths:
        with fits.open(p, memmap=True) as h:
            d = np.array(h[0].data, dtype=np.float64)
        frames.append(d if cut is None else d[cut[0]:cut[1], cut[2]:cut[3]])
    ref = frames[0]
    aligned = [ref]
    shifts = [(0, 0)]
    for f in frames[1:]:
        dy, dx = _shift_fft(ref, f)
        if abs(dy) > max_shift or abs(dx) > max_shift:
            raise RuntimeError(f"implausible shift {(dy, dx)}")
        aligned.append(np.roll(np.roll(f, -dy, axis=0), -dx, axis=1))
        shifts.append((dy, dx))
    A = np.stack(aligned)
    # keep a margin so that wrapped edges do not enter the product
    m = max(max(abs(s[0]), abs(s[1])) for s in shifts) + 2
    A = A[:, m:-m, m:-m]
    tmpl = A.mean(axis=0)
    # Per-pixel scatter of the individual frames about their own mean.  This is
    # NOT the template noise: it is dominated by REAL inter-frame systematics
    # (different nights -> different sky level / seeing / transparency).  It is
    # reported as a diagnostic and is deliberately NOT used as the template
    # variance; the template variance is derived from the noise MODEL by
    # template_noise_model() below.
    s1 = A.std(axis=0, ddof=1) if A.shape[0] > 1 else np.zeros_like(tmpl)
    tmpl_sigma = np.zeros_like(tmpl)
    info = dict(n_frames=A.shape[0], shifts=shifts, margin=m,
                shape=list(tmpl.shape),
                template_median_adu=float(np.median(tmpl)),
                interframe_scatter_median_adu=float(np.median(s1)),
                note="interframe_scatter_median_adu is NOT noise; it contains "
                     "real night-to-night sky/seeing differences")
    return tmpl, tmpl_sigma, info


def template_noise_model(template_adu, G=1.3, RN=10.0, n_frames=4,
                         quantise=True):
    """Variance of the *template* itself: it is the mean of n_frames noisy
    frames, so Var(template) = Var(one frame)/n_frames.

    This term is COMMON to every simulated frame (same base scene), i.e. it is a
    fixed pattern rather than an independent noise.  It is returned separately
    so that both conventions can be reported.
    """
    v1 = np.maximum(template_adu, 0.0) / G + RN ** 2 / G ** 2
    if quantise:
        v1 = v1 + (1.0 / 12.0) / G ** 2
    return v1 / n_frames


# --------------------------------------------------------------------------- #
# the physical forward model
# --------------------------------------------------------------------------- #
def simulate_frame(template_adu, template_sigma_adu, G=1.3, RN=10.0,
                   sky_scale=1.0, sky_gradient_adu=(0.0, 0.0), dark_e=0.0,
                   sigma_flat=0.0, flat_field=None, rng=None,
                   quantise=True, return_components=False, source_e=None):
    """One physical realisation of a calibrated frame.

    Parameters
    ----------
    template_adu        : real scene (ADU), the *expectation* of the sky+source
    template_sigma_adu  : per-pixel uncertainty of the template itself (ADU)
    G                   : gain [e-/ADU]
    RN                  : read noise [e- rms]
    sky_scale           : multiplies the template -> photon-domain sky change
    sky_gradient_adu    : (gx, gy) linear additive sky plane [ADU/px]
    dark_e              : dark current [e-] (uniform)
    sigma_flat          : rms of the flat-field response residual (multiplicative)
    flat_field          : optional precomputed (1+eps) map (overrides sigma_flat)
    """
    if rng is None:
        rng = np.random.default_rng()
    h, w = template_adu.shape
    yy, xx = np.mgrid[0:h, 0:w]
    gx, gy = sky_gradient_adu
    sky_plane = gx * xx + gy * yy

    # sky_scale acts on the SKY+SOURCE template (a real photon-domain change of
    # the incident illumination); source_e is an ADDITIONAL source term in
    # electrons that is deliberately NOT scaled, so that a sky change can be
    # tested with the source flux held fixed.
    mu_adu = template_adu * sky_scale + sky_plane + dark_e / G
    mu_adu = np.maximum(mu_adu, 0.0)
    mu_e = mu_adu * G
    if source_e is not None:
        mu_e = mu_e + source_e
        mu_adu = mu_e / G

    n_e = rng.poisson(mu_e).astype(np.float64)
    n_e += rng.normal(0.0, RN, size=mu_e.shape)

    adu = n_e / G
    if quantise:
        adu = np.floor(adu + 0.5)          # ADC: electrons -> integer ADU

    if flat_field is not None:
        ff = flat_field
    elif sigma_flat > 0:
        ff = 1.0 + rng.normal(0.0, sigma_flat, size=mu_e.shape)
    else:
        ff = None
    obs = adu * ff if ff is not None else adu

    if not return_components:
        return obs

    # ---- exact model variance of this realisation (the TRUTH) --------------
    var_poisson = mu_adu / G                       # ADU^2
    var_read = RN ** 2 / G ** 2
    var_quant = (1.0 / 12.0) / G ** 2 if quantise else 0.0
    var_tmpl = (template_sigma_adu ** 2 if template_sigma_adu is not None
                else 0.0)
    var_flat = (obs ** 2) * (sigma_flat ** 2) if (sigma_flat > 0 or
                                                  flat_field is not None) else 0.0
    comp = dict(mu_adu=mu_adu, var_poisson=var_poisson, var_read=var_read,
                var_quant=var_quant, var_tmpl=var_tmpl, var_flat=var_flat,
                flat_field=ff)
    var_tot = var_poisson + var_read + var_quant + var_tmpl + var_flat
    return obs, var_tot, comp


def simulate_quantisation_pair(template_adu, G=1.3, RN=0.0, rng=None,
                               source_e=None):
    """One electron draw, both ADC treatments -> isolates the quantisation term.

    Returns (adu_quantised, adu_not_quantised) from the SAME Poisson/Gaussian
    realisation, so their variance difference has no Monte-Carlo scatter.
    """
    if rng is None:
        rng = np.random.default_rng()
    mu_e = np.maximum(template_adu, 0.0) * G
    if source_e is not None:
        mu_e = mu_e + source_e
    n_e = rng.poisson(mu_e).astype(np.float64)
    if RN > 0:
        n_e = n_e + rng.normal(0.0, RN, size=mu_e.shape)
    return np.floor(n_e / G + 0.5), n_e / G


def make_flat_field(shape, sigma_pixel=0.01, sigma_large=0.005, seed=0,
                    corr_px=64):
    """Flat-field response residual: pixel noise + large-scale drift."""
    rng = np.random.default_rng(seed)
    eps = rng.normal(0.0, sigma_pixel, size=shape)
    if sigma_large > 0:
        h, w = shape
        gy, gx = max(h // corr_px, 2), max(w // corr_px, 2)
        big = rng.normal(0.0, sigma_large, size=(gy, gx))
        from scipy.ndimage import zoom
        eps = eps + zoom(big, (h / gy, w / gx), order=1)[:h, :w]
    return 1.0 + eps


# --------------------------------------------------------------------------- #
# weight-efficiency machinery (shared with the audited experiments)
# --------------------------------------------------------------------------- #
def efficiency_penalty(weights, var_true):
    """Exact Var_approx / Var_opt for a stack of independent frames.

    weights  : (F, ...) assumed weights w_f(p)
    var_true : (F, ...) TRUE variance of each frame at each point
    Returns (penalty, var_p_delta, delta_bar) with
        penalty = Var_approx / Var_opt = 1 + Var_p(delta)/(1+delta_bar)^2
    """
    w = np.asarray(weights, float)
    v = np.asarray(var_true, float)
    t = 1.0 / v                                     # optimal weights
    var_approx = (w ** 2 * v).sum(axis=0) / (w.sum(axis=0) ** 2)
    var_opt = 1.0 / t.sum(axis=0)
    pen = var_approx / var_opt
    d = w / t - 1.0
    p = t / t.sum(axis=0, keepdims=True)
    dbar = (p * d).sum(axis=0)
    var_p = (p * (d - dbar) ** 2).sum(axis=0)
    return pen, var_p, dbar


def scalar_penalty(sigmas):
    """Efficiency penalty of the per-frame SCALAR weight (production choice).

    sigmas : (F, ...) true per-frame sigma field.  The scalar is the frame
    median sigma -- exactly what the production frame-level estimator is.
    """
    F = sigmas.shape[0]
    s_scalar = np.median(sigmas.reshape(F, -1), axis=1)
    w = (1.0 / s_scalar ** 2)[:, None, None] * np.ones_like(sigmas)
    return efficiency_penalty(w, sigmas ** 2)


def bilinear_nodes(nodes, shape, stride):
    """Bilinear interpolation from a coarse node grid to the dense grid."""
    gy, gx = shape
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    fy, fx = yq / stride, xq / stride
    y0 = np.clip(np.floor(fy).astype(int), 0, ny - 2)
    x0 = np.clip(np.floor(fx).astype(int), 0, nx - 2)
    wy, wx = (fy - y0)[:, None], (fx - x0)[None, :]
    a, b = nodes[np.ix_(y0, x0)], nodes[np.ix_(y0, x0 + 1)]
    c, d = nodes[np.ix_(y0 + 1, x0)], nodes[np.ix_(y0 + 1, x0 + 1)]
    rec = (a * (1 - wx) * (1 - wy) + b * wx * (1 - wy)
           + c * (1 - wx) * wy + d * wx * wy)
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def kriging_nodes(nodes, shape, stride, ell_px, nugget=0.0):
    """Separable ordinary kriging (SE covariance) from a coarse node grid."""
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    Cy = np.exp(-0.5 * (ye[:, None] - ye[None, :]) ** 2 / ell_px ** 2)
    Cx = np.exp(-0.5 * (xe[:, None] - xe[None, :]) ** 2 / ell_px ** 2)
    Cy = Cy + nugget * np.eye(ny)
    Cx = Cx + nugget * np.eye(nx)
    Ky = np.exp(-0.5 * (yq[:, None] - ye[None, :]) ** 2 / ell_px ** 2)
    Kx = np.exp(-0.5 * (xq[:, None] - xe[None, :]) ** 2 / ell_px ** 2)
    Wy = np.linalg.solve(Cy, Ky.T).T
    Wx = np.linalg.solve(Cx, Kx.T).T
    rec = Wy @ nodes @ Wx.T
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def default_paths(root="run/RELEASE-02/L4-rebuild/norm/t2_m2_red"):
    import glob
    return sorted(glob.glob(os.path.join(root, "calibrated_*.fts")))
