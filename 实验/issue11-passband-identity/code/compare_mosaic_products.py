#!/usr/bin/env python3
"""Issue #11 —— pre/post 同配置 Mosaic 产品对照（科学数值不变性）。

因为 HiPS tile FITS 头里的 `FILTER` 键会随 obs_filter 合法变化，**不能按文件字节**
比较；本脚本比较：
  1. signal 叶 tile 的 **DATA 数组**（astropy），逐 tile 必须逐元素相同；
  2. 上游/科学二进制面 sha256：p2_integrated_signal.bin / _support.bin /
     _wsum.bin / _nused.bin / _nrej.bin / p2_sky_plane.bin / p2_samples.json /
     p2_rejection.json / p2_corrected.json 等（标签无关，应完全相同）；
  3. p2_final.json 递归 diff（预期仅 obs_filter / response_distribution 变化）。
非 signal 面的 run 相关字段（run_id/时间戳）单独列出，不据其判数值回归。

用法：python3 compare_mosaic_products.py <pre_product_or_dir> <post_product_or_dir> [--json out]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys


def find_product(d: str) -> str:
    if os.path.isfile(os.path.join(d, "p2_final.json")):
        return d
    for e in sorted(os.listdir(d)):
        p = os.path.join(d, e)
        if os.path.isdir(p) and os.path.isfile(os.path.join(p, "p2_final.json")):
            return p
    return d


def sha(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def flat(o, path="", acc=None):
    if acc is None:
        acc = {}
    if isinstance(o, dict):
        for k in sorted(o):
            flat(o[k], f"{path}.{k}", acc)
    elif isinstance(o, list):
        for i, v in enumerate(o):
            flat(v, f"{path}[{i}]", acc)
    else:
        acc[path] = o
    return acc


def tile_arrays(product: str):
    from astropy.io import fits
    import glob
    out = {}
    for t in sorted(glob.glob(os.path.join(product, "signal", "Norder*", "Dir*", "Npix*.fits"))):
        rel = os.path.relpath(t, os.path.join(product, "signal"))
        with fits.open(t, memmap=False) as h:
            out[rel] = h[0].data
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pre_dir")
    ap.add_argument("post_dir")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    pre, post = find_product(a.pre_dir), find_product(a.post_dir)
    res = {"pre_product": pre, "post_product": post}

    # 1) signal tile DATA
    ta, tb = tile_arrays(pre), tile_arrays(post)
    res["n_signal_tiles"] = {"pre": len(ta), "post": len(tb)}
    only_a = sorted(set(ta) - set(tb))
    only_b = sorted(set(tb) - set(ta))
    pixel_diffs = []
    import numpy as np
    for k in sorted(set(ta) & set(tb)):
        x, y = ta[k], tb[k]
        if x.shape != y.shape:
            pixel_diffs.append({"tile": k, "reason": "shape", "pre": list(x.shape),
                                "post": list(y.shape)})
            continue
        if not np.array_equal(x, y, equal_nan=True):
            d = np.abs(x.astype("f8") - y.astype("f8"))
            pixel_diffs.append({"tile": k, "reason": "values",
                                "max_abs_diff": float(np.nanmax(d)) if d.size else 0.0})
    res["tiles_only_pre"] = only_a[:20]
    res["tiles_only_post"] = only_b[:20]
    res["pixel_diffs"] = pixel_diffs[:20]
    res["n_pixel_diff_tiles"] = len(pixel_diffs)
    res["signal_pixels_identical"] = not (only_a or only_b or pixel_diffs)

    # 2) binary/json 面 sha256
    files = ["p2_integrated_signal.bin", "p2_integrated_support.bin",
             "p2_integrated_wsum.bin", "p2_integrated_nused.bin",
             "p2_integrated_nrej.bin", "p2_sky_plane.bin", "p2_samples.json",
             "p2_rejection.json", "p2_corrected.json", "p2_integrated.json"]
    binres = {}
    for f in files:
        pa, pb = os.path.join(pre, f), os.path.join(post, f)
        if os.path.isfile(pa) and os.path.isfile(pb):
            binres[f] = {"pre": sha(pa), "post": sha(pb), "identical": sha(pa) == sha(pb)}
        else:
            binres[f] = {"pre": os.path.isfile(pa), "post": os.path.isfile(pb),
                         "identical": None}
    res["binary_faces"] = binres
    res["binary_faces_identical"] = all(v.get("identical") in (True,) for v in binres.values())

    # 3) p2_final.json 递归 diff
    fa = json.load(open(os.path.join(pre, "p2_final.json")))
    fb = json.load(open(os.path.join(post, "p2_final.json")))
    ra, rb = flat(fa), flat(fb)
    diffs = [{"path": k, "pre": ra.get(k, "<absent>"), "post": rb.get(k, "<absent>")}
             for k in sorted(set(ra) | set(rb)) if ra.get(k) != rb.get(k)]
    res["p2_final_diffs"] = diffs

    print("=== Issue #11 pre/post mosaic comparison ===")
    print(f"pre : {pre}\npost: {post}")
    print(f"signal_pixels_identical={res['signal_pixels_identical']} "
          f"(tiles {len(ta)}/{len(tb)}, diff_tiles={res['n_pixel_diff_tiles']})")
    print(f"binary_faces_identical={res['binary_faces_identical']}")
    for f, v in binres.items():
        if v.get("identical") is not True:
            print("  bin-diff:", f, v)
    print("p2_final diffs:")
    for d in diffs:
        print(f"   {d['path']}: {d['pre']!r} -> {d['post']!r}")
    if a.json:
        json.dump(res, open(a.json, "w"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
