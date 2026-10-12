#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""k_photo 帧间一致性：真实帧间差异 vs 拟合误差。

只用 P1 自己的产物与方法（与 P5 的"共同星通量比"不重样）：
  1. 逐帧 k_photo 的内禀不确定度：①产物 sigma_location_se_dex；②解析 σ_resid/√n；
     ③**自助法**（帧内重采样星，重跑 Tukey-IRLS location）→ 标准差；
  2. 帧间离散 vs 误差棒：观测 MAD/0.6745 与 χ²_red = Σ((x−x̄)/σ_i)²/(N−1)，
     并给"超出量" σ_excess = sqrt(σ_obs² − σ_err²)；
  3. 外部相关：观测时间、天光/背景、指向组(M1..M6)、星数/密度/中位通量；
  4. 同指向近邻帧对照 vs 随机帧对。

模型/口径：r = log10(F_instr/F_syn)，location = −log10(k_photo)，scale = 10^(−location)
（docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md §13.1）。

只读 run/PHOT-FIX/**；输出 results/p1fix/kphoto_consistency.json
"""
from __future__ import annotations
import json, math, re, random
from pathlib import Path
from datetime import datetime
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
C_TUKEY = 4.685
NBOOT = 200


def tukey_location(r, iters=50, tol=1e-6):
    loc = float(np.median(r))
    for _ in range(iters):
        S = float(np.median(np.abs(r - loc)) / MAD)
        if S <= 0:
            break
        u = (r - loc) / (C_TUKEY * S)
        w = np.where(np.abs(u) < 1, (1 - u * u) ** 2, 0.0)
        if w.sum() <= 0:
            break
        new = float((w * r).sum() / w.sum())
        if abs(new - loc) < tol:
            loc = new
            break
        loc = new
    return loc


def canon(name):
    return name.replace("cleaned_", "").replace(".fts", "").replace("@", "_")


def parse_time(fk):
    m = re.search(r"-(\d{8})_(\d{6})-", fk)
    if not m:
        return None, None
    d, t = m.group(1), m.group(2)
    dt = datetime(int(d[:4]), int(d[4:6]), int(d[6:8]), int(t[:2]), int(t[2:4]), int(t[4:6]))
    return dt, d


def main():
    rows = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        snr = json.loads((SRC / tag / "p1_snr.json").read_text())
        srcs = json.loads((SRC / tag / "p1_sources.json").read_text())
        smap = {canon(f["file"]): f for f in srcs["frames"]}
        snrmap = {canon(f["file"]): f for f in snr["frames"]}
        for fk in sorted(phot["spatial_gain"]):
            sg = phot["spatial_gain"][fk]; pd = phot["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"])
            used = np.array(s["used"], bool)
            kp = pd.get("k_photo")
            if not (kp and kp > 0):
                continue
            loc = -math.log10(kp)
            rv = r[used]
            sig_resid = float(np.median(np.abs(rv - np.median(rv))) / MAD)
            n = len(rv)
            se_analytic = sig_resid / math.sqrt(n) if n > 0 else float("nan")
            se_prod = None
            if fk in snrmap:
                v = snrmap[fk].get("sigma_location_se_dex")
                se_prod = float(v) if isinstance(v, (int, float)) and v == v else None
            # 自助法（帧内重采样）
            rng = np.random.default_rng(12345)
            boots = np.empty(NBOOT)
            for b in range(NBOOT):
                idx = rng.integers(0, n, n)
                boots[b] = tukey_location(rv[idx])
            se_boot = float(np.std(boots, ddof=1))
            bg = None; nsrc = None; medf = None
            if fk in smap:
                bg = smap[fk].get("background")
                nsrc = smap[fk].get("n_sources")
                fl = [q["flux"] for q in smap[fk]["sources"] if q.get("flux")]
                medf = float(np.median(fl)) if fl else None
            dt, dstr = parse_time(fk)
            mg = re.search(r"M(\d+)_T(\d)", fk)
            rows.append({
                "tag": tag, "frame": fk,
                "k_photo": kp, "log10_k": math.log10(kp), "location": loc,
                "n_used": int(n), "sigma_resid_dex": sig_resid,
                "se_prod": se_prod, "se_analytic": se_analytic, "se_boot": se_boot,
                "background": bg, "n_sources": nsrc, "median_flux": medf,
                "dt": dt.isoformat() if dt else None, "date": dstr,
                "mirror": f"M{mg.group(1)}" if mg else None, "telescope": f"T{mg.group(2)}" if mg else None,
                "sigma_resid_stored": pd.get("sigma_residual_dex"),
                "n_matched": pd.get("n_matched"),
            })
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "kphoto_consistency.json").write_text(json.dumps(rows, indent=1, ensure_ascii=False))

    x = np.array([q["log10_k"] for q in rows])
    N = len(x)
    xbar = x.mean()
    mad_obs = float(np.median(np.abs(x - np.median(x))) / MAD)
    sd_obs = float(np.std(x, ddof=1))
    print(f"N frames = {N}")
    print(f"log10(k_photo): mean={xbar:.6f}  SD={sd_obs:.6f} dex  MAD/0.6745={mad_obs:.6f} dex")
    print(f"  即峰峰 {x.max()-x.min():.6f} dex")
    for key in ("se_prod", "se_analytic", "se_boot"):
        v = np.array([q[key] for q in rows if q[key] is not None], float)
        if v.size == 0:
            print(f"  {key}: 无数据"); continue
        print(f"  {key:12s} n={v.size} median={np.median(v):.6f} dex  "
              f"(p10 {np.percentile(v,10):.6f}, p90 {np.percentile(v,90):.6f})")
    se = np.array([q["se_boot"] for q in rows], float)
    chi2 = float(np.sum(((x - xbar) / se) ** 2) / (N - 1))
    print()
    print(f"χ²_red (用自助法误差棒, 对全局均值) = {chi2:.1f}  (dof={N-1})")
    print(f"⇒ 帧间离散 / 典型误差棒 = {sd_obs / np.median(se):.1f} 倍")
    excess2 = sd_obs ** 2 - float(np.median(se)) ** 2
    print(f"σ_excess = sqrt(σ_obs² − σ_err²) = {math.sqrt(excess2) if excess2>0 else float('nan'):.6f} dex")
    se_a = np.array([q["se_analytic"] for q in rows], float)
    excess2a = sd_obs ** 2 - float(np.median(se_a)) ** 2
    print(f"  (解析误差棒口径) σ_excess = {math.sqrt(excess2a) if excess2a>0 else float('nan'):.6f} dex")

    # ── 外部相关 ──
    print()
    print("--- 外部相关（对 log10 k_photo）---")
    from scipy import stats
    def corr(name, vals):
        v = np.array(vals, float)
        m = np.isfinite(v)
        if m.sum() < 5 or np.std(v[m]) == 0:
            print(f"  {name}: 不可算"); return
        r_, p_ = stats.pearsonr(v[m], x[m])
        sl = stats.linregress(v[m], x[m])
        print(f"  {name:22s} n={m.sum():2d}  r={r_:+.3f}  p={p_:.3g}  slope={sl.slope:+.3e}")
    t0 = None
    tsecs = []
    for q in rows:
        if q["dt"]:
            dt = datetime.fromisoformat(q["dt"])
            if t0 is None:
                t0 = dt
            tsecs.append((dt - t0).total_seconds())
        else:
            tsecs.append(np.nan)
    corr("观测时间(since first)", tsecs)
    corr("天光/背景", [q["background"] for q in rows])
    corr("星数 n_sources", [q["n_sources"] for q in rows])
    corr("中位通量", [q["median_flux"] for q in rows])
    corr("n_used", [q["n_used"] for q in rows])
    corr("场后残差 sigma_resid", [q["sigma_resid_dex"] for q in rows])
    # 同夜内趋势
    print()
    print("--- 同夜内时间趋势（夜=日期）---")
    bynight = {}
    for q, ts in zip(rows, tsecs):
        bynight.setdefault(q["date"], []).append((ts, q["log10_k"]))
    for d in sorted(bynight):
        v = bynight[d]
        if len(v) < 3:
            continue
        ts = np.array([a for a, _ in v]); y = np.array([b for _, b in v])
        if np.std(ts) == 0:
            continue
        sl = stats.linregress(ts, y)
        print(f"  {d}: n={len(v)} slope={sl.slope:+.3e} dex/s  r={sl.rvalue:+.3f} p={sl.pvalue:.3g}")
    # 指向组
    print()
    print("--- 指向组 (M1..M6) ---")
    groups = {}
    for q in rows:
        groups.setdefault(q["mirror"], []).append(q["log10_k"])
    for g in sorted(groups):
        v = np.array(groups[g])
        print(f"  {g}: n={len(v)} median={np.median(v):.6f} SD={np.std(v, ddof=1) if len(v)>1 else 0:.6f}")
    arrs = [np.array(groups[g]) for g in sorted(groups) if len(groups[g]) > 1]
    if len(arrs) > 1:
        F, p = stats.f_oneway(*arrs)
        print(f"  one-way ANOVA: F={F:.3f} p={p:.4g}")
    # 望远镜
    print()
    print("--- 望远镜 (T2/T3) ---")
    for t in sorted({q["telescope"] for q in rows}):
        v = np.array([q["log10_k"] for q in rows if q["telescope"] == t])
        print(f"  {t}: n={len(v)} median={np.median(v):.6f} SD={np.std(v, ddof=1):.6f}")

    # ── 同指向近邻帧对照 ──
    print()
    print("--- 同指向(M)帧对：|Δlog10 k| 按 Δt 分箱 vs 全随机帧对 ---")
    import itertools
    pairs = []
    for a, b in itertools.combinations(rows, 2):
        if a["dt"] and b["dt"]:
            dt = abs((datetime.fromisoformat(a["dt"]) - datetime.fromisoformat(b["dt"])).total_seconds())
            same = (a["mirror"] == b["mirror"])
            pairs.append((dt, abs(a["log10_k"] - b["log10_k"]), same))
    allsc = np.array([p[1] for p in pairs])
    print(f"  全部帧对 n={len(pairs)} 中位|Δ|={np.median(allsc):.6f} dex")
    for lo, hi, lab in [(0, 3600, "<1h"), (3600, 86400, "1h–1d"), (86400, 7*86400, "1d–7d"), (7*86400, 60*86400, "1–8w")]:
        for same in (True, False):
            sub = [p[1] for p in pairs if lo <= p[0] < hi and p[2] == same]
            if len(sub) >= 5:
                tagtxt = "同指向" if same else "异指向"
                print(f"  Δt {lab:7s} {tagtxt}: n={len(sub):3d} 中位|Δ|={np.median(sub):.6f} dex")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
