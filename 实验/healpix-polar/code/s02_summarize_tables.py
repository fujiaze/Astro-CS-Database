#!/usr/bin/env python3
"""summarize_s02_tables.py — 生成 S02 报告所需的修前/修后（+最小 NORM-01 变体）成对表。

输出：run/ACSDSDOC-02/S02/results/s02_tables.md（人读）+ stdout
"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "results")
MACH = 2.220446049250313e-16
ARCSEC = math.pi / 180.0 / 3600.0


def L(n):
    with open(os.path.join(RES, n)) as f:
        return json.load(f)["experiments"]


def cells(rows):
    return {(r["pos_class"], r["ra"], r["dec"], r["nside"], r["pixfrac"], r["scale_arcsec"]): r
            for r in rows if r.get("id") == "S02_matrix_cell"}


def q(vals, p):
    s = sorted(vals)
    if not s:
        return None
    if len(s) == 1:
        return s[0]
    i = p * (len(s) - 1)
    lo = int(math.floor(i)); hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (i - lo)


def main():
    b = L("results_before.json"); a = L("results_after.json")
    n1 = L("results_norm01_only.json"); neg = L("results_negative_legacy_cross.json")
    cb, ca, c1, cn = cells(b), cells(a), cells(n1), cells(neg)
    out = []
    w = out.append

    w("# S02 成对读数（自动生成）\n")
    w(f"- cells: before={len(cb)} after={len(ca)} norm01_only={len(c1)} negative={len(cn)}")
    m = max(abs(v["epsilon_j"]) for v in cb.values())
    w(f"- max|ε_j| before = {m:.4e}；norm01_only = "
      f"{max(abs(v['epsilon_j']) for v in c1.values()):.4e}；after = "
      f"{max(abs(v['epsilon_j']) for v in ca.values()):.4e}；negative = "
      f"{max(abs(v['epsilon_j']) for v in cn.values()):.4e}")
    for nm, c in (("before", cb), ("norm01_only", c1), ("after", ca), ("negative", cn)):
        w(f"- {nm}: #cells |ε|>1e-6 = {sum(1 for v in c.values() if abs(v['epsilon_j']) > 1e-6)}")

    # E12 成对
    def e12(rows):
        return [r for r in rows if r.get("id") == "E12_seam_ra_survey"][0]
    w("\n## E12_seam_ra_survey（0.04″、nside=8192、pixfrac=0.8、dec=asin(2/3)）\n")
    w("| RA° | before ε_j | norm01 ε_j | after ε_j | negative ε_j | n_leaves | n_cand(query) |")
    w("|---|---|---|---|---|---|---|")
    for i, ra in enumerate(e12(b)["ra"]):
        w(f"| {ra:g} | {e12(b)['sumw_m1'][i]:+.4e} | {e12(n1)['sumw_m1'][i]:+.4e} | "
          f"{e12(a)['sumw_m1'][i]:+.4e} | {e12(neg)['sumw_m1'][i]:+.4e} | "
          f"{int(e12(a)['n_leaves'][i])} | {int(e12(a)['n_candidates_query'][i])} |")

    # E13 成对
    w("\n## E13_perdrop_close（复刻 T06；pixfrac=0.8）\n")
    def e13(rows, cls, ns):
        for r in rows:
            if r.get("id") == "E13_perdrop_close" and r["pos_class"] == cls and r["nside"] == ns:
                return r
        return None
    for cls in ("interior", "leaf_edge", "face_seam"):
        for ns in (512, 16384):
            rb, r1, ra = e13(b, cls, ns), e13(n1, cls, ns), e13(a, cls, ns)
            w(f"\n### {cls} / nside={ns}\n")
            w("| scale″ | before | norm01 | after | before nleaf | after nleaf |")
            w("|---|---|---|---|---|---|")
            for i, sc in enumerate(rb["scale_arcsec"]):
                w(f"| {sc:g} | {rb['sumw_m1'][i]:+.4e} | {r1['sumw_m1'][i]:+.4e} | "
                  f"{ra['sumw_m1'][i]:+.4e} | {int(rb['n_leaves'][i])} | {int(ra['n_leaves'][i])} |")

    # 全矩阵：逐 (nside,pixfrac,scale) 跨位置类分位数
    w("\n## S02 全矩阵分组分位数（|ε_j|，跨 24 个位置类）\n")
    groups = {}
    for k in cb:
        groups.setdefault((k[3], k[4], k[5]), []).append(k)
    w("| nside | pixfrac | scale″ | θ_drop[rad] | n_pos | before max | before p90 | after max | after p90 | after p99 | after 最坏格 | 最坏格 n_leaves | cand |")
    w("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for g in sorted(groups):
        ks = groups[g]
        eb = [abs(cb[k]["epsilon_j"]) for k in ks]
        ea = [abs(ca[k]["epsilon_j"]) for k in ks]
        wk = max(ks, key=lambda k: abs(ca[k]["epsilon_j"]))
        w(f"| {g[0]} | {g[1]} | {g[2]:g} | {g[1]*g[2]*ARCSEC:.3e} | {len(ks)} | "
          f"{max(eb):.3e} | {q(eb,.9):.3e} | {max(ea):.3e} | {q(ea,.9):.3e} | {q(ea,.99):.3e} | "
          f"{ca[wk]['pos_class']} | {ca[wk]['n_leaves']} | {ca[wk]['n_candidates_query']} |")

    # 逐尺度：全矩阵 max，用于标度律
    w("\n## 逐 drop 尺度：全矩阵 max|ε_j|（跨 nside × pixfrac × 位置类）\n")
    w("| scale″ | θ_drop[rad] | before max | norm01 max | after max | after/θ | after·θ/ε_mach |")
    w("|---|---|---|---|---|---|---|")
    for sc in (0.04, 0.5, 2.0, 10.0, 60.0):
        kb = [k for k in cb if k[5] == sc]
        mb = max(abs(cb[k]["epsilon_j"]) for k in kb)
        m1 = max(abs(c1[k]["epsilon_j"]) for k in kb)
        ma = max(abs(ca[k]["epsilon_j"]) for k in kb)
        ws = max(kb, key=lambda k: abs(ca[k]["epsilon_j"]))
        th = ca[ws]["drop_side_arcsec"] * ARCSEC
        w(f"| {sc:g} | {th:.3e} | {mb:.3e} | {m1:.3e} | {ma:.3e} | {ma/th:.3e} | "
          f"{ma*th/MACH:.3f} (pixfrac={ca[ws]['pixfrac']}, nside={ca[ws]['nside']}) |")

    # 小 θ 支的拟合
    pts = []
    for sc in (0.04, 0.5, 2.0):
        kb = [k for k in cb if k[5] == sc]
        ma = max(abs(ca[k]["epsilon_j"]) for k in kb)
        ws = max(kb, key=lambda k: abs(ca[k]["epsilon_j"]))
        pts.append((math.log(ca[ws]["drop_side_arcsec"] * ARCSEC), math.log(ma)))
    n = len(pts)
    mx = sum(p[0] for p in pts) / n; my = sum(p[1] for p in pts) / n
    slope = sum((p[0]-mx)*(p[1]-my) for p in pts) / sum((p[0]-mx)**2 for p in pts)
    Cs = [math.exp(p[1]) * math.exp(p[0]) / MACH for p in pts]
    w(f"\n- 小 θ 支（0.04″–2″）log-log 斜率 dln|ε|/dln θ = **{slope:.3f}**")
    w(f"- C = |ε|·θ/ε_mach = {['%.3f' % c for c in Cs]}（中位 {sorted(Cs)[len(Cs)//2]:.3f}）")
    w(f"- 由 C_med·ε_mach/θ = 1e-6 ⇒ θ_1e-6 = {sorted(Cs)[len(Cs)//2]*MACH/1e-6:.3e} rad "
      f"= {sorted(Cs)[len(Cs)//2]*MACH/1e-6/ARCSEC:.3e}″")

    txt = "\n".join(out) + "\n"
    with open(os.path.join(RES, "s02_tables.md"), "w") as f:
        f.write(txt)
    print(txt)


if __name__ == "__main__":
    main()
