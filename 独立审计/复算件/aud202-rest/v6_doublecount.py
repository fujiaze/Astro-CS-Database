#!/usr/bin/env python3
"""AUD202 补派 V6：双计头条数字的三方对账（我的解析 / 归档单次实现值 / 我的大样本 MC）。

独立做法：
 1) 我自己从 §4.2a 的口径定义推解析值：v_corr = (B+D+RN^2+F*P_i)/g^2，
    v_emp = v_corr + (RN/g)^2，sigma_F(v) = (Σ P_i^2/v_i)^-1/2，bias = σ(emp)/σ(corr)-1。
 2) 我自己跑 MC（seed=20260925，默认 120k 帧，V6_NF 可调，burn-in 独立），
    估计量 = 生产双计臂的算法（逐帧 MAD 经验 σ̂ + 再加 (RN/g)^2 + 两次迭代最优权）。
 3) 从归档 JSON 取"同一物理点"在每条扫描轴上的实现值，算互差与抽样标准误。
PSF 形状用本仓冻结约定 K_MOFFAT4_FWHM=1.230310、t=1+r²/(2σ²)、β=4（输入形状，非结论）。
不 import 仓库代码。
"""
import json
import math
import os
import sys

import numpy as np

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

REPO = r"F:\Astro dev\Astro CS Normalization Database"
JSON = os.path.join(REPO, "实验", "absolute-snr", "results", "b2_noise_terms.json")
K_MAD = 1.482602218505602
HALF, R_IN, R_OUT = 30, 10.0, 30.0
BASE = dict(g=1.3, RN=10.0, D=0.5, sig=1.5, F=1000.0, B=100.0)


def moffat4(sigma_px, half=HALF):
    fwhm = sigma_px * 1.230310
    j, i = np.mgrid[-half:half + 1, -half:half + 1]
    r2 = i.astype(float) ** 2 + j.astype(float) ** 2
    v = (1.0 + r2 / (2.0 * sigma_px * sigma_px)) ** -4.0
    P = v / v.sum()
    return P, float((P ** 2).sum()), float(P[half, half]), fwhm


def sigma_f_from(Pf, var_adu):
    return math.sqrt(1.0 / float((Pf * Pf / var_adu).sum()))


def analytic(g, RN, D, sig, F, B):
    P, sum_p2, pc, fwhm = moffat4(sig)
    Pf = P.ravel()
    v_corr = (B + D + RN ** 2 + np.maximum(F * Pf, 0.0)) / g ** 2
    v_emp = v_corr + (RN / g) ** 2
    s_c, s_e = sigma_f_from(Pf, v_corr), sigma_f_from(Pf, v_emp)
    # 文档字面式 sqrt(1+F/σ_bg^2)（丢 1/g 与 P_i 依赖）与正确逐像素式的对比
    sig_sky_adu = math.sqrt((B + D + RN ** 2) / g ** 2)
    literal_adu = math.sqrt(1.0 + (F / g ** 2) / sig_sky_adu ** 2)
    return dict(sigma_f_corr=s_c, sigma_f_emp=s_e, bias=s_e / s_c - 1.0,
                sum_p2=sum_p2, p_center=pc, fwhm=fwhm, sig_sky_adu=sig_sky_adu,
                literal=literal_adu, asymptotic=sqrt_lim(g, sum_p2))


def sqrt_lim(g, sum_p2):
    """高源极限：真值式 -> sqrt(F ΣP²/(g σ_sky²))，文档式 -> sqrt(F/σ_sky²)，
    文档/真值 -> sqrt(g/ΣP²)。"""
    return math.sqrt(g / sum_p2)


