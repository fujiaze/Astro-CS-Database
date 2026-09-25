#!/usr/bin/env python3
"""AUD202-V / V1: 权重在信号维退化的独立复算（不看成稿，从代码定义自建）

代码事实（自己顺出来的来源面）：
  臂 A  逐像素 ivar:  Phase1 挂 "variance" 帧内块 = snr_noise_model_v1_fill 的
        var(x,y)=a+b*x+c*y（控制点 = 星点掩膜外的 blank-sky 稳健方差）
        -> drizzle sumVarNum += v*w^2 -> 产品 variance/ivar
        -> Phase2 module_adapters.cpp:11936  w = ivar_v[d][p]（逐样本）
        按构造 S_src 项为 0（patch 在掩膜外；平面是自变量 (x,y) 的线性函数）。
  臂 A' 逐像素 corrected variance: w = 1/Var(corrected)，Var 传播的逐像素噪声项
        同样来自臂 A 的那张面（无源项）。
  臂 B  帧级 SNR 链: w_k = (frame_snr*intra)^2/F_ref,k^2, intra 恒 1
        (module_adapters.cpp:11534 in.sparse = nullptr)
        frame_snr = F_ref/sigma_F(F_ref) —— 参考轮廓 + 固定 F_ref 的一个数。

最优权重（本仓权威口径 ASTROCS_DESIGN §2.2 / CONTROL_WEIGHT_SNR §2a /
NOISE_MODEL §5c）：sigma_w^2(x,y) = sigma_bg^2(x,y) + S_src(x,y)/g
=> 过权倍数 R(x,y) = w_used/w_opt = 1 + S_src/(g*sigma_bg^2)
   以"该像素源信号对背景 rms 的比" u = S_src/sigma_bg 表达：
   R = 1 + u^2*(sigma_bg/g)/sigma_bg^2 ... 化简：S_src/(g sigma_bg^2) = u*(sigma_bg/g)
   记 N_sky_rms_e = g*sigma_bg（天空噪声按电子计），则 R = 1 + u/N_sky_rms_e。
"""
import math
import sys

# 本仓冻结常数（snr_science.cpp:50,54）
K_GAUSS_FWHM = 2.3548200450309493
K_MOFFAT4_FWHM = 1.230310


def moffat4_peak_fraction(sigma_px, half_px=None):
    """离散归一化 Moffat4 beta=4 的峰值像素占比 P_0 = 1/sum(v)。
    与 snr_science.cpp::moffat4Discrete/autoHalf 同网格规则（own re-impl）。"""
    alpha2 = 2.0 * sigma_px * sigma_px
    if half_px is None:
        fwhm_eff = sigma_px * K_MOFFAT4_FWHM
        h = max(30, min(256, int(math.ceil(12.0 * fwhm_eff))))
        half_px = h
    s = 0.0
    for j in range(-half_px, half_px + 1):
        for i in range(-half_px, half_px + 1):
            t = 1.0 + (i * i + j * j) / alpha2
            s += t ** -4
    return 1.0 / s  # 中心像素 v=1 -> P_0 = 1/sum


def over_weight_factor(S_src_adu, sigma_bg_adu, gain_e_per_adu):
    return 1.0 + S_src_adu / (gain_e_per_adu * sigma_bg_adu ** 2)


def coadd_efficiency(sigmas, weights):
    """E = Var_w/Var_opt - 1；Var_w = (sum w)^-2 * sum w_i^2 sigma_i^2。"""
    n = len(sigmas)
    num = sum(w * w * s * s for w, s in zip(weights, sigmas))
    den = sum(weights) ** 2
    var_w = num / den
    var_opt = 1.0 / sum(1.0 / (s * s) for s in sigmas)
    return var_w / var_opt - 1.0


