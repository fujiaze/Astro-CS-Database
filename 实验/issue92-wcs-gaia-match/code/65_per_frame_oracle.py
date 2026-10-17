#!/usr/bin/env python3
"""Issue #92 A1-FIX —— 六帧各自的独立入帧分母与 pre/post 先验 oracle。

对 `实验/issue92-wcs-gaia-match/fixtures/*.json` 的每一帧:
  1. 由产品 WCS 复算生产同式 FOV 半径 (sqrt|det|·sqrt(W²+H²)/2·1.2, W=H=4096);
  2. 用真实 GaiaDR3SP 以该锥心/半径复刻生产查询口径取星 (run_wcs_probe.py);
  3. 运行 post(修复后)/pre(38eaeba8b^) 投影探针;
  4. 运行独立 Astropy 端到端入帧核对 (50_astropy_crosscheck.py);
  5. 汇总逐帧 n_gaia / post 入帧 / pre 入帧 / Astropy 入帧。

输出 evidence/65-per-frame-oracle.json + 逐帧 evidence/40-gaia-stars-<tag>.csv。
"""
from __future__ import annotations

import argparse
import glob
import json
import math
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CODE = os.path.join(REPO, "实验/issue92-wcs-gaia-match/code")
EVID = os.path.join(REPO, "实验/issue92-wcs-gaia-match/evidence")
FIXTURES = os.path.join(REPO, "实验/issue92-wcs-gaia-match/fixtures")


def tag_of(name: str) -> str:
    m = re.search(r"flying_dutchman-(.+)\.json$", name)
    return m.group(1) if m else os.path.splitext(name)[0]


def fov_radius(w) -> float:
    det = abs(w["cd11"] * w["cd22"] - w["cd12"] * w["cd21"])
    scale = math.sqrt(det) if det > 0 else 0.0
    r = scale * math.sqrt(4096.0 * 4096.0 + 4096.0 * 4096.0) / 2.0 * 1.2
    if r <= 0.0 or r >= 30.0:
        r = min(max(r, 1.0), 10.0)
    return r


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--gaia-dir", default=os.path.join(REPO, "gaia", "GaiaDR3SP"))
    ap.add_argument("--only", default="", help="只跑 tag 含该子串的帧")
    a = ap.parse_args()

    frames = []
    for path in sorted(glob.glob(os.path.join(FIXTURES, "*.json"))):
        name = os.path.basename(path)
        tag = tag_of(name)
        if a.only and a.only not in tag:
            continue
        d = json.load(open(path))
        w = d["wcs"]
        r = fov_radius(w)
        print(f"[{tag}] radius={r:.6f} crval=({w['crval1']:.6f},{w['crval2']:.6f})", flush=True)
        p = subprocess.run([sys.executable, os.path.join(CODE, "run_wcs_probe.py"),
                            "--p1wcs", path, "--out", EVID, "--tag", tag,
                            "--radius", repr(r), "--gaia-dir", a.gaia_dir],
                           cwd=REPO, capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stdout[-2000:]); print(p.stderr[-2000:]); return 1
        probe = json.load(open(os.path.join(EVID, f"20-wcs-probe-{tag}.json")))
        # Astropy 端到端核对 (用修复后入帧数作为期望)
        p2 = subprocess.run([sys.executable, os.path.join(CODE, "50_astropy_crosscheck.py"),
                             "--p1wcs", path,
                             "--stars", os.path.join(EVID, f"40-gaia-stars-{tag}.csv"),
                             "--out", os.path.join(EVID, f"50-astropy-crosscheck-{tag}.json"),
                             "--cpp-in-frame", str(probe["n_in_frame_post"])],
                            cwd=REPO, capture_output=True, text=True)
        if p2.returncode != 0:
            print(p2.stdout[-2000:]); print(p2.stderr[-2000:]); return 1
        cc = json.load(open(os.path.join(EVID, f"50-astropy-crosscheck-{tag}.json")))
        frames.append({
            "tag": tag, "fixture": path, "radius_deg": r,
            "n_gaia": probe["n_gaia"],
            "post_in_frame": probe["n_in_frame_post"],
            "post_nan": probe["n_nan_post"],
            "noap_in_frame": probe["n_in_frame_noap_post"],
            "pre_in_frame": probe["n_in_frame_pre"],
            "astropy_in_frame": cc["astropy_in_frame"],
            "astropy_agrees_post": cc["in_frame_agrees"],
        })
    out = {"frames": frames}
    json.dump(out, open(os.path.join(EVID, "65-per-frame-oracle.json"), "w"),
              indent=2, ensure_ascii=False)
    print(f"\n{'frame':<32} {'n_gaia':>7} {'post':>6} {'pre':>6} {'astropy':>8} {'agree':>6}")
    for f in frames:
        print(f"{f['tag']:<32} {f['n_gaia']:>7} {f['post_in_frame']:>6} "
              f"{f['pre_in_frame']:>6} {f['astropy_in_frame']:>8} {str(f['astropy_agrees_post']):>6}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