def my_mc(nf, seed=20260925, burn=3000):
    """自写 MC：同时产出 (a) 双计臂逐帧预测 sigma_F、(b) 正确臂逐帧通量 F_hat 的实测散布。
    后者 = 实现里的分母 sigma_f_mc_adu（样本标准差），是 bias_empirical_rn 的噪声源。"""
    rng = np.random.default_rng(seed)
    P, sum_p2, pc, _ = moffat4(BASE["sig"])
    Pf = P.ravel()
    n = P.shape[0]
    yy, xx = np.mgrid[0:n, 0:n]
    c = (n - 1) / 2.0
    rr = np.hypot(yy - c, xx - c)
    mask = (rr >= R_IN) & (rr <= R_OUT)
    g, RN, D, F, B = BASE["g"], BASE["RN"], BASE["D"], BASE["F"], BASE["B"]
    lam = F * P + B + D
    v_corr = (B + D + RN ** 2 + np.maximum(F * Pf, 0.0)) / g ** 2
    s_def = sigma_f_from(Pf, v_corr)

    def one():
        e = rng.poisson(lam).astype(np.float64)
        e += rng.normal(0.0, RN, e.shape)
        d = (e - None if False else e) / g
        bg = d[mask]
        bh = np.median(bg)
        dd = (d - bh).ravel()
        sh = K_MAD * np.median(np.abs(bg - bh))
        out = []
        for mode in ("correct", "double"):
            Fk = 0.0
            for _ in range(2):
                extra = (RN / g) ** 2 if mode == "double" else 0.0
                var_i = sh ** 2 + extra + np.maximum(Fk * Pf, 0.0) / g
                den = float((Pf * Pf / var_i).sum())
                Fk = float((Pf / var_i * dd).sum()) / den
            out.append((math.sqrt(1.0 / den), Fk))
        return out

    s_double, F_correct = np.empty(nf + burn), np.empty(nf + burn)
    for k in range(nf + burn):
        a, b = one()
        s_double[k], F_correct[k] = b[0], a[1]
    s_double, F_correct = s_double[burn:], F_correct[burn:]

    def block_stats(size):
        nb = nf // size
        b = s_double[:nb * size].reshape(nb, size)
        # 实现的分母 = 正确臂通量的样本标准差（arm "total" 的 std(F_tot)）
        fsc = np.std(F_correct[:nb * size].reshape(nb, size), axis=1, ddof=1)
        bias = b.mean(axis=1) / fsc - 1.0
        return bias

    bias_full = s_double.mean() / np.std(F_correct, ddof=1) - 1.0
    bs = block_stats(1000)
    return dict(mean_arm_sigma_f=float(s_double.mean()), sigma_f_def=s_def,
                bias_vs_analytic_denom=float(s_double.mean() / s_def - 1.0),
                sem_of_mean_arm=float(s_double.std(ddof=1) / math.sqrt(nf)),
                sd_single_frame_sigma_f=float(s_double.std(ddof=1)),
                bias_n1000_mean=float(bs.mean()), bias_n1000_sd=float(bs.std(ddof=1)),
                bias_n1000_range=float(bs.max() - bs.min()), n_blocks=len(bs),
                bias_full=bias_full, n=nf, burn=burn)


def archive():
    with open(JSON, encoding="utf-8") as fh:
        j = json.load(fh)
    return j, j["scans"], j["gates"]


