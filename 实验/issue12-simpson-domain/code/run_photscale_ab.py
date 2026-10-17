#!/usr/bin/env python3
"""Issue #12 —— 真实光度定标路径（p1_phot.json 应用标度）pre/post 对拍。

用真实 M42 T3 M1 Red 帧 + GaiaDR3SP + 真实响应曲线配置（`photometry.fit.enabled=true`）
跑 `acsd normalize --json cfg -y`，比较:
  · p1_phot.json: photscal / photscale_source / photscale_spread_dex / photscale_fit /
    passband_identity / 逐帧 k_photo
  · Phase1 signal 产品文件 sha256（应用标度后的像素面）
预/后二进制由 build_ab_pre12.sh 产出。

用法: python3 run_photscale_ab.py --pre /tmp/acsd_pre12 --post /tmp/acsd_post12 \
        --out /workspace/issue12-scratch
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
GAIA = os.path.join(REPO, "gaia", "GaiaDR3SP")
TD = os.path.join(REPO, "testdata", "M42_T2T3_mosaic_Flying_dutchman")
T3CAL = os.path.join(REPO, "testdata", "T3 calibration files")


def make_cfg(out_dir: str) -> dict:
    return {
        "schema_version": "1",
        "blocks": [{
            "name": "issue12_phot",
            "input_lights": [os.path.join(TD, "T3/M1/M42_M1_T3_flying_dutchman-20251126@041940-300S-Red.fts")],
            "master_bias": os.path.join(T3CAL, "masterBias_BIN-1_4096x4096.xisf"),
            "master_dark": os.path.join(T3CAL, "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf"),
            "master_flat": os.path.join(T3CAL, "masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf"),
            "output_dir": out_dir,
            "drizzle": {"nested": 1, "pixfrac": 0.8, "precision_mode": 1},
            "filter_passband": "Baader R",
            "wcs": {"init_source": "header_pointing", "gaia_data_dir": GAIA},
            "photometry": {"fit": {"enabled": True, "gaia_data_dir": GAIA,
                                   "filter": "Baader R", "qe_name": "Ideal QE curve"}},
            "dark_optimization": True,
            "dark_scale_factor": 0.5,
            "master_flat_normalize": "median",
            "master_units": {"bias": "normalized", "dark": "normalized", "flat": "normalized"},
            "master_scale": {"bias": 65535, "dark": 65535, "flat": 65535},
        }],
    }


def find(name: str, root: str):
    for dp, _dd, fs in os.walk(root):
        if name in fs:
            return os.path.join(dp, name)
    return None


def sha_tree(d: str) -> dict:
    out = {}
    for dp, _dd, fs in os.walk(d):
        for f in sorted(fs):
            p = os.path.join(dp, f)
            h = hashlib.sha256()
            with open(p, "rb") as fh:
                for c in iter(lambda: fh.read(1 << 20), b""):
                    h.update(c)
            out[os.path.relpath(p, d)] = h.hexdigest()
    return out


def run(tag: str, acsd: str, out_root: str):
    out_dir = os.path.join(out_root, "norm_" + tag)
    shutil.rmtree(out_dir, ignore_errors=True)
    cfg_path = os.path.join(out_root, "cfg_" + tag + ".json")
    json.dump(make_cfg(out_dir), open(cfg_path, "w"), indent=2)
    t0 = time.time()
    p = subprocess.run([acsd, "normalize", "--json", cfg_path, "-y"],
                       capture_output=True, text=True)
    dt = time.time() - t0
    open(os.path.join(out_root, "norm_" + tag + ".stderr"), "w").write(p.stderr)
    rec = {"tag": tag, "exit": p.returncode, "wall_s": round(dt, 1), "out_dir": out_dir}
    ph = find("p1_phot.json", out_dir)
    if ph:
        d = json.load(open(ph))
        rec["p1_phot_path"] = ph
        rec["photscal"] = d.get("photscal")
        rec["photscale_source"] = d.get("photscale_source")
        rec["photscale_spread_dex"] = d.get("photscale_spread_dex")
        rec["photscale_fit"] = d.get("photscale_fit")
        rec["passband_identity"] = d.get("passband_identity")
        rec["k_photo_by_frame"] = {k: v.get("k_photo") for k, v in (d.get("photscale_detail") or {}).items()}
        rec["n_frames_applied"] = d.get("n_frames_applied")
    sig = find("signal", out_dir)
    if sig:
        rec["signal_sha256"] = sha_tree(sig)
        rec["n_signal_files"] = len(rec["signal_sha256"])
    print(f"[{tag}] exit={rec['exit']} wall={dt:.0f}s photscal={rec.get('photscal')} "
          f"source={rec.get('photscale_source')} spread_dex={rec.get('photscale_spread_dex')} "
          f"n_signal={rec.get('n_signal_files')}", flush=True)
    return rec


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pre", default="/tmp/acsd_pre12")
    ap.add_argument("--post", default="/tmp/acsd_post12")
    ap.add_argument("--out", default="/workspace/issue12-scratch")
    ap.add_argument("--only", default="")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    res = {"pre_bin": a.pre, "post_bin": a.post}
    if a.only != "compare":
        res["pre"] = run("pre", a.pre, a.out)
        res["post"] = run("post", a.post, a.out)
    else:
        res["pre"] = json.load(open(os.path.join(a.out, "photscale_result.json")))["pre"]
        res["post"] = json.load(open(os.path.join(a.out, "photscale_result.json")))["post"]
    pre, post = res["pre"], res["post"]
    diffs = {}
    for k in ("photscal", "photscale_source", "photscale_spread_dex", "photscale_fit",
              "passband_identity", "k_photo_by_frame", "n_frames_applied", "exit"):
        if pre.get(k) != post.get(k):
            diffs[k] = {"pre": pre.get(k), "post": post.get(k)}
    res["photometry_field_diffs"] = diffs
    a_sig, b_sig = pre.get("signal_sha256", {}), post.get("signal_sha256", {})
    res["signal_identical"] = (a_sig == b_sig)
    res["signal_only_pre"] = sorted(set(a_sig) - set(b_sig))[:10]
    res["signal_only_post"] = sorted(set(b_sig) - set(a_sig))[:10]
    res["n_signal_diff_files"] = sum(1 for k in set(a_sig) & set(b_sig) if a_sig[k] != b_sig[k])
    print("photometry field diffs:", json.dumps(diffs, ensure_ascii=False)[:600])
    print("signal identical:", res["signal_identical"], "diff files:", res["n_signal_diff_files"])
    json.dump(res, open(os.path.join(a.out, "photscale_result.json"), "w"),
              indent=2, ensure_ascii=False)
    ok = (pre.get("exit") == 0 and post.get("exit") == 0 and not diffs and res["signal_identical"])
    print("AB_PASS" if ok else "AB_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
