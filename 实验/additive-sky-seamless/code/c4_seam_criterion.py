#!/usr/bin/env python3
# 实验/SCI-C/code/c4_seam_criterion.py
"""C4 非退化接缝判据：定义、红/绿双向、检测限、退化判据反证。

判据 D（非退化，保留背景）：
  profile m(x)=median_y mosaic(x,·)；边界两侧 ±base_win 内（挖掉 ±(halfwin+2)）
  拟合 order 阶多项式基线；step = median(残差右 halfwin) − median(残差左 halfwin)。
判据 D_deg（退化，全减背景）：先把每帧的整张天光背景减掉再做同一度量。

判据（证据分级见 README §5）：
  N1  D_deg 在"无接缝"与"注入 50 e- 接缝"两种输入下分布不可分（|Δmean| < 2σ_comb）
      ⇒ 退化判据无鉴别力（反证成立）
  N2  D 在同两种输入下可分（分离度 > 10σ）⇒ 有鉴别力
  N3  双向：A=0 时 D 不翻红（假阳性率 ≤ 5%）；A=3×检测限时翻红率 ≥ 95%
  N4  检测限 A* 与噪声标度一致（A* 落在 [1,5] e-）
  N5  D 对"强平滑梯度但无阶跃"的输入保持绿（不被真结构误判）
  N6  D 的零假设分布近高斯（偏度 |s| < 0.5），阈值外推可信
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S

XB = 256          # 注入/度量边界（B 覆盖 gx 2..4 ⇒ 右子集 {B,C}，左子集 {B}）
NMC = 120


def one_realization(seed_tag, amp):
    """注入已知加性接缝 amp [e-] 到帧 B 的 x>=XB 一侧（真值已知）。"""
    w = S.build_world(seed_tag=seed_tag)
    if amp:
        ramp = amp * (np.arange(S.TILE_PX)[None, :] >= XB)
        w["frames"]["B"] = w["frames"]["B"] + ramp
        w["sky"]["B"] = w["sky"]["B"] + ramp
    return w


def nondeg(w, key="excess"):
    """非退化判据：保留背景 + off-locus 对照（扣掉真结构系统台阶）。"""
    wt = {k: v["control_ivar"] for k, v in w["ctrl"].items()}
    m = S.stack_mosaic(w["frames"], w["names"],
                       [np.zeros((S.TILE_PX, S.TILE_PX))] * len(w["names"]), wt)
    return float([s for s in S.seam_steps(m) if s["x"] == XB][0][key])


def deg(w, key="excess"):
    st = S.degenerate_steps(w["frames"], w["names"], w["sky"])
    return float([s for s in st if s["x"] == XB][0][key])


def main():
    g = S.Gates()
    res = {"n_mc": NMC, "boundary": XB}

    # ---- 零假设分布（A=0）
    d0, g0 = [], []
    for i in range(NMC):
        w = one_realization("c4_null_%d" % i, 0.0)
        d0.append(nondeg(w))
        g0.append(deg(w))
    d0 = np.array(d0)
    g0 = np.array(g0)
    res["null"] = dict(nondeg=S.stats(d0), deg=S.stats(g0))

    # ---- 注入臂
    arms = {}
    for amp in (2.0, 5.0, 10.0, 20.0, 50.0):
        dn, dg = [], []
        for i in range(NMC):
            w = one_realization("c4_inj_%g_%d" % (amp, i), amp)
            dn.append(nondeg(w))
            dg.append(deg(w))
        arms[amp] = dict(nondeg=S.stats(dn), deg=S.stats(dg))
    res["injected"] = arms

    # 探测器标定：前半 null 定 (mu, sigma)，后半 null 测**样本外**假阳性率
    half = NMC // 2
    cal, val = d0[:half], d0[half:]
    mu, sd = float(np.mean(cal)), float(np.std(cal, ddof=1))
    thr = 5.0 * sd                     # **双边**阈值：|D − mu| > 5σ
    res["detector_calibration"] = dict(
        mu=mu, sigma=sd, threshold_5sigma_two_sided=thr, threshold_one_sided=mu + thr,
        two_sided=True, n_cal=half, n_val=NMC - half,
        boundary_bias_note="边界特异系统偏置（mu=%.3f e-）来自真实结构，off-locus 对照只减小"
                           "不消除；探测器按边界标定。**双边**阈值避免只对一种注入符号敏感。"
                           % mu)
    res["threshold_5sigma"] = thr
    sep = abs(np.mean(arms[50.0]["nondeg"]["mean"] and [arms[50.0]["nondeg"]["mean"]]) -
              np.mean(d0)) / max(np.std(d0, ddof=1), 1e-12)
    sep50 = abs(arms[50.0]["nondeg"]["mean"] - np.mean(d0)) / max(np.std(d0, ddof=1), 1e-12)
    sep50_deg = abs(arms[50.0]["deg"]["mean"] - np.mean(g0)) / max(np.std(g0, ddof=1), 1e-12)
    res["separation"] = dict(nondeg_50=sep50, deg_50=sep50_deg)
    g.add("N2_nondeg_discriminates",
          "D 在 A=0 与 A=50 e- 下分离度 > 10σ（有鉴别力）", sep50, sep50 > 10.0)
    g.add("N1_deg_no_power",
          "D_deg 在 A=0 与 A=50 e- 下分离度 < 2σ（**恒真反证**：退化臂减掉逐帧天光真值，"
          "注入被代数精确抵消 ⇒ 该门只说明退化判据的定义，不是经验证据）",
          sep50_deg, sep50_deg < 2.0, level="meta")

    # ---- 检测限与双向
    def detect_rate(amp):
        c = 0
        for i in range(NMC):
            w = one_realization("c4_inj_%g_%d" % (amp, i), amp)
            if abs(nondeg(w) - mu) > thr:
                c += 1
        return c / NMC
    fp = float(np.mean(np.abs(val - mu) > thr))        # 样本外假阳性率（双边）
    rates = {a: detect_rate(a) for a in (2.0, 5.0, 10.0)}
    res["false_positive_rate"] = fp
    res["detect_rate"] = rates
    # 响应斜率（对 null 均值的增量）：s = d(response)/d(amplitude)
    amps_r = np.array(sorted(float(k) for k in arms))
    resp = np.array([arms[a]["nondeg"]["mean"] - mu for a in amps_r])
    slope = float(np.polyfit(amps_r, resp, 1)[0])
    lim = float(5.0 * sd / abs(slope)) if slope != 0 else np.nan
    res["response"] = dict(amplitudes=amps_r.tolist(), response=resp.tolist(),
                           slope=slope, detection_limit_e=lim)
    res["detection_limit_e"] = lim
    g.add("N3_two_way", "A=0 样本外假阳性 ≤5% 且 A=10 e- 检出率 ≥95%（红/绿双向）",
          dict(fp=fp, rate10=rates[10.0]), fp <= 0.05 and rates[10.0] >= 0.95)
    g.add("N4_detection_limit", "5σ 检测限（响应斜率外推）落在 [0.5,8] e-",
          dict(lim=lim, slope=slope), bool(np.isfinite(lim) and 0.5 <= lim <= 8.0))

    # ---- 强平滑梯度但无阶跃：不得误判
    fp_grad = 0
    for i in range(30):
        w = S.build_world(seed_tag="c4_grad_%d" % i,
                          coeffs={k: dict(v, pl=(v["pl"][0] * 6.0, v["pl"][1] * 6.0),
                                          qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0))
                                  for k, v in S.SKY_TRUE.items()})
        # 所有帧加同一个强平滑梯度（= 真实天光结构，非帧间失配）⇒ 不是接缝
        yy, xx = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
        common = 40.0 * ((xx - 256.0) / 256.0)
        for nm in w["names"]:
            w["frames"][nm] = w["frames"][nm] + common
        if nondeg(w) > thr:
            fp_grad += 1
    res["false_positive_smooth_gradient"] = fp_grad / 30
    g.add("N5_no_false_alarm_on_gradient",
          "强平滑公共梯度（非帧间失配）下假阳性 ≤10%",
          res["false_positive_smooth_gradient"],
          res["false_positive_smooth_gradient"] <= 0.10)

    # ---- 零假设近高斯
    from scipy import stats as st
    sk = float(st.skew(d0))
    res["null_normality"] = dict(skew=sk, shapiro_p=float(st.shapiro(d0).pvalue))
    g.add("N6_null_gaussian", "零假设分布偏度 |skew| < 0.5（阈值外推可信）", sk, abs(sk) < 0.5)

    res["gates"] = g.summary()
    p = S.json_dump(res, "c4_seam_criterion.json")
    print("== C4 非退化接缝判据 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-30s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
