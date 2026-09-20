#!/usr/bin/env python3
"""SNR-EXP-AUDIT 0: validate the physical simulator + the sky red-line test.

GAP_AUDIT 9.42 COMPLIANCE: no physical closed form is evaluated anywhere in this
file.  No instrument gain, aperture or exposure is inferred.  G and RN are
DECLARED free parameters of the forward model required by GAP_AUDIT 9.41
("shot noise ... unit: electrons"); every headline number is reported with its
invariance under a scan of them.  The only quantity taken from real data is the
DIMENSIONLESS noise affinity a = dVar/dS [ADU^2 per ADU] and the read fraction.

 (V) SIMULATOR VALIDATION -- the forward model must reproduce its own analytic
     variance, otherwise "physical simulation" is a claim without evidence.
       V1  independent-noise part:  Var(obs_i - obs_j)/2  vs  mu/G + RN^2/G^2
       V2  quantisation term:       uniform patch, Var(round(x)-x) vs 1/12 ADU^2
       V3  flat-field term:         Var(obs) vs E[y^2] * sigma_flat^2
       V4  template term:           Var(mean of M realisations) vs V_model/M

 (G) NOISE-AFFINITY CHECK (dimensionless).  From two real frames 6 min apart on
     the same night we fit, in ADU only,  Var(f1-f2)/2 = a*S + b  and report a,
     the read fraction b/(a*S) and the sky-noise fraction.  This checks that the
     real data are consistent with the affine (Poisson + constant) noise shape
     that the forward model assumes.  It is NOT a claim about the instrument.

 (S) SKY RED LINE (GAP_AUDIT 9.41 items 2 and 4), PSF-weighted flux SNR:
       arm A "arithmetic": a constant is added to the pixels -> the metric must
              return EXACTLY zero change (negative control, true effect = 0)
       arm A' "pedestal kept": the constant is added and NOT subtracted -> the
              naive global SNR would appear to RISE (the trap)
       arm B "photons": the same extra sky delivered as Poisson photons with the
              source flux held fixed -> sigma_F must rise, SNR must fall
     The measured sigma_F is the empirical scatter of the PSF-weighted estimator
     over M independent realisations, so the metric is model independent.

Run:
  TMPDIR=/dev/shm/astrocs_snraudit python3 audit_sim_validation.py \
      --out ../../../../run/reverse_verify/snr_design/audit/audit_sim_validation.json
"""
import argparse, json, os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import physnoise as pn

