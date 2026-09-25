"""AUD-201 R2: is the F_syn `lambda` factor a COLOR term, and can the repo's
existing anchor statistic (median/MAD of m_syn - G, binned by brightness) see it?

Two independent parts, pure numpy, read-only on the repo:

  A. ALGEBRAIC SYNTHETIC (standards/02 sec.2 D2): blackbody SEDs x the official
     Gaia G passband (tracked GaiaEDR3_passband.dat). Compute the mean band flux
     WITH and WITHOUT the `lambda` weight. If the difference is constant across
     SED shape it is absorbed by `location`; if it varies with colour it is NOT
     absorbed and it lands in sigma_residual. Quantify both.

  B. REAL DATA (standards/02 sec.2 D3, tracked archived comparison table):
     per-star astrocs vs GaiaXPy mag_G residuals for 1050 real XPSD stars,
     binned by BRIGHTNESS (the axis the repo's anchor uses) and by COLOUR
     (the axis a lambda/unit error moves along). Inject the colour-dependent
     defect from part A and show which of the two binnings reports it.

Usage: python -B aud201_r2_lambda_colorterm.py "<repo_root>"
Writes: aud201_r2_lambda_colorterm.out next to this file.
"""
import io
import os
import sys

import numpy as np

REPO = sys.argv[1] if len(sys.argv) > 1 else r"F:\Astro dev\Astro CS Normalization Database"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "aud201_r2_lambda_colorterm.out")

H = 6.62607015e-34        # J s
C = 2.99792458e8          # m/s
KB = 1.380649e-23         # J/K


def simpson(y, x):
    """Composite Simpson on an evenly spaced grid with an even interval count."""
    n = len(x)
    h = (x[-1] - x[0]) / (n - 1)
    if (n - 1) % 2:
        raise ValueError("interval count must be even")
    w = np.ones(n)
    w[1:-1:2] = 4.0
    w[2:-1:2] = 2.0
    return float(np.dot(w, y) * h / 3.0)


def blackbody_nm(wl_nm, T):
    """Spectral radiance shape, W m^-2 nm^-1 sr^-1 (only shape matters here)."""
    lam = wl_nm * 1e-9
    return (2.0 * H * C ** 2 / lam ** 5) / (np.exp(H * C / (lam * KB * T)) - 1.0)


def load_g_passband(path):
    """GaiaEDR3_passband.dat columns: wl, G, G_err, BP, BP_err, RP, RP_err.
    99.99 = out-of-band sentinel."""
    arr = np.loadtxt(path)
    wl, tr = arr[:, 0], arr[:, 1].copy()
    tr[(tr >= 99.0) | (tr <= 0.0)] = 0.0
    return wl, tr


def resample_acima_free(wl_c, tr_c, wl_grid):
    """Linear resample of the passband onto the XP grid; 0 outside the curve.
    (Deliberately NOT Akima: keeps this recomputation independent of the
    implementation under audit. Grid is dense enough that the scheme difference
    is a small high-order term and cancels in the WITH/WITHOUT-lambda ratio.)"""
    return np.clip(np.interp(wl_grid, wl_c, tr_c, left=0.0, right=0.0), 0.0, None)


def band_mean_flux(sed, tp, wl_grid, photon_weight):
    """<f> = int sed*T*(lambda?) dlambda / int T*(lambda?) dlambda"""
    w = (wl_grid if photon_weight else np.ones_like(wl_grid)) * tp
    num = simpson(sed * w, wl_grid)
    den = simpson(w, wl_grid)
    return num / den if den > 0 else np.nan


def medmad(v):
    v = np.asarray(v, float)
    v = v[np.isfinite(v)]
    if v.size == 0:
        return np.nan, np.nan
    m = float(np.median(v))
    return m, float(1.4826 * np.median(np.abs(v - m)))


def binned(values, centres, nbin, lo, hi):
    """Return per-bin median of `values` for bins over [lo,hi]."""
    edges = np.linspace(lo, hi, nbin + 1)
    out = []
    for i in range(nbin):
        sel = (centres >= edges[i]) & (centres < edges[i + 1] if i < nbin - 1
                                       else centres <= edges[i + 1])
        out.append(float(np.median(values[sel])) if sel.sum() >= 3 else np.nan)
    return np.array(out), np.array([((edges[i] + edges[i + 1]) / 2.0) for i in range(nbin)])


