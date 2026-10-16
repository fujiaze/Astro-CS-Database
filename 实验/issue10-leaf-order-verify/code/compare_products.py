#!/usr/bin/env python3
"""Issue #10 —— 两个已发布 Normalize 产品的科学数值对照（pre-#10 vs post-#10）。

对照面：
  1. sidecar `frame_science.json` 递归 JSON diff（报告键路径）；
  2. signal/properties 逐键 diff；
  3. signal FITS 文件集合 + 逐文件 sha256；
  4. precision.controls 逐控制点对照（reference_snr / flux_variance /
     support_area_sr / leaf_order / leaf_ipix）。
sidecar 自身哈希会随 signal.leaf_order 与 software_sha 变化，属预期；不据此判数值回归。

用法：
    python3 compare_products.py <pre_dir> <post_dir> [--json out.json]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys


def sha256(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def leaf_files(root: str):
    out = {}
    for dp, _d, fs in os.walk(root):
        for f in fs:
            p = os.path.join(dp, f)
            out[os.path.relpath(p, root)] = sha256(p)
    return out


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


def sidecar_diff(a: dict, b: dict):
    fa, fb = flat(a), flat(b)
    keys = sorted(set(fa) | set(fb))
    diffs = []
    for k in keys:
        va, vb = fa.get(k, "<absent>"), fb.get(k, "<absent>")
        if va != vb:
            diffs.append({"path": k, "pre": va, "post": vb})
    return diffs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pre_dir")
    ap.add_argument("post_dir")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    pre, post = a.pre_dir.rstrip("/"), a.post_dir.rstrip("/")
    res = {"pre": pre, "post": post}

    sa = json.load(open(os.path.join(pre, "frame_science.json"), encoding="utf-8"))
    sb = json.load(open(os.path.join(post, "frame_science.json"), encoding="utf-8"))
    res["sidecar_sha256"] = {"pre": sha256(os.path.join(pre, "frame_science.json")),
                             "post": sha256(os.path.join(post, "frame_science.json"))}
    res["sidecar_diffs"] = sidecar_diff(sa, sb)
    res["sidecar_identical"] = not res["sidecar_diffs"]

    # 科学字段（除 leaf_order / software_sha 等元数据）
    sci_paths = ["precision.frame_summary.reference_snr",
                 "precision.frame_summary.supported_area_sr",
                 "precision.frame_summary.n_valid"]
    res["science_summary"] = {p: {"pre": flat(sa).get("." + p), "post": flat(sb).get("." + p)}
                              for p in sci_paths}

    # controls
    ca, cb = sa.get("precision", {}).get("controls", []), sb.get("precision", {}).get("controls", [])
    res["n_controls"] = {"pre": len(ca), "post": len(cb)}
    ctrl_diff = []
    if len(ca) == len(cb):
        for i, (x, y) in enumerate(zip(ca, cb)):
            for f in ("control_id", "reference_snr", "flux_variance", "support_area_sr",
                      "leaf_order", "leaf_ipix", "covariance_rank", "condition_number"):
                if x.get(f) != y.get(f):
                    ctrl_diff.append({"i": i, "field": f, "pre": x.get(f), "post": y.get(f)})
    else:
        ctrl_diff.append({"error": "control count differs"})
    res["control_diffs"] = ctrl_diff
    res["controls_identical"] = not ctrl_diff

    # signal FITS
    fa = leaf_files(os.path.join(pre, "signal"))
    fb = leaf_files(os.path.join(post, "signal"))
    only_a = sorted(set(fa) - set(fb))
    only_b = sorted(set(fb) - set(fa))
    changed = sorted(k for k in set(fa) & set(fb) if fa[k] != fb[k])
    res["signal_files"] = {"n_pre": len(fa), "n_post": len(fb),
                           "only_pre": only_a[:20], "only_post": only_b[:20],
                           "changed": changed[:20], "n_changed": len(changed)}
    res["signal_identical"] = not (only_a or only_b or changed)

    # properties
    def props(p):
        kv = {}
        for line in open(os.path.join(p, "signal", "properties"), encoding="utf-8", errors="replace"):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                kv[k] = v
        return kv
    pa, pb = props(pre), props(post)
    pkeys = sorted(set(pa) | set(pb))
    prop_diff = [{"key": k, "pre": pa.get(k), "post": pb.get(k)}
                 for k in pkeys if pa.get(k) != pb.get(k)]
    res["properties_diffs"] = prop_diff

    print("=== Issue #10 pre/post product comparison ===")
    print(f"pre : {pre}")
    print(f"post: {post}")
    print(f"sidecar_identical={res['sidecar_identical']}  controls_identical={res['controls_identical']}  "
          f"signal_identical={res['signal_identical']}  signal_files(pre/post)={len(fa)}/{len(fb)}")
    print("-- sidecar diffs --")
    for d in res["sidecar_diffs"]:
        print(f"   {d['path']}: {d['pre']!r} -> {d['post']!r}")
    print("-- control diffs --")
    for d in res["control_diffs"]:
        print("  ", d)
    print("-- signal file diffs --")
    print("  ", res["signal_files"])
    print("-- properties diffs --")
    for d in prop_diff:
        print(f"   {d['key']}: {d['pre']!r} -> {d['post']!r}")
    if a.json:
        json.dump(res, open(a.json, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
