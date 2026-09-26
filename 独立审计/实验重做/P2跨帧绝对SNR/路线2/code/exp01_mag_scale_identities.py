#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P2-R2-01: P-CST-01 / P-CST-02 / P-CST-19 (mag-scale identities & m_ref anchor)

A literature leg  : Pogson 1856 (MNRAS 17, 12) definition m = -2.5 log10(F/F0).
B experiment leg  : (i) algebraic identity & round-trip of the mag<->flux map,
                    negative control: zero effect (dF=0) => dM=0 exactly;
                    (ii) Pogson step: flux ratio 100 => exactly 5 mag;
                    (iii) m_ref sweep m_ref in {5,6,7,8} through the P2 forward
                    model: frame_snr = F_ref/(sigma_bg*sqrt(sum P^2)) and
                    per-pixel weight w = SNR_src^2/F_ref^2.
C seed             : SEED = 20260926 (fixed).
D outputs          : results/exp01_mag_scale_identities.json
Pure python + numpy; imports nothing from the repository.
"""
import json
import numpy as np

SEED = 20260926
K_MAG = 2.5                 # P-CST-01
INV = -0.4                  # P-CST-02
OUT = "results/exp01_mag_scale_identities.json"

def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "numpy_version": np.__version__}

    # ---- (i) round-trip identity -------------------------------------------
    F = 10.0 ** rng.uniform(-3, 6, size=200_000)
    ZP = 25.0
    m = ZP - K_MAG * np.log10(F)          # m = ZP - 2.5 log10(F)
    F_rt = 10.0 ** (INV * (m - ZP))       # F = 10^(-0.4 (m - ZP))
    m2 = ZP - K_MAG * np.log10(F_rt)
    res["roundtrip_max_abs_dm"] = float(np.max(np.abs(m2 - m)))
    # negative control: zero effect => metric exactly zero
    res["zero_effect_dm"] = float(np.max(np.abs(-K_MAG * np.log10(F / F))))

    # ---- (ii) Pogson step ---------------------------------------------------
    res["pogson_100x_delta_mag"] = float(-K_MAG * np.log10(0.01))
    res["pogson_1mag_flux_ratio"] = float(10 ** 0.4)

    # ---- (iii) m_ref sweep through the P2 forward model ---------------------
    # frame: sigma_bg fixed; PSF profile P normalised in a window; reference
    # star flux F_ref = 10^(-0.4 (m_ref - ZP)); a *source* of fixed flux F_src
    # sits on the frame (its SNR must NOT depend on m_ref).
    sigma_bg = 5.0
    ZP_k = 25.0
    # Gaussian PSF, FWHM = 3 px, window half-width 30 px, pixel-centred grid
    fwhm = 3.0
    sig_p = fwhm / (2.0 * np.sqrt(2.0 * np.log(2.0)))
    h = np.arange(-30, 31)
    xx, yy = np.meshgrid(h, h)
    rr2 = xx ** 2 + yy ** 2
    P = np.exp(-rr2 / (2.0 * sig_p ** 2))
    sumP2 = float(np.sum(P ** 2))         # profile treated as normalised: sum P ~ 1
    F_src = 10.0 ** (-0.4 * (19.0 - ZP_k))   # fixed 19 mag source

    rows = []
    prev_snr = prev_w = None
    for m_ref in (5.0, 6.0, 7.0, 8.0):
        F_ref = 10.0 ** (INV * (m_ref - ZP_k))
        sigma_F_ref = sigma_bg * np.sqrt(sumP2)      # background-limited
        frame_snr = F_ref / sigma_F_ref              # P-ALG-06 definition
        SNR_src = F_src / sigma_F_ref                # source SNR (m_ref-independent)
        # weight semantics A: control-point numerator = F_ref => exact identity
        w_cp = (F_ref / sigma_F_ref) ** 2 / F_ref ** 2
        w_identity = 1.0 / sigma_F_ref ** 2
        # weight semantics B: per-source numerator = F_src (NOT inverse-variance)
        w_src = SNR_src ** 2 / F_ref ** 2
        row = {"m_ref": m_ref, "F_ref_adu": float(F_ref),
               "frame_snr": float(frame_snr), "SNR_src": float(SNR_src),
               "w": float(w_src), "w_cp_identity_rel": float(abs(w_cp - w_identity) / w_identity),
               "w_src_over_invvar": float(w_src * sigma_F_ref ** 2)}
        if prev_snr is not None:
            row["frame_snr_ratio_vs_prev"] = float(frame_snr / prev_snr)
            row["w_ratio_vs_prev"] = float(w_src / prev_w)
        rows.append(row)
        prev_snr, prev_w = frame_snr, w_src
    res["m_ref_sweep"] = rows
    res["expected_frame_snr_ratio_per_mag"] = float(10 ** -0.4)   # 0.39811
    res["expected_w_ratio_per_mag"] = float(10 ** 0.8)            # 6.30957
    res["sumP2_window30_fwhm3"] = sumP2
    print(json.dumps(res, indent=2))
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)

if __name__ == "__main__":
    main()
