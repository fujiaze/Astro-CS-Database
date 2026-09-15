"""W2 photometric chain numeric checks (read-only analysis, no repo mutation).

Replicates frozen formulas from:
  lib/snr_estimator/cpp/src/snr_science.cpp (moffat4Discrete, snr_source_snr_f64)
  lib/phase1/photometry/photometer.cpp:92
  lib/phase1/stars/star_detector.cpp:162
  lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1553-1559
  lib/astro_image_io/src/hips/aio_hips_writer.cpp:569-571
"""
import math

KF = 1.230310            # snr_science.cpp:36  FWHM = 1.230310*sigma (Moffat4 beta=4)
KGAUSS = 2.3548200450309493   # star_detector.cpp:162 second-moment FWHM


def auto_half(fwhm):                      # snr_science.cpp:78-83
    h = int(math.ceil(12.0 * fwhm))
    return min(max(h, 30), 256)


def sigma_f(F, fwhm, sig_sky, gain=0.0, rn=0.0):
    sigma = fwhm / KF
    half = auto_half(sigma * KF)
    if gain > 0.0:
        a2 = 2.0 * sigma * sigma
        rn2 = (rn / gain) ** 2 if rn > 0 else 0.0
        vals, tot = [], 0.0
        for j in range(-half, half + 1):
            for i in range(-half, half + 1):
                v = 1.0 / ((1.0 + (i * i + j * j) / a2) ** 4)
                vals.append(v); tot += v
        acc = 0.0
        for v in vals:
            p = v / tot
            si = F * p
            var = sig_sky * sig_sky + rn2 + (si / gain if si > 0 else 0.0)
            acc += p * p / var
        return 1.0 / math.sqrt(acc)
    a2 = 2.0 * sigma * sigma
    s = s2 = 0.0
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            v = 1.0 / ((1.0 + (i * i + j * j) / a2) ** 4)
            s += v; s2 += v * v
    sp2 = s2 / (s * s)
    return sig_sky / math.sqrt(sp2)


print("A) FWHM convention mismatch star_detector.cpp:162 -> snr_science.cpp:96/128")
sig_true = 2.0
fwhm_rep = KGAUSS * math.sqrt(3.0) * sig_true     # Moffat4 mu2 = 3*sigma^2
sig_used = fwhm_rep / KF
print(f"   sigma_true={sig_true} px  emitted fwhm_px={fwhm_rep:.4f}  "
      f"sigma used={sig_used:.4f}  ratio={sig_used/sig_true:.4f}")
print(f"   sqrt(3)*2.35482/1.23031 = {math.sqrt(3)*KGAUSS/KF:.6f}")
F, SKY = 1000.0, 5.0
for gain, rn in ((0.0, 0.0), (2.0, 10.0)):
    ok = sigma_f(F, KF * sig_true, SKY, gain, rn)
    bad = sigma_f(F, fwhm_rep, SKY, gain, rn)
    print(f"   gain={gain} rn={rn}: sigma_F correct={ok:.4f} chain={bad:.4f} "
          f"x{bad/ok:.4f}  m5 shift={-2.5*math.log10(bad/ok):+.4f} mag")

print()
print("B) read-noise DOUBLE count snr_science.cpp:167")
F, SKY, G, RN = 1000.0, 5.0, 2.0, 10.0
code = sigma_f(F, 2.46062, SKY, G, RN)
corr = sigma_f(F, 2.46062, SKY, G, 0.0)
print(f"   (rn/gain)^2={(RN/G)**2:.2f} ADU^2 vs sigma_sky^2={SKY*SKY:.2f} ADU^2")
print(f"   sigma_F code={code:.4f} correct={corr:.4f} ratio={code/corr:.4f}")
print(f"   SNR_F ratio={corr/code:.4f}  m5 bias={-2.5*math.log10(code/corr):+.4f} mag")
print(f"   aperture path (194-195) has no rn term -> intra-function disagreement x{code/corr:.4f}")

print()
print("C) photometer.cpp:92 missing /gain on Poisson term")
s_adu, n_in, sky = 10000.0, 50, 5.0
e_code = math.sqrt(s_adu + n_in * sky * sky)
for G in (1.0, 2.0, 0.5):
    e_c = math.sqrt(s_adu / G + n_in * sky * sky)
    print(f"   gain={G:4.2f}: code={e_code:.3f} correct={e_c:.3f} "
          f"err x{e_code/e_c:.4f}  snr x{e_c/e_code:.4f}")
print(f"   sky term={n_in*sky*sky:.1f} ADU^2, source term as written={s_adu:.1f}")

print()
print("D) drizzle/HiPS signal unit ADU vs ADU/sr (sumArea in steradians)")
for a in (0.2, 1.0, 1.1, 2.0):
    ap = (a / 206264.80624706) ** 2
    print(f"   {a:4.2f} arcsec/px: A_pix={ap:.4e} sr  1/A_pix={1/ap:.4e}  "
          f"={2.5*math.log10(1/ap):.3f} mag")
for nside in (16384, 65536):
    ac = 4*math.pi/(12*nside*nside)
    print(f"   nside={nside}: A_cell={ac:.4e} sr")

print()
print("E) EXPTIME never divided out: m5 bias if ZP is per ADU/s")
for t in (30.0, 300.0, 1800.0):
    print(f"   EXPTIME={t:7.1f}s -> {-2.5*math.log10(t):+.4f} mag")

print()
print("F) photscal mislabel / per-frame ivar scaling")
for loc in (2.0, 4.0, 6.0, 8.0):
    print(f"   location={loc:4.1f} dex -> photscal={10**(-loc):.3e}; "
          f"BUNIT=ADU mislabel = {2.5*loc:5.1f} mag, flux x{10**loc:.3e}")
for r in (1.5, 2.0, 3.0):
    print(f"   photscal ratio {r}:1 -> ivar weight ratio {1/r**2:.4f} "
          f"({100*(1-1/r**2):.1f}% under-weight)")
