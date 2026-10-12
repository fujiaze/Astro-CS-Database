#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1：把"不可靠"这一判定本身查清。

B1 物理协变量：逐帧 dev 与 输入 FITS 头的气团/曝光/时间、产物天光背景 的回归。
B2 增值检验：在**共同星**上比较"应用 k_photo 后 / 不应用"的帧对散度。

共同星识别：同一颗 Gaia 星在不同帧的 `F_syn` 相同（XPSD 绝对谱积分），
故按 F_syn 配对；`F_syn = F_instr / 10^r`，F_instr 由 p1_flux 按位置匹配得到。
  · 不应用 k_photo：Δr = r_a − r_b 关于 **0** 的散度（把两帧当作已同标度）
  · 应用 k_photo：Δr − median(Δr) 的散度（帧级偏移被 k_photo 吃掉）

只读 run/PHOT-FIX/** 与输入 FITS 头；输出 results/p1fix/reliability_test.json
"""
from __future__ import annotations
import json, math, re, struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
C_TUKEY = 4.685


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
            return new
        loc = new
    return loc


def read_fits_header_vals(path):
    """只读头，取 AIRMASS / EXPTIME / DATE-OBS / FILTER。"""
    out = {}
    try:
        with open(path, "rb") as f:
            data = f.read(2880 * 5)
        for i in range(0, len(data), 80):
            c = data[i:i + 80].decode("ascii", errors="replace")
            if c.startswith("END"):
                break
            if "=" in c:
                k = c[:8].strip()
                v = c[10:].split("/")[0].strip()
                if k in ("AIRMASS", "EXPTIME", "DATE-OBS", "FILTER", "ZD", "MOONPHAS"):
                    out[k] = v.strip("'").strip()
    except Exception as e:
        out["_err"] = str(e)
    return out


def canon(s):
    return s.replace("cleaned_", "").replace(".fts", "").replace("@", "_")


def nn_index(src_xy, dst_xy, tol=5.0):
    out = np.full(len(dst_xy), -1, int)
    if len(src_xy) == 0:
        return out
    sx, sy = src_xy[:, 0], src_xy[:, 1]
    for i, (dx, dy) in enumerate(dst_xy):
        d2 = (sx - dx) ** 2 + (sy - dy) ** 2
        j = int(np.argmin(d2))
        if d2[j] <= tol * tol:
            out[i] = j
    return out


def main():
    frames = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        flux = json.loads((SRC / tag / "p1_flux.json").read_text())
        srcs = json.loads((SRC / tag / "p1_sources.json").read_text())
        fmap = {canon(f["file"]): f for f in flux["frames"]}
        smap = {canon(f["file"]): f for f in srcs["frames"]}
        for fk in sorted(phot["spatial_gain"]):
            sg = phot["spatial_gain"][fk]; pd = phot["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"])
            X = np.array(s["x"]); Y = np.array(s["y"]); u = np.array(s["used"], bool)
            kp = pd.get("k_photo")
            if not (kp and kp > 0):
                continue
            ffl = np.full(len(X), np.nan)
            if fk in fmap:
                R = fmap[fk]["results"]
                rxy = np.array([[q["x"], q["y"]] for q in R])
                for i, j in enumerate(nn_index(rxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        ffl[i] = R[j].get("flux", np.nan) or np.nan
            fsyn = ffl / np.power(10.0, r)
            fsyn[~np.isfinite(fsyn)] = np.nan
            inp = next((f["input_light"] for f in phot["frames"] if f.get("frame_key") == fk), None)
            hdr = read_fits_header_vals(inp) if inp else {}
            bg = smap[fk].get("background") if fk in smap else None
            frames.append({
                "tag": tag, "frame": fk, "r": r, "used": u, "fsyn": fsyn,
                "loc": tukey_location(r[u]), "k_photo": kp,
                "airmass": float(hdr["AIRMASS"]) if hdr.get("AIRMASS") else None,
                "exptime": float(hdr["EXPTIME"]) if hdr.get("EXPTIME") else None,
                "filter": hdr.get("FILTER"), "date_obs": hdr.get("DATE-OBS"),
                "background": float(bg) if bg else None,
                "n_used": int(u.sum()),
                "mirror": (re.search(r"M(\d+)_T", fk).group(1) if re.search(r"M(\d+)_T", fk) else None),
                "telescope": (re.search(r"M\d+_(T\d)", fk).group(1) if re.search(r"M\d+_(T\d)", fk) else None),
            })
    loc = np.array([f["loc"] for f in frames])
    dev = np.abs(loc - np.median(loc))
    print("frames =", len(frames))

    # ── B1 ──────────────────────────────────────────────────────────────
    from scipy import stats
    print()
    print("=== B1 逐帧 dev 与物理协变量 ===")
    b1 = {}
    for name, key in (("气团 AIRMASS", "airmass"), ("天光背景", "background"), ("曝光 EXPTIME", "exptime")):
        v = np.array([f[key] if f[key] is not None else np.nan for f in frames], float)
        m = np.isfinite(v) & np.isfinite(dev)
        if m.sum() < 8:
            print("  %-14s 可用 %d 帧，样本不足" % (name, m.sum())); continue
        if np.std(v[m]) == 0:
            print("  %-14s 全帧取值相同(%g) ⇒ 无判别力，跳过" % (name, v[m][0])); continue
        lr = stats.linregress(v[m], dev[m])
        b1[name] = {"n": int(m.sum()), "slope": lr.slope, "r": lr.rvalue, "p": lr.pvalue, "r2": lr.rvalue ** 2}
        print("  %-14s n=%2d slope=%+.4g  r=%+.3f  p=%.3g  R²=%.3f" %
              (name, m.sum(), lr.slope, lr.rvalue, lr.pvalue, lr.rvalue ** 2))
    # 多元：气团 + 背景
    va = np.array([f["airmass"] if f["airmass"] is not None else np.nan for f in frames], float)
    vb = np.array([f["background"] if f["background"] is not None else np.nan for f in frames], float)
    m = np.isfinite(va) & np.isfinite(vb) & np.isfinite(dev)
    if m.sum() > 10:
        Xd = np.column_stack([np.ones(m.sum()), va[m], vb[m]])
        beta, *_ = np.linalg.lstsq(Xd, dev[m], rcond=None)
        pred = Xd @ beta
        r2 = 1 - ((dev[m] - pred) ** 2).sum() / ((dev[m] - dev[m].mean()) ** 2).sum()
        print("  多元(气团+背景)  R²=%.3f  n=%d  beta=%s" % (r2, m.sum(), np.round(beta, 6)))
        b1["multi_airmass_bg"] = {"r2": float(r2), "n": int(m.sum())}
    # 按夜分组（date 作为分类变量）
    bynight = {}
    for f in frames:
        d = (f["date_obs"] or "?")[:10]
        bynight.setdefault(d, []).append(f["loc"])
    if len(bynight) > 1:
        arrs = [np.array(v) for v in bynight.values() if len(v) > 1]
        if len(arrs) > 1:
            F, p = stats.f_oneway(*arrs)
            print("  按夜(date) 分组 one-way ANOVA: F=%.3f p=%.3g (组数=%d)" % (F, p, len(arrs)))
            b1["night_anova"] = {"F": float(F), "p": float(p), "n_groups": len(arrs)}

    # ── B2 ──────────────────────────────────────────────────────────────
    print()
    print("=== B2 共同星上的帧对散度：不应用 vs 应用 k_photo ===")
    pairs = []
    for i in range(len(frames)):
        for j in range(i + 1, len(frames)):
            a, b = frames[i], frames[j]
            if a["mirror"] != b["mirror"] or a["telescope"] != b["telescope"]:
                continue
            fa = a["fsyn"][a["used"]]; ra = a["r"][a["used"]]
            fb = b["fsyn"][b["used"]]; rb = b["r"][b["used"]]
            oa = np.isfinite(fa); ob = np.isfinite(fb)
            if oa.sum() < 30 or ob.sum() < 30:
                continue
            fa_, ra_ = fa[oa], ra[oa]
            fb_, rb_ = fb[ob], rb[ob]
            used_b = np.zeros(len(fb_), bool)
            d1, d2 = [], []
            for k in range(len(fa_)):
                rel = np.abs(fb_ - fa_[k]) / fa_[k]
                cand = np.where((rel < 0.005) & (~used_b))[0]
                if len(cand) == 0:
                    continue
                m_ = cand[np.argmin(rel[cand])]
                used_b[m_] = True
                d1.append(ra_[k]); d2.append(rb_[m_])
            if len(d1) < 20:
                continue
            d1 = np.array(d1); d2 = np.array(d2)
            dr = d1 - d2
            raw = float(np.median(np.abs(dr - 0.0)) / MAD)      # 不应用：关于 0
            cal = float(np.median(np.abs(dr - np.median(dr))) / MAD)  # 应用：偏移被吃掉
            pairs.append({"a": a["frame"], "b": b["frame"], "n_common": len(dr),
                          "median_dr": float(np.median(dr)), "raw_dex": raw, "cal_dex": cal})
    if pairs:
        raw = np.array([p["raw_dex"] for p in pairs]); cal = np.array([p["cal_dex"] for p in pairs])
        off = np.array([abs(p["median_dr"]) for p in pairs])
        print("  同指向帧对 n=%d" % len(pairs))
        print("  (1) 不应用 k_photo：Δr 关于 0 的散度  中位 = %.5f dex" % np.median(raw))
        print("  (2) 应用 k_photo：Δr−median 的散度   中位 = %.5f dex" % np.median(cal))
        print("  降幅 = %.2f×  (%.1f%% 的帧对散度来自帧级偏移)" %
              (np.median(raw) / max(np.median(cal), 1e-12),
               100.0 * (1 - np.median(cal) / max(np.median(raw), 1e-12))))
        print("  帧级偏移 |median Δr| 中位 = %.5f dex ; 逐星残余散度中位 = %.5f dex" %
              (np.median(off), np.median(cal)))
        rng = np.random.default_rng(7)
        bs = [np.median(cal[rng.integers(0, len(cal), len(cal))]) for _ in range(2000)]
        print("  应用后散度中位 95%% CI = [%.5f, %.5f]" % (np.percentile(bs, 2.5), np.percentile(bs, 97.5)))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "reliability_test.json").write_text(json.dumps(
        {"b1": b1, "b2_pairs": pairs[:400],
         "b2_summary": ({"n_pairs": len(pairs), "raw_med": float(np.median(raw)),
                         "cal_med": float(np.median(cal))} if pairs else None)}, indent=1, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