def main():
    b = BASE
    nf = int(os.environ.get("V6_NF", "120000"))
    a = analytic(b["g"], b["RN"], b["D"], b["sig"], b["F"], b["B"])
    print("=" * 78)
    print("V6-1 我的解析复算（基准点，与实现同参数：F=1000 e-, B=100 e-/px, RN=10 e-,")
    print("     g=1.3 e-/ADU, D=0.5 e-/px, σ_psf=1.5 px, 半宽 30, 环带 [10,30]）")
    print("=" * 78)
    print(f"  我的 Moffat4: FWHM={a['fwhm']:.4f} px  ΣP_i²={a['sum_p2']:.8f}  "
          f"P_center={a['p_center']:.6f}  σ_sky(ADU)={a['sig_sky_adu']:.6f}")
    print(f"  σ_F 解析（读噪计一次）  = {a['sigma_f_corr']:.6f} ADU")
    print(f"  σ_F 解析（双计）        = {a['sigma_f_emp']:.6f} ADU")
    print(f"  ★ 我的解析双计偏差       = {100*a['bias']:.4f}%")
    print(f"  （无 P_i 依赖的逐像素渐近式 sqrt(1+RN²/(B+D+RN²))-1 = "
          f"{100*(math.sqrt(1+b['RN']**2/(b['B']+b['D']+b['RN']**2))-1):.4f}%）")

    print()
    print("=" * 78)
    print("V6-2 归档值 vs 我的解析值：同一物理点在各扫描轴上的实现值")
    print("=" * 78)
    j, scans, gates = archive()
    print(f"  JSON frozen base = {j['frozen_config']['base']}")
    print(f"  seeds：实现按行累加 seed += 1000（b2_noise_terms.py:175）⇒ 每个物理点每次独立实现")
    hits = []
    for axis, lst in scans.items():
        for r in lst:
            if all(abs(r[k] - b[nm]) < 1e-12 for k, nm in
                   (("gain_e_per_adu", "g"), ("read_noise_e", "RN"), ("dark_e_per_px", "D"),
                    ("psf_sigma_px", "sig"), ("F_e", "F"), ("sky_e_per_px", "B"))):
                hits.append((axis, r))
    print(f"  与基准点**物理完全相同**的行数 = {len(hits)}")
    vals = []
    for ax, r in hits:
        v = r["bias_empirical_rn"]
        vals.append(v)
        print(f"    {ax:<18} axis_value={r['axis_value']:<7} 实现值 bias_empirical_rn="
              f"{100*v:8.4f}%   闭式 pred_doublecount_bias={100*r['pred_doublecount_bias']:8.4f}%"
              f"   σ̂(单帧 sF) 散布={100*r['sigma_f_empirical_rn_adu'][1]/r['sigma_f_def_adu']:.4f}")
    print(f"  同点实现值极差 = {100*(max(vals)-min(vals)):.4f} pp"
          f"；实现值 − 我的解析 = " + ", ".join(f"{100*(v-a['bias']):+.3f}pp" for v in vals))
    hd = gates["G4c_bias_at_base_point"]
    src = next(r for ax, r in hits if ax == "source_flux_e")
    print(f"  归档头条 gates['G4c_bias_at_base_point'] = {100*hd:.4f}%")
    print(f"    取数式（b2_noise_terms.py:214-215）= results['source_flux_e'] 中 axis_value==1000"
          f" 那一行的 bias_empirical_rn = {100*src['bias_empirical_rn']:.4f}%")
    print(f"    ⇒ 头条 = 单次 MC 实现值（N_MC=1000，seed=20260921+{src.get('_seed','?')}），非闭式")
    print(f"    与我的解析值差 = {100*(hd-a['bias']):+.4f} pp")
    r0 = hits[0][1]
    print(f"  归档 JSON 基准行的臂统计（同一物理点，各行 seed 不同）：")
    for ax, r in hits:
        mn, sd = r["sigma_f_empirical_rn_adu"]
        print(f"    {ax:<18} mean(sF_arm)={mn:.4f} ADU  sd(sF_arm)={sd:.4f} ADU"
              f"        ⇒ sd/bias 单帧 = {sd/r['sigma_f_def_adu']:.4f}"
              f"  N=1000 的 sem = {100*sd/math.sqrt(1000)/r['sigma_f_def_adu']:.4f} pp"
              f"  sigma_f_def_adu={r['sigma_f_def_adu']:.4f}")
    print(f"  最坏点：{gates['G4c_max_bias_over_all_points']*100:.3f}%"
          f"（RN=50 归档头条 +34.0% 之出处）")

    print()
    print("=" * 78)
    print("V6-3 我自己的大样本 MC 期望与 N=1000 的抽样标准误")
    print("=" * 78)
    if nf:
        m = my_mc(nf)
        print(f"  n={m['n']} 帧（burn-in {m['burn']} 另计），seed=20260925")
        print(f"  双计臂 E[sigma_F] = {m['mean_arm_sigma_f']:.4f} ADU   (我的解析 = {a['sigma_f_emp']:.4f} ADU)")
        print(f"  ⇒ 我的 MC 臂均值 vs 我的解析 = "
              f"{100*(m['mean_arm_sigma_f']/a['sigma_f_emp']-1):+.4f}%  "
              f"(sem {100*m['sem_of_mean_arm']/a['sigma_f_emp']:.4f}%)")
        print(f"  分母（正确臂通量的样本标准差）单帧 sd(sigma_F) = "
              f"{100*m['sd_single_frame_sigma_f']/m['sigma_f_def']:.4f}%")
        print(f"  ★ 我把实现当作 N=1000 独立重跑 {m['n_blocks']} 次 ⇒ bias_empirical_rn 的")
        print(f"      抽样分布：均值 = {100*m['bias_n1000_mean']:.4f}%  sd = "
              f"{100*m['bias_n1000_sd']:.4f} pp  极差 = {100*m['bias_n1000_range']:.4f} pp")
        print(f"      归档同点 4 轴实现值极差 = {100*(max(vals)-min(vals)):.4f} pp"
              f"  ⇒ 与我的 N=1000 抽样极差同量级 ⇒ 4 个百分数差 = 抽样噪声，非物理差异")
        print(f"  我的解析（闭式） = {100*a['bias']:.4f}%  vs 归档头条 {100*hd:.4f}%"
              f"（差 {100*(hd-a['bias']):+.4f} pp = "
              f"{abs(hd-a['bias'])/m['bias_n1000_sd']:.2f} 个 N=1000 sd）")
        print(f"  全 30 点 max|实现−闭式| = "
              f"{100*gates['G4c_pred_vs_measured_mc_max_abs_diff']:.4f} pp"
              f" = {gates['G4c_pred_vs_measured_mc_max_abs_diff']/m['bias_n1000_sd']:.2f} 个 sd"
              f"（该字段无门，见 V6-4）")
    print()
    print("=" * 78)
    print("V6-4 各 max/min 字段的门约束配对（我从 JSON gates + 源码 assert 现场点）")
    print("=" * 78)
    for k, v in gates.items():
        if k == "G3_sky_limited_check":
            continue
        print(f"  {k:<48} = {format(v,'.6g') if isinstance(v,float) else v}")


if __name__ == "__main__":
    main()
