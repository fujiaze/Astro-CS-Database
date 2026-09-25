#!/usr/bin/env python3
"""AUD202-V / V1-b: 用自己的解析式复算成稿 AUD202-001 的过权倍数表（盲推后对表）。

我的解析式（v1_weight_degeneracy.py 推得）：
    R = 1 + S_src,peak_e / sigma_sky,e^2 ,  S_src,peak_e = F_e * P_0
成稿表：F=10→1.011, 100→1.106, 1000→2.057, 3e4→32.7, 1e6→1058
实验侧声明常数（实验/absolute-snr/code/b3_domain_map.py:41）：
    GAIN=1.3 e-/ADU, RN=10 e-, DARK=0.5 e-, SKY_E=200 e-, SIG_MED_E=200 e-
    truth sigma = sqrt((sig_e+SKY_E+DARK+RN**2))/GAIN  -> 空背景 sigma_e^2 = 300.5
"""
import math
import sys

K_MOFFAT4_FWHM = 1.230310


def moffat4_peak_fraction(sigma_px, half_px=None):
    alpha2 = 2.0 * sigma_px * sigma_px
    if half_px is None:
        half_px = max(30, int(math.ceil(12.0 * sigma_px * K_MOFFAT4_FWHM)))
    s = 0.0
    for j in range(-half_px, half_px + 1):
        for i in range(-half_px, half_px + 1):
            s += (1.0 + (i * i + j * j) / alpha2) ** -4
    return 1.0 / s


def main():
    sig_e2 = 200.0 + 0.5 + 10.0 ** 2      # SKY_E + DARK + RN^2 = 300.5 e-^2
    draft = [(10.0, 1.011), (100.0, 1.106), (1000.0, 2.057),
             (3e4, 32.7), (1e6, 1058.0)]
    print("成稿表反解 P_0（用我的式子 R = 1 + F*P_0/sig_e2）：")
    implied = []
    for F, R in draft:
        p0 = (R - 1.0) * sig_e2 / F
        implied.append(p0)
        print("  F=%-10.4g R=%-9.4g -> P_0 = %.6f" % (F, R, p0))
    p0_bar = sum(implied) / len(implied)
    print("  一致性：P_0 离散 max/min = %.6f / %.6f = %.4f（>1.02 即说明常数不同源）"
          % (max(implied), min(implied), max(implied) / min(implied)))
    print("  平均隐含 P_0 = %.6f" % p0_bar)
    print()
    print("本仓 Moffat4 离散轮廓 P_0(sigma_px) 对照：")
    for s in [1.0, 1.1, 1.15, 1.2, 1.25, 1.3, 1.4, 1.486313, 1.7, 2.0, 2.5]:
        print("  sigma_px=%-8.5f (Moffat4 FWHM=%-7.4f px, 检测块高斯 FWHM=%-7.4f px)"
              " P_0=%.6f" % (s, s * K_MOFFAT4_FWHM, s * 2.3548200450309493,
                             moffat4_peak_fraction(s)))
    print()
    print("用 P_0=%.6f, sigma_sky,e^2=%.1f 复算成稿表：" % (p0_bar, sig_e2))
    for F, R in draft:
        mine = 1.0 + F * p0_bar / sig_e2
        print("  F=%-10.4g  成稿 R=%-9.4g   我的式子 R=%-9.4g   相对差=%.2e"
              % (F, R, mine, abs(mine - R) / R))


if __name__ == "__main__":
    main()
