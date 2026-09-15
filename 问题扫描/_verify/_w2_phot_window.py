"""W2 photometric chain, part 2: star_detector 5x5 window -> Moffat4 SNR chain.

Models lib/phase1/stars/star_detector.cpp:138-162 (5x5 window, background
subtracted per pixel, v<=0 pixels SKIPPED, second moments -> fwhm=2.3548*avg(a,b))
and feeds the resulting (flux, fwhm_px) into the frozen
lib/snr_estimator/cpp/src/snr_science.cpp optimal-extraction formula.
"""
import math

KF = 1.230310
KGAUSS = 2.3548200450309493
SIG = 2.0          # true Moffat4 beta=4 sigma [px]
A = 1000.0         # true peak amplitude [ADU]  (total flux = 2*pi*A*SIG^2/3)
SKY = 5.0          # blank-sky rms [ADU]
F_TRUE = 2 * math.pi * A * SIG * SIG / 3.0
print(f"true Moffat4 sigma={SIG} px, FWHM_moffat={KF*SIG:.4f} px, A={A} ADU, "
      f"F_true={F_TRUE:.2f} ADU, sigma_sky={SKY} ADU")


def moff(x, y):
    return A / (1.0 + (x * x + y * y) / (2.0 * SIG * SIG)) ** 4


# --- noiseless 5x5 window (deterministic part) ---
m00 = m10 = m01 = m20 = m02 = m11 = 0.0
excluded_neg = 0
for iy in range(-2, 3):
    for ix in range(-2, 3):
        v = moff(ix, iy)          # background is 0 in this noiseless test
        if v <= 0:
            excluded_neg += 1
            continue
        x = float(ix); y = float(iy)
        m00 += v; m10 += v * x; m01 += v * y
        m20 += v * x * x; m02 += v * y * y; m11 += v * x * y
xc = m10 / m00; yc = m01 / m00
mu20 = m20 / m00 - xc * xc
mu02 = m02 / m00 - yc * yc
sgx, sgy = math.sqrt(max(mu20, 1e-12)), math.sqrt(max(mu02, 1e-12))
fwhm_report = KGAUSS * 0.5 * (sgx + sgy)
print(f"5x5 window: enclosed flux m00={m00:.2f} ADU = {100*m00/F_TRUE:.2f}% of F_true "
      f"(missing aperture correction)")
print(f"  moment sigma (5x5 truncated) = {0.5*(sgx+sgy):.4f} px ; "
      f"full Moffat4 moment sigma = sqrt(3)*SIG = {math.sqrt(3)*SIG:.4f}")
print(f"  fwhm_px emitted by star_detector.cpp:162 = {fwhm_report:.4f}")
print(f"  snr_science divides by {KF} -> sigma_px = {fwhm_report/KF:.4f} "
      f"(true {SIG}) ratio {fwhm_report/KF/SIG:.4f}")


def sum_p2(sigma, half):
    a2 = 2.0 * sigma * sigma
    s = s2 = 0.0
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            v = 1.0 / ((1.0 + (i * i + j * j) / a2) ** 4)
            s += v; s2 += v * v
    return s2 / (s * s)


def half_of(fwhm):
    return min(max(int(math.ceil(12.0 * fwhm)), 30), 256)


for label, fw in (("correct Moffat4 FWHM", KF * SIG), ("chain (star_detector)", fwhm_report)):
    sig = fw / KF
    sp2 = sum_p2(sig, half_of(fw))
    sf = SKY / math.sqrt(sp2)
    print(f"  {label:24s}: fwhm={fw:7.4f} sum_P^2={sp2:.6f} sigma_F={sf:9.4f} ADU "
          f"SNR_F={F_TRUE/sf:8.3f}")

sig_c, sp2_c = SIG, sum_p2(SIG, half_of(KF * SIG))
sc = SKY / math.sqrt(sp2_c)
sig_b, sp2_b = fwhm_report / KF, sum_p2(fwhm_report / KF, half_of(fwhm_report))
sb = SKY / math.sqrt(sp2_b)
print(f"  => sigma_F inflated x{sb/sc:.4f}; F_5=5*sigma_F: {5*sc:.3f} -> {5*sb:.3f} ADU; "
      f"m_5 shift = {-2.5*math.log10(sb/sc):+.4f} mag; SNR_F shrinks x{sb/sc:.4f}")

# --- rectification bias: v<=0 pixels skipped (star_detector.cpp:145) ---
print()
print("rectification bias from 'if (v <= 0) continue;' (star_detector.cpp:145):")
E = SKY / math.sqrt(2 * math.pi)      # E[max(N(0,sigma),0)]
print(f"  E[max(v,0)] per pure-sky pixel = sigma/sqrt(2pi) = {E:.4f} ADU "
      f"(true expectation of a correctly subtracted pixel = 0)")
print(f"  5x5 window (25 px, source-free) -> +{25*E:.2f} ADU spurious flux")
for F_true_small in (50.0, 100.0, 250.0, 1000.0):
    meas = F_true_small * (m00 / F_TRUE) + 25 * E
    print(f"   true total flux {F_true_small:7.1f} ADU -> reported {meas:8.2f} ADU "
          f"(mag bias {-2.5*math.log10(meas/F_true_small):+.3f})")
print(f"  5-sigma point-source limit implied by rectification alone: "
      f"{25*E:.1f} ADU ({25*E/sc:.2f} x sigma_F)")
"""
"""