def main():
    out = []
    w = out.append
    w("=" * 78)
    w("V1-1 解析式核对：过权倍数 R = 1 + S_src/(g*sigma_bg^2)")
    w("  等价形式 R = 1 + u/N_sky_rms_e, u = S_src/sigma_bg（该像素源信号的")
    w("  背景 rms 倍数），N_sky_rms_e = g*sigma_bg。推导：")
    w("  Var_src(ADU^2) = S_src/g  (本仓 §5c / Howell CCD 方程约定)")
    w("  w_used = 1/sigma_bg^2 ; w_opt = 1/(sigma_bg^2 + S_src/g)")
    w("  R = w_used/w_opt = (sigma_bg^2 + S_src/g)/sigma_bg^2 = 1 + S_src/(g*sigma_bg^2)")
    w("")

    # ---- 表 1：异源输入下的过权倍数（峰值像素），三档 sky rms x 两档 gain ----
    w("V1-2 表：亮源峰值像素的过权倍数 R（P_0 由本仓 Moffat4 离散轮廓算出）")
    sigma_bg_list = [1.0, 5.0, 20.0]         # ADU 逐像素背景 rms
    gain_list = [0.5, 1.3, 2.0]              # e-/ADU
    fwhm_gauss = 3.5                         # 检测块高斯 FWHM (px)
    sigma_px = fwhm_gauss / K_GAUSS_FWHM
    p0 = moffat4_peak_fraction(sigma_px)
    w("  PSF: 检测块 FWHM=%.2f px -> Moffat4 sigma=%.6f px -> 峰值像素占比 P_0=%.6f"
      % (fwhm_gauss, sigma_px, p0))
    hdr = "  F_src[ADU]   " + "".join(
        ["  sg=%-5.1f g=%-4.1f |" % (sg, g) for sg in [5.0] for g in gain_list])
    w("")
    w("  --- 固定 sigma_bg=5 ADU，扫源通量 F 与增益 g ---")
    w("  F_src[ADU]      g=0.5        g=1.3        g=2.0     (R, sigma_bg=5 ADU)")
    for F in [10.0, 100.0, 1e3, 1e4, 1e5, 1e6]:
        S = F * p0
        row = "  %-12.3g " % F
        for g in gain_list:
            row += " %10.4g" % over_weight_factor(S, 5.0, g)
        w(row)
    w("")
    w("  --- 固定 g=1.3 e-/ADU，扫背景 rms ---")
    w("  F_src[ADU]    sg=1 ADU    sg=5 ADU   sg=20 ADU")
    for F in [10.0, 100.0, 1e3, 1e4, 1e5, 1e6]:
        S = F * p0
        row = "  %-12.3g " % F
        for sg in sigma_bg_list:
            row += " %10.4g" % over_weight_factor(S, sg, 1.3)
        w(row)
    w("")

    # ---- 表 2：写成 u = 峰值像素 SNR 的形式（与增益/背景标度解耦）----
    w("V1-3 表：R = 1 + u/N_sky_rms，u=峰值像素源信号/sigma_bg")
    w("  u(=S/sigma_bg)   N_sky=1 e-   N_sky=6.5 e-   N_sky=40 e-")
    for u in [1, 10, 100, 1000, 10000]:
        w("  %-16g %10.3f %12.3f %12.3f" % (u, 1 + u / 1.0, 1 + u / 6.5, 1 + u / 40.0))
    w("")

    # ---- 表 3：过权对叠加功率的实际损失（两帧/三帧，一帧带亮源）----
    w("V1-4 权重误用导致的叠加方差效率损失 E = Var_w/Var_opt - 1")
    w("  场景：同一输出像素有 n 帧样本；其中 1 帧该像素被源占据（S_src），")
    w("  其余帧该像素为背景（S=0）；sigma_bg=5 ADU, g=1.3。")
    sg, g = 5.0, 1.3
    p0 = moffat4_peak_fraction(3.5 / K_GAUSS_FWHM)
    w("  F_src[ADU]   n=2      n=4      n=8    (E)")
    for F in [100.0, 1e3, 1e4, 1e5, 1e6]:
        S = F * p0
        true_sig = [math.sqrt(sg * sg + S / g)] + [sg] * 7
        rows = []
        for n in (2, 4, 8):
            sig = true_sig[:n]
            wused = [1.0 / (sg * sg)] * n          # 用到的权（背景逆方差）
            wopt = [1.0 / (s * s) for s in sig]    # 最优权（总方差逆）
            # 全局常数无关：E 用同一比例
            e1 = coadd_efficiency(sig, wused)
            e2 = coadd_efficiency(sig, wopt)
            rows.append(e1)
        w("  %-12.3g %8.4f %8.4f %8.4f" % (F, rows[0], rows[1], rows[2]))
    w("")

    # ---- 表 4：方差面（产品 uncertainty）被低估的倍数 ----
    w("V1-5 产品方差面被低估倍数 = Var_true/Var_reported（同权重下 1/W 口径）")
    w("  报告方差 = 1/sum(w_used)；真方差 = sum(w_used^2 sigma_true^2)/sum(w_used)^2")
    for F in [100.0, 1e3, 1e4, 1e5, 1e6]:
        S = F * p0
        for n in (2, 8):
            sig = [math.sqrt(sg * sg + S / g)] + [sg] * (n - 1)
            wused = [1.0 / (sg * sg)] * n
            rep = 1.0 / sum(wused)
            truev = sum(w * w * s * s for w, s in zip(wused, sig)) / sum(wused) ** 2
            w("    F=%-10.3g n=%d  ratio=%10.4f" % (F, n, truev / rep))
    w("")

    # ---- V2 传导式 ----
    w("V2 传导式核对：稀疏层若存逐源 SNR_i=F_i/sigma_F,i，而合同冻结为")
    w("  SNR_c = F_ref/sigma_F,c。消费侧 weight_chain.cpp:739 actual=frame_snr*intra,")
    w("  :779 w=actual^2/F_ref^2。")
    w("  正确：w_opt = 1/sigma_F,c^2")
    w("  若 intra 为绝对 SNR 且仍乘 frame_snr（现行算子）：")
    w("      w = (frame_snr * F_ref/sigma_F,c)^2/F_ref^2 = frame_snr^2/sigma_F,c^2")
    w("      => 权重被整体乘 frame_snr^2（帧级 SNR 平方）")
    w("  若 intra 为逐源 SNR（F_i/sigma_F,i）并按 §8c 直接换算 w=intra^2/F_ref^2：")
    w("      w = (F_i/F_ref)^2 / sigma_F,i^2  => 相对正确权重乘 (F_i/F_ref)^2")
    w("")
    w("  以本仓 fixed_magnitude 口径 F_ref,k = 10^(-0.4*(m_ref-ZP_k))，m_ref=6.0：")
    for zp in [18.0, 20.0, 22.0]:
        fref = 10 ** (-0.4 * (6.0 - zp))
        w("    ZP=%4.1f -> F_ref=%12.4g ADU" % (zp, fref))
        for Fi in [1e2, 1e3, 1e4, 1e5]:
            w("       F_i=%-9.3g  乘性因子 (F_i/F_ref)^2 = %12.4g"
              % (Fi, (Fi / fref) ** 2))
    w("")
    w("  若再叠加现行 frame_snr*intra 组合，倍率 = frame_snr^2*(F_i/F_ref)^2：")
    for fs in [21.364, 27.3646, 47.3882]:      # 实测 49 帧 ASTROCS_FRAME_SNR
        for Fi in [1e3, 1e4, 1e5]:
            fref = 10 ** (-0.4 * (6.0 - 20.0))
            w("    frame_snr=%-8.3f F_i=%-9.3g -> 倍率 %12.4g" % (fs, Fi, fs ** 2 * (Fi / fref) ** 2))

    txt = "\n".join(out)
    print(txt)


if __name__ == "__main__":
    main()
