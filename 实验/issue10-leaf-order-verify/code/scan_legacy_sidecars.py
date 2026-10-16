#!/usr/bin/env python3
"""Issue #10 —— 扫描仓内历史 `frame_science.json`，给出"旧格式受影响产品"清单。

对每个 sidecar 独立确定真叶阶 K（优先级）：
  1. <product>/signal/properties 的 hips_order + 9（writer 口径：hips_order=K−9）；
  2. controls[*].leaf_order 的唯一值；
  3. sidecar signal.leaf_order（仅当上面都不可得）——此时不可判"错"，只登记。
然后判定该 sidecar 的 signal.leaf_order 是否等于 bug 签名 K−9（应为 K）。

只做只读扫描，不改任何产品。

用法：
    python3 scan_legacy_sidecars.py [--root run] [--json out.json]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from collections import Counter


def read_props(path: str) -> dict:
    kv = {}
    if not os.path.isfile(path):
        return kv
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                kv[k.strip()] = v.strip()
    return kv


def norder_leaf(signal_dir: str):
    ns = []
    if os.path.isdir(signal_dir):
        for n in os.listdir(signal_dir):
            m = re.fullmatch(r"Norder(\d+)", n)
            if m:
                ns.append(int(m.group(1)))
    return max(ns) if ns else None


def sha256(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def classify(sidecar: str):
    try:
        d = json.load(open(sidecar, encoding="utf-8"))
    except Exception as e:  # noqa: BLE001
        return {"sidecar": sidecar, "error": f"parse: {e}"}
    if not isinstance(d, dict) or d.get("schema_id") != "acsd.normalize.frame-science/v1":
        return {"sidecar": sidecar, "error": "not frame-science/v1"}
    sig = d.get("signal", {}) or {}
    sig_leaf = sig.get("leaf_order")
    controls = (d.get("precision", {}) or {}).get("controls", []) or []
    ctrl = sorted({c.get("leaf_order") for c in controls})
    # product root = sidecar 的目录；发布面为 <frame_key>/frame_science.json
    root = os.path.dirname(sidecar)
    props = read_props(os.path.join(root, "signal", "properties"))
    hips_order = int(props["hips_order"]) if "hips_order" in props else None
    leaf_dir = norder_leaf(os.path.join(root, "signal"))
    source = None
    K = None
    if hips_order is not None:
        K, source = hips_order + 9, "properties.hips_order+9"
    elif len(ctrl) == 1 and ctrl[0] is not None:
        K, source = int(ctrl[0]), "controls[*].leaf_order"
    else:
        K, source = (int(sig_leaf) if sig_leaf else None), "sidecar.signal.leaf_order"
    bug = (K is not None and sig_leaf is not None and sig_leaf == K - 9 and K != 9)
    return {
        "sidecar": sidecar,
        "product_root": root,
        "software_sha": d.get("software_sha", ""),
        "signal_leaf_order": sig_leaf,
        "true_K": K,
        "K_source": source,
        "controls_leaf_orders": ctrl,
        "properties_hips_order": hips_order,
        "signal_leaf_norder": leaf_dir,
        "bug_signature_K-9": bug,
        "sha256": sha256(sidecar),
        "size_bytes": os.path.getsize(sidecar),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="run")
    ap.add_argument("--json", default=None)
    a = ap.parse_args()
    rows = []
    for dirpath, _dirs, files in os.walk(a.root):
        if "frame_science.json" in files:
            rows.append(classify(os.path.join(dirpath, "frame_science.json")))
    ok = [r for r in rows if "error" not in r]
    bad = [r for r in rows if r.get("bug_signature_K-9")]
    err = [r for r in rows if "error" in r]
    by_sha = Counter(r["software_sha"] for r in ok)
    print(f"scanned sidecars: {len(rows)} (ok={len(ok)}, parse/schema errors={len(err)})")
    print(f"bug-signature (signal.leaf_order==K-9, K!=9): {len(bad)}")
    print("software_sha distribution:", dict(by_sha))
    print("K distribution:", dict(Counter(r["true_K"] for r in ok)))
    for r in bad[:10]:
        print(f"  BUG  {r['product_root']}  signal.leaf_order={r['signal_leaf_order']} "
              f"K={r['true_K']} ({r['K_source']}) props_hips_order={r['properties_hips_order']} "
              f"leaf_Norder={r['signal_leaf_norder']} sw={r['software_sha'][:12]}")
    if a.json:
        json.dump({"n_scanned": len(rows), "n_bug_signature": len(bad),
                   "n_error": len(err), "rows": rows,
                   "software_sha_distribution": dict(by_sha)},
                  open(a.json, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
        print("json:", a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