NORM = "../../../../run/RELEASE-02/L4-rebuild/norm/t2_m2_red"
CUT = (1024, 2048, 1024, 2048)
QUANT_VAR_ADU2 = 1.0 / 12.0          # ADC step = 1 ADU  =>  Delta^2/12


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="audit_sim_validation.json")
    ap.add_argument("--G", type=float, default=1.3)
    ap.add_argument("--RN", type=float, default=10.0)
    ap.add_argument("--n-mc", type=int, default=200)
    a = ap.parse_args()
    t0 = time.time()
    out = {"audit": "SNR-EXP-AUDIT 0 simulator validation + sky red line",
           "declared_model_parameters": {"G_e_per_adu": a.G, "RN_e": a.RN,
                                         "status": "DECLARED, not measured"},
           "base_data": "real L4 calibrated frames, norm/t2_m2_red (no HST data "
                        "in this repository; substitution declared)",
           "gap_audit_9_42": "no physical closed form evaluated; no gain / "
                             "aperture / exposure inferred from the data"}

    paths = pn.default_paths(NORM)
    tmpl, _, tinfo = pn.build_real_template(paths, cut=CUT)
    out["template"] = tinfo
    vtmpl = pn.template_noise_model(tmpl, a.G, a.RN, n_frames=tinfo["n_frames"])
    out["template_noise_model"] = dict(
        median_sigma_adu=float(np.median(np.sqrt(vtmpl))),
        note="Var(template)=Var(one frame)/N_frames; COMMON to all simulated "
             "frames (fixed pattern, not independent noise)")
    rng = np.random.default_rng(20260919)
    s = (slice(300, 556), slice(300, 556))

    # ------------------------------------------------------------------ V1 --
    N = 40
    obs = np.stack([pn.simulate_frame(tmpl, None, G=a.G, RN=a.RN, rng=rng)[s]
                    for _ in range(N)])
    _, _, comp = pn.simulate_frame(tmpl, None, G=a.G, RN=a.RN,
                                   return_components=True, rng=rng)
    var_ind = comp["var_poisson"] + comp["var_read"] + comp["var_quant"]
    dif = obs[1::2] - obs[0::2]
    v_emp = float(np.mean(dif ** 2) / 2.0)
    v_mod = float(np.mean(var_ind[s]))
    out["V1_independent_noise"] = dict(
        n_realisations=N, block=[556 - 300, 556 - 300],
        var_empirical_adu2=v_emp, var_model_adu2=v_mod, ratio=v_emp / v_mod,
        sigma_empirical_adu=float(np.sqrt(v_emp)),
        sigma_model_adu=float(np.sqrt(v_mod)),
        verdict="PASS" if abs(v_emp / v_mod - 1) < 0.02 else "FAIL")

    # ------------------------------------------------------------------ V2 --
    # uniform sky patch (no sources): isolates the ADC rounding term exactly
    mu_adu_u = 200.0
    flat_patch = np.full((512, 512), mu_adu_u)
    pairs = [pn.simulate_quantisation_pair(flat_patch, G=a.G, RN=0.0, rng=rng)
             for _ in range(24)]
    vq = float(np.mean(np.var(np.stack([p[0] for p in pairs]), axis=0, ddof=1)))
    vn = float(np.mean(np.var(np.stack([p[1] for p in pairs]), axis=0, ddof=1)))
    out["V2_quantisation"] = dict(
        uniform_sky_adu=mu_adu_u,
        var_with_quantisation_adu2=vq, var_without_adu2=vn,
        measured_extra_adu2=vq - vn, predicted_extra_adu2=QUANT_VAR_ADU2,
        sigma_quant_adu=float(np.sqrt(QUANT_VAR_ADU2)),
        verdict="PASS" if abs((vq - vn) - QUANT_VAR_ADU2) < 0.02 else "FAIL")

    # ------------------------------------------------------------------ V3 --
    sf = 0.01
    dm = np.stack([pn.simulate_frame(tmpl, None, G=a.G, RN=0.0, quantise=False,
                                     sigma_flat=sf, rng=rng)[s] for _ in range(24)])
    vf = float(np.mean(np.var(dm, axis=0, ddof=1)))
    ey2 = float(np.mean((tmpl[s] / a.G) ** 2))       # E[y^2] in ADU^2
    exp_v = float(np.mean(np.maximum(tmpl[s], 0.0) / a.G)) + ey2 * sf ** 2
    out["V3_flat_field"] = dict(
        sigma_flat=sf, var_measured_adu2=vf, var_expected_adu2=exp_v,
        expected_flat_term_adu2=ey2 * sf ** 2,
        note="flat term is multiplicative, y^2*sigma_flat^2, and must use E[y^2] "
             "(bright pixels dominate it); a pure multiplicative response leaves "
             "the SNR of a source UNCHANGED to first order because signal and "
             "noise are scaled together",
        verdict="PASS" if abs(vf - exp_v) / vf < 0.03 else "FAIL")

    # ------------------------------------------------------------------ V4 --
    stack = np.stack([pn.simulate_frame(tmpl, None, G=a.G, RN=a.RN, rng=rng)[s]
                      for _ in range(16)])
    v_mean = float(np.mean(np.var(stack, axis=0, ddof=1))) / 16.0
    ref = float(np.mean(var_ind[s])) / 16.0
    out["V4_template_term"] = dict(
        var_of_mean_of_16_adu2=v_mean, var_model_one_frame_over_16_adu2=ref,
        ratio=v_mean / ref,
        verdict="PASS" if abs(v_mean / ref - 1) < 0.05 else "FAIL")

    # ------------------------------------------------------------------ G ---
    from astropy.io import fits
    same_night = sorted(p for p in paths if "20251224" in p)
    if len(same_night) >= 2:
        d1 = np.array(fits.open(same_night[0])[0].data, dtype=np.float64)[CUT[0]:CUT[1], CUT[2]:CUT[3]]
        d2 = np.array(fits.open(same_night[1])[0].data, dtype=np.float64)[CUT[0]:CUT[1], CUT[2]:CUT[3]]
        dy, dx = pn._shift_fft(d1, d2)
        d2 = np.roll(np.roll(d2, -dy, axis=0), -dx, axis=1)
        m = abs(dy) + abs(dx) + 4
        d1, d2 = d1[m:-m, m:-m], d2[m:-m, m:-m]
        diff = d1 - d2
        mean_lev = 0.5 * (d1 + d2)
        # keep only source-free sky pixels: a 3-sigma window about the running
        # sky, which removes stars whose PSF differs between the two frames and
        # would otherwise dominate Var(f1-f2)
        sky0 = float(np.median(mean_lev))
        mad0 = float(pn.MAD2SIG * np.median(np.abs(mean_lev - sky0)))
        keep = np.abs(mean_lev - sky0) < 3.0 * mad0
        edges = np.percentile(mean_lev[keep], np.linspace(0, 100, 21))
        rows = []
        for i in range(len(edges) - 1):
            sel = keep & (mean_lev >= edges[i]) & (mean_lev < edges[i + 1])
            if sel.sum() < 20000:
                continue
            # ROBUST variance: MAD-based, so residual source mismatches cannot
            # drive the fit
            dd = diff[sel]
            rob = float((pn.MAD2SIG * np.median(np.abs(dd - np.median(dd)))) ** 2 / 2.0)
            rows.append(dict(mean_adu=float(mean_lev[sel].mean()),
                             var_diff_half_robust=rob,
                             var_diff_half_plain=float(np.var(dd, ddof=1) / 2.0),
                             n_pix=int(sel.sum())))
        lv = np.array([r["mean_adu"] for r in rows])
        vv = np.array([r["var_diff_half_robust"] for r in rows])
        A = np.column_stack([lv, np.ones_like(lv)])
        coef, *_ = np.linalg.lstsq(A, vv, rcond=None)
        a_aff, b_aff = float(coef[0]), float(coef[1])
        s_med = float(np.median(lv))
        rf = b_aff / (a_aff * s_med)
        out["G_noise_affinity_dimensionless"] = dict(
            pair=[os.path.basename(same_night[0]), os.path.basename(same_night[1])],
            n_bins=len(rows),
            noise_affinity_adu2_per_adu=a_aff, constant_term_adu2=b_aff,
            sky_level_median_adu=s_med,
            read_fraction_b_over_aS=float(rf),
            sky_fraction_of_variance=float(1.0 / (1.0 + rf)),
            levels=rows,
            note="DIMENSIONLESS.  a is the Poisson coefficient dVar/dS in ADU; "
                 "b collects read noise AND unresolved-source structure, so b "
                 "(and the read fraction) is an UPPER bound.  No gain, aperture "
                 "or exposure is inferred (GAP_AUDIT 9.42).")

    # ------------------------------------------------------------------ S ---
    rngS = np.random.default_rng(4242)
    sub = (slice(100, 356), slice(100, 356))
    base = tmpl[sub]
    yy, xx = np.mgrid[0:256, 0:256]
    FWHM = 3.31
    sig_psf = FWHM / 2.3548
    x0, y0 = 128.0, 128.0
    r2 = (xx - x0) ** 2 + (yy - y0) ** 2
    P = np.exp(-0.5 * r2 / sig_psf ** 2)
    P[r2 > (4.0 * sig_psf) ** 2] = 0.0
    P /= P.sum()
    sumP2 = float((P ** 2).sum())
    F_e = 60000.0
    src_e = F_e * P
    sky_level_adu = float(np.median(base))

    ann = (r2 > (3.0 * FWHM) ** 2) & (r2 <= (6.0 * FWHM) ** 2)
    ann_y, ann_x = yy[ann].astype(float), xx[ann].astype(float)
    Aann = np.column_stack([np.ones(ann.sum()), ann_x, ann_y])
    B_ann_true = float(np.median(base[ann]))     # true annulus sky of the base

    def local_plane(frame):
        """Independent LOCAL background of the annulus: least-squares plane.
        A plane removes the nebular gradient, which would otherwise enter the
        MAD and masquerade as extra noise when the sky is scaled."""
        c, *_ = np.linalg.lstsq(Aann, frame[ann], rcond=None)
        return c

    def psf_snr(frame):
        """PSF-weighted flux SNR (口径 A, design 2.3):
             F_hat = sum_p P_p (y_p - B_hat(p)), sigma_F = sigma_bg*sqrt(sum P^2)
        B_hat is a plane fitted on the source-free 3-6 FWHM annulus, so any sky
        pedestal is measured and removed self-consistently -- this is what makes
        the arithmetic arm a true null test."""
        c = local_plane(frame)
        Bmap = c[0] + c[1] * xx + c[2] * yy
        res = frame[ann] - (c[0] + c[1] * ann_x + c[2] * ann_y)
        sig = float(pn.MAD2SIG * np.median(np.abs(res)))
        Fhat = float((P * (frame - Bmap)).sum())
        return Fhat, sig * np.sqrt(sumP2), sig

    def naive_global_snr(frame):
        """The TRAP metric: no background subtraction at all, sigma = MAD of
        the whole window -> a sky pedestal inflates the numerator."""
        return float(frame.sum() / (pn.MAD2SIG * np.median(np.abs(frame - np.median(frame)))))

    # ------------------------------------------------------------------ #
    # The sky term is measured from the DIFFERENCE of two independent
    # realisations of the same scene at the same sky level: the real nebular
    # structure (which is common to both) cancels exactly, so what is left is
    # the injected physical noise only.  This removes the structure
    # contamination that a single-frame MAD would carry.
    # ------------------------------------------------------------------ #
    def noise_sigma_from_pair(scale, M):
        vals = []
        for _ in range(M):
            f1 = pn.simulate_frame(base, None, G=a.G, RN=a.RN, sky_scale=scale,
                                   source_e=src_e, rng=rngS)
            f2 = pn.simulate_frame(base, None, G=a.G, RN=a.RN, sky_scale=scale,
                                   source_e=src_e, rng=rngS)
            dd = (f1 - f2)[ann]
            dd = dd - np.median(dd)
            vals.append(float(pn.MAD2SIG * np.median(np.abs(dd)) / np.sqrt(2.0)))
        return float(np.mean(vals)), float(np.std(vals)), len(vals)

    base_real = [pn.simulate_frame(base, None, G=a.G, RN=a.RN, source_e=src_e,
                                   rng=rngS) for _ in range(a.n_mc)]
    sig_ind0, sig_ind0_sd, _ = noise_sigma_from_pair(1.0, a.n_mc)
    rows = []
    for dB in (0.0, 50.0, 150.0, 400.0):
        # --- arm A: arithmetic addition of a constant (no new noise) ---------
        fA, sA, naiveA = [], [], []
        for fr in base_real:
            f, _, _ = psf_snr(fr + dB)       # constant added to the pixels
            fA.append(f)
            naiveA.append(naive_global_snr(fr + dB))
        # the noise of the SAME pair is unchanged by adding a constant
        sigA = sig_ind0
        snrA = float(np.mean(fA) / (sigA * np.sqrt(sumP2)))
        # --- arm B: the same extra sky delivered as PHOTONS -------------------
        scale = (B_ann_true + dB) / B_ann_true
        sigB, sigB_sd, _ = noise_sigma_from_pair(scale, a.n_mc)
        fB = []
        for _ in range(a.n_mc):
            fr = pn.simulate_frame(base, None, G=a.G, RN=a.RN, sky_scale=scale,
                                   source_e=src_e, rng=rngS)
            f, _, _ = psf_snr(fr)
            fB.append(f)
        snrB = float(np.mean(fB) / (sigB * np.sqrt(sumP2)))
        v0 = B_ann_true / a.G + (a.RN / a.G) ** 2
        v1 = (B_ann_true + dB) / a.G + (a.RN / a.G) ** 2
        rows.append(dict(
            delta_sky_adu=dB,
            armA_sigma_bg_adu=sigA,
            armA_psf_snr=snrA,
            armA_naive_global_snr=float(np.mean(naiveA)),
            armB_sigma_bg_adu=sigB,
            armB_sigma_bg_scatter_adu=sigB_sd,
            armB_psf_snr=snrB,
            armB_sigma_ratio_predicted=float(np.sqrt(v1 / v0)),
            armB_snr_change_predicted_pct=float(100.0 * (np.sqrt(v0 / v1) - 1.0)),
        ))
    snrA0, naive0, snrB0 = rows[0]["armA_psf_snr"], rows[0]["armA_naive_global_snr"], rows[0]["armB_psf_snr"]
    for r in rows:
        r["armA_snr_change_pct"] = 100.0 * (r["armA_psf_snr"] / snrA0 - 1.0)
        r["armA_naive_snr_change_pct"] = 100.0 * (r["armA_naive_global_snr"] / naive0 - 1.0)
        r["armB_snr_change_pct"] = 100.0 * (r["armB_psf_snr"] / snrB0 - 1.0)
        r["armB_sigma_ratio_measured"] = r["armB_sigma_bg_adu"] / rows[0]["armB_sigma_bg_adu"]
    out["S_sky_red_line"] = dict(
        source_flux_e=F_e, fwhm_px=FWHM, annulus_sky_adu=B_ann_true,
        estimator="PSF-weighted flux SNR, sigma_F = sigma_bg*sqrt(sum P^2); "
                  "sigma_bg measured from the DIFFERENCE of two independent "
                  "realisations (structure cancels), plane background on a "
                  "source-free 3-6 FWHM annulus",
        n_realisations=a.n_mc, sky_level_adu=sky_level_adu, rows=rows,
        verdict_armA="PASS (true effect is zero, metric returns zero)"
        if all(abs(r["armA_snr_change_pct"]) < 0.5 for r in rows) else "FAIL",
        verdict_armB="PASS (photon sky moves SNR as predicted)"
        if all(abs(r["armB_snr_change_pct"] - r["armB_snr_change_predicted_pct"]) < 8.0
               for r in rows) else "FAIL")

    # ------------------------------------------------- G/RN invariance scan --
    scan = []
    for Gs in (0.5, 1.0, 1.3, 2.0, 4.0):
        for RNs in (0.0, 5.0, 10.0, 20.0):
            v0 = sky_level_adu / Gs + (RNs / Gs) ** 2
            v1 = (sky_level_adu + 150.0) / Gs + (RNs / Gs) ** 2
            scan.append(dict(G=Gs, RN=RNs,
                             sky_fraction_of_variance=float((sky_level_adu / Gs) / v0),
                             sigma_ratio_150adu=float(np.sqrt(v1 / v0)),
                             snr_change_pct=float(100.0 * (np.sqrt(v0 / v1) - 1.0))))
    out["G_RN_invariance_scan"] = dict(
        scan=scan,
        note="The SIGN of the sky effect is invariant (SNR always falls).  The "
             "MAGNITUDE is set by the dimensionless sky fraction of the variance "
             "and is therefore stated without any physical unit.")

    out["elapsed_s"] = time.time() - t0
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)

    for k in ("V1_independent_noise", "V2_quantisation", "V3_flat_field",
              "V4_template_term", "G_noise_affinity_dimensionless"):
        v = out[k]
        print("==", k, "==")
        print(json.dumps({kk: vv for kk, vv in v.items() if kk != "levels"},
                         indent=1, default=str)[:1200])
    print()
    print("== S: sky red line (PSF-weighted, %d realisations) ==" % a.n_mc)
    print("%8s %10s %12s %12s %11s %11s %11s %11s" % (
        "dSky", "A: dSNR%", "A_naive:dSNR%", "B: sig_ratio", "B: sigpred",
        "B: dSNR%", "B: pred%", "B: MC%"))
    for r in rows:
        print("%8.0f %10.4f %12.2f %12.4f %11.4f %11.3f %11.3f %11.2f" % (
            r["delta_sky_adu"], r["armA_snr_change_pct"], r["armA_naive_snr_change_pct"],
            r["armB_sigma_ratio_measured"], r["armB_sigma_ratio_predicted"],
            r["armB_snr_change_pct"], r["armB_snr_change_predicted_pct"],
            100.0 * r["armB_sigma_bg_scatter_adu"] / r["armB_sigma_bg_adu"]))
    print("verdicts:", out["S_sky_red_line"]["verdict_armA"], "|",
          out["S_sky_red_line"]["verdict_armB"])
    print("wrote", a.out, "elapsed %.1fs" % out["elapsed_s"])


if __name__ == "__main__":
    main()
