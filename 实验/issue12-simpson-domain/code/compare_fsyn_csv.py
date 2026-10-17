#!/usr/bin/env python3
"""Issue #12 —— 真实 Gaia XPSD F_syn/零点/光子率 pre/post CSV 对拍。

用法: python3 compare_fsyn_csv.py <pre.csv> <post.csv> [--json out]
"""
from __future__ import annotations

import argparse
import csv
import json
import sys


def load(p):
    with open(p) as f:
        return list(csv.DictReader(f))


def fnum(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pre")
    ap.add_argument("post")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    pre, post = load(a.pre), load(a.post)
    res = {"n_pre": len(pre), "n_post": len(post), "n_equal_length": len(pre) == len(post)}
    cols = ("F_syn", "zp_syn", "Q_gamma")
    maxd = {c: 0.0 for c in cols}
    ndiff = 0
    for x, y in zip(pre, post):
        for c in cols:
            vx, vy = fnum(x.get(c)), fnum(y.get(c))
            if vx is None or vy is None:
                if vx != vy:
                    ndiff += 1
                continue
            d = abs(vx - vy) / max(1e-300, max(abs(vx), abs(vy)))
            maxd[c] = max(maxd[c], d)
            if vx != vy:
                ndiff += 1
    res["max_rel_diff"] = maxd
    res["n_cells_differing"] = ndiff
    res["bit_identical"] = (ndiff == 0)
    # 光子率单位独立核对: SI 换算 F_syn*1e-9/(hc) vs 独立梯形
    rels = [fnum(r.get("rel_trap")) for r in post if fnum(r.get("rel_trap")) is not None]
    zs = [fnum(r.get("zp_syn")) for r in post if fnum(r.get("zp_syn")) is not None]
    if rels:
        rels.sort()
        res["photon_rate_rel_si_vs_trapezoid"] = {
            "max": rels[-1], "median": rels[len(rels) // 2], "n": len(rels)}
    if zs:
        zs.sort()
        res["zp_syn_median_mag"] = zs[len(zs) // 2]
    print(f"n={len(pre)}/{len(post)} bit_identical={res['bit_identical']} "
          f"cells_diff={ndiff} max_rel_diff={ {k: '%.3e' % v for k, v in maxd.items()} }")
    if rels:
        print(f"photon-rate rel(SI vs trapezoid): max={rels[-1]:.3e} median={rels[len(rels)//2]:.3e}")
    if a.json:
        json.dump(res, open(a.json, "w"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0 if res["bit_identical"] and res["n_equal_length"] else 1


if __name__ == "__main__":
    sys.exit(main())