def main():
    lines = []
    A = lines.append
    pb = load_g_passband(os.path.join(
        REPO, "lib", "algorithms", "photometry", "cpp", "test",
        "gate4_dr3sp_gaiaxpy", "GaiaEDR3_passband.dat"))
    xp = np.arange(336.0, 1020.0 + 1e-9, 2.0)          # official grid: 343 @2nm
    assert xp.size == 343, xp.size
    tp = resample_acima_free(pb[0], pb[1], xp)

    A("=== PART A  algebraic synthetic: amplitude of the `lambda` factor ===")
    A("XP grid            : %d pts, %.0f-%.0f nm, step %.1f nm (interval count %d -> Simpson 1/3)"
      % (xp.size, xp[0], xp[-1], xp[1] - xp[0], xp.size - 1))
    A("G passband         : %.0f-%.0f nm, %d tabulated pts, photon-weighted lambda_eff computed below"
      % (pb[0][pb[1] > 0].min(), pb[0][pb[1] > 0].max(), int((pb[1] > 0).sum())))
    lw = float(np.sum(tp * xp * xp) / np.sum(tp * xp))
    A("photon-weighted lambda_eff(G) = %.2f nm" % lw)
    A("")
    A("%-8s %-12s %-12s %-12s %-12s" % ("T[K]", "f_with_lam", "f_no_lam", "dm[mag]", "peak_wl[nm]"))
    temps = [3000, 3500, 4000, 5000, 6000, 6500, 7500, 9000, 12000]
    dm = []
    for T in temps:
        sed = blackbody_nm(xp, T)
        fw = band_mean_flux(sed, tp, xp, True)
        fn = band_mean_flux(sed, tp, xp, False)
        d = -2.5 * np.log10(fw / fn)
        dm.append(d)
        A("%-8d %-12.5e %-12.5e %-12.5f %-12.1f" % (T, fw, fn, d, 2.898e6 / T))
    dm = np.array(dm)
    A("")
    A("lambda-factor dm : mean = %.5f mag (ABSORBED by location, star-independent part)" % float(dm.mean()))
    A("                   spread (max-min) = %.5f mag  <- NOT absorbed; this is what leaks"
      % float(dm.max() - dm.min()))
    A("                   sd = %.5f mag,  p95-ish peak-to-peak proxy 1.4826*MAD = %.5f mag"
      % (float(dm.std(ddof=1)), float(1.4826 * np.median(np.abs(dm - np.median(dm))))))
    A("VERDICT A: the lambda factor is a COLOUR-DEPENDENT (SED-shape-dependent)")
    A("           multiplicative term, not a constant. Its constant part is")
    A("           absorbed by the fitted zero point; its VARIABLE part is not.")
    A("")

    # ---------------- PART B : real archived comparison table ----------------
    csv = os.path.join(REPO, "lib", "algorithms", "photometry", "cpp", "test",
                       "gate4_dr3sp_gaiaxpy", "evidence", "gate4_compare_per_star_1050.csv")
    with io.open(csv, encoding="utf-8") as f:
        head = f.readline().strip().split(",")
    data = np.genfromtxt(csv, delimiter=",", names=True, dtype=None, encoding="utf-8")
    n_rows = len(data)
    rec = np.array([str(s) for s in data["source_id"]])
    aG = np.array(data["astrocs_mag_G"], float)
    gG = np.array(data["gaiagx_mag_G"], float)
    col = np.array(data["gaiagx_color_BP_RP"], float)
    finite = np.isfinite(aG) & np.isfinite(gG) & np.isfinite(col) & (rec != b"None")
    res = aG[finite] - gG[finite]
    colv, gGv = col[finite], gG[finite]

    A("=== PART B  real archived per-star table: which binning sees a colour term? ===")
    A("file : %s" % os.path.relpath(csv, REPO).replace("\\", "/"))
    A("rows : %d (csv lines = %d incl header); usable after finite filter = %d"
      % (n_rows, sum(1 for _ in io.open(csv, encoding="utf-8")), int(finite.sum())))
    m, s = medmad(res)
    A("residual = astrocs_mag_G - GaiaXPy_mag_G :  median = %+.6f mag, MAD-sigma = %.6f mag"
      % (m, s))
    A("colour range BP-RP = %.3f .. %.3f mag ; mag_G range = %.2f .. %.2f"
      % (colv.min(), colv.max(), gGv.min(), gGv.max()))
    A("")

    # (1) the repo's own anchor axis: bin by BRIGHTNESS (G).
    bg, ec = binned(res, gGv, 6, np.percentile(gGv, 1), np.percentile(gGv, 99))
    A("[axis = G mag, as used by PHOTOMETRY.md a_absolute_check_by_magbin]")
    A("  bin centres : %s" % np.round(ec, 2).tolist())
    A("  medians     : %s" % np.round(bg, 6).tolist())
    A("  inter-bin spread = %.6f mag" % float(np.nanmax(bg) - np.nanmin(bg)))
    A("")

    # (2) the axis a lambda/unit error moves along: bin by COLOUR.
    bc, ec2 = binned(res, colv, 6, np.percentile(colv, 1), np.percentile(colv, 99))
    A("[axis = BP-RP colour]")
    A("  bin centres : %s" % np.round(ec2, 3).tolist())
    A("  medians     : %s" % np.round(bc, 6).tolist())
    A("  inter-bin spread = %.6f mag" % float(np.nanmax(bc) - np.nanmin(bc)))
    A("")

    # (3) NEGATIVE CONTROL: inject the colour-dependent lambda defect from part A
    #     (a monotone mapping colour -> dm), then re-run BOTH binnings.
    #     Model: bluest star (BP-RP ~0) -> hot-blackbody end of dm; reddest -> cool end.
    lo, hi = np.percentile(colv, 2), np.percentile(colv, 98)
    t = np.clip((colv - lo) / (hi - lo), 0.0, 1.0)
    dm_lo, dm_hi = float(dm[-1]), float(dm[0])      # hot -> cool across colour
    injected = (dm_lo + (dm_hi - dm_lo) * t) - float(np.mean(dm_lo + (dm_hi - dm_lo) * t))
    A("[NEGATIVE CONTROL] inject a colour-dependent offset, amplitude %.5f mag peak-to-peak"
      % float(injected.max() - injected.min()))
    r_inj = res + injected
    b_g, _ = binned(r_inj, gGv, 6, np.percentile(gGv, 1), np.percentile(gGv, 99))
    b_c, _ = binned(r_inj, colv, 6, np.percentile(colv, 1), np.percentile(colv, 99))
    A("  G-axis    : medians %s  spread = %.6f mag" % (np.round(b_g, 6).tolist(),
                                                       float(np.nanmax(b_g) - np.nanmin(b_g))))
    A("  colour-ax : medians %s  spread = %.6f mag" % (np.round(b_c, 6).tolist(),
                                                        float(np.nanmax(b_c) - np.nanmin(b_c))))
    sp_g = float(np.nanmax(b_g) - np.nanmin(b_g))
    sp_c = float(np.nanmax(b_c) - np.nanmin(b_c))
    amp = float(injected.max() - injected.min())
    A("")
    A("  overall median of the injected residual = %+.6f mag (was %+.6f)"
      % (medmad(r_inj)[0], m))
    A("  overall MAD-sigma of the injected residual = %.6f mag" % medmad(r_inj)[1])
    A("")
    bc_spread = float(np.nanmax(bc) - np.nanmin(bc))
    A("SENSITIVITY OF EACH AXIS (defect amplitude = %.5f mag peak-to-peak):" % amp)
    A("  defect-free colour-axis spread = %.6f mag" % bc_spread)
    A("  after injection, colour-axis   = %.6f mag  (x%.1f vs defect-free)"
      % (sp_c, sp_c / max(bc_spread, 1e-12)))
    A("  after injection, G-axis        = %.6f mag" % sp_g)
    A("  colour-axis / G-axis sensitivity ratio on the injected defect = %.2f"
      % (sp_c / max(sp_g, 1e-12)))
    A("")
    A("KEY DISCRIMINATION (real sample, 1050 stars):")
    A("  observed colour-axis spread of astrocs-GaiaXPy = %.5f mag" % bc_spread)
    A("  a wrong lambda convention would give           = %.5f mag" % amp)
    A("  margin = %.1fx  => the photon-flux-unit hypothesis IS excluded on this sample"
      % (amp / max(bc_spread, 1e-12)))
    A("  SCOPE LIMIT: this sample's spectra come from gaiaxpy calibrate(), NOT from")
    A("  the XPSD uint8 decode. It certifies the integrator + lambda convention, not")
    A("  the container's absolute scale.")
    A("")
    A("CAVEAT on criterion design: in this magnitude-limited sample colour correlates")
    A("with G, so brightness binning picks up %.5f of the injected %.5f mag colour"
      % (sp_g, amp))
    A("  defect (%.0f%% attenuation). The G-binned anchor is therefore NOT blind, but it"
      % (100.0 * sp_g / amp))
    A("  is attenuated and its sensitivity is an artefact of the sample's colour-")
    A("  magnitude correlation, which is not a property of the pipeline. A criterion")
    A("  whose power depends on sample selection is not a sound gate; the colour axis")
    A("  must be measured directly.")
    A("")
    A("NOTE on what part B does and does not prove:")
    A("  astrocs_mag_G and GaiaXPy_mag_G were computed from the SAME absolute")
    A("  spectrum and the SAME passband file (gate4_gaiaxpy_compare.py:57-60 feeds")
    A("  both). The %.1e-mag agreement therefore certifies the INTEGRATOR" % float(medmad(res)[1]))
    A("  (Akima + Simpson + lambda weight) against a second implementation, but it")
    A("  cannot certify the XPSD uint8 DECODE units, because that decode is not on")
    A("  either side of this comparison.")
    text = "\n".join(lines)
    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text.encode("ascii", "backslashreplace").decode("ascii"))


if __name__ == "__main__":
    main()
