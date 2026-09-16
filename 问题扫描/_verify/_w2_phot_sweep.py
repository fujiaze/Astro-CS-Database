"""W2 part 3: sweep of true Moffat4 sigma for the star_detector 5x5 window ->
snr_science FWHM-convention factor and the 5x5 aperture (missing aperture
correction) fraction. All numbers reproducible from this file alone."""
import math
KF, KG = 1.230310, 2.3548200450309493
SKY, A = 5.0, 1000.0


def half_of(f):
    return min(max(int(math.ceil(12.0 * f)), 30), 256)


def sum_p2(sig, half):
    a2 = 2 * sig * sig
    s = s2 = 0.0
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            v = 1.0 / (1.0 + (i * i + j * j) / a2) ** 4
            s += v; s2 += v * v
    return s2 / (s * s)


print(f"{'sig':>5} {'FWHM_true':>9} {'fwhm_rep':>9} {'ratio_sig':>9} "
      f"{'sf_fac':>7} {'m5_shift':>8} {'m00/F':>7} {'apcorr_mag':>9}")
for SIG in (0.8, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0):
    F = 2 * math.pi * A * SIG * SIG / 3.0
    m00 = m10 = m01 = m20 = m02 = 0.0
    for iy in range(-2, 3):
        for ix in range(-2, 3):
            v = A / (1.0 + (ix * ix + iy * iy) / (2 * SIG * SIG)) ** 4
            if v <= 0:
                continue
            x = float(ix); y = float(iy)
            m00 += v; m10 += v * x; m01 += v * y
            m20 += v * x * x; m02 += v * y * y
    xc, yc = m10 / m00, m01 / m00
    sgx = math.sqrt(max(m20 / m00 - xc * xc, 1e-12))
    sgy = math.sqrt(max(m02 / m00 - yc * yc, 1e-12))
    fwhm_rep = KG * 0.5 * (sgx + sgy)
    sf_ok = SKY / math.sqrt(sum_p2(SIG, half_of(KF * SIG)))
    sf_bad = SKY / math.sqrt(sum_p2(fwhm_rep / KF, half_of(fwhm_rep)))
    print(f"{SIG:5.1f} {KF*SIG:9.3f} {fwhm_rep:9.3f} {fwhm_rep/KF/SIG:9.4f} "
          f"{sf_bad/sf_ok:7.4f} {-2.5*math.log10(sf_bad/sf_ok):+8.4f} "
          f"{100*m00/F:6.2f}% {-2.5*math.log10(m00/F):+9.4f}")
print()
print("note: fwhm_rep/KF/SIG is the sigma inflation actually realised by the")
print("5x5 truncation (it partially cancels the 2.3548-vs-1.230310 mismatch for")
print("narrow PSFs and grossly underestimates it is wrong for wide PSFs; the")
print("analytic no-truncation factor is sqrt(3)*2.3548/1.230310 =",
      round(math.sqrt(3) * KG / KF, 4), ")")
print()
print("rectification (v<=0 skipped) floor, sigma_sky=5 ADU, 25 px:")
E = SKY / math.sqrt(2 * math.pi)
print(f"  +{25*E:.2f} ADU per source-free 5x5 window; "
      f"= {25*E/(5*SKY/math.sqrt(sum_p2(2.0, half_of(KF*2.0)))):.2f} x sigma_F(sig=2)")
