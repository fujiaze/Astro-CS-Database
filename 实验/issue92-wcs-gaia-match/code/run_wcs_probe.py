#!/usr/bin/env python3
"""Issue #92 —— 投影探针驱动: 真实 WCS + 真实 Gaia 星, post/pre A/B。

post = 当前 main `lib/algorithms/photometry/cpp/src/wcs_transform.cpp`
pre  = #56/#57/#61 修复前 `38eaeba8b^` 的同名文件（收缩到 build/, 不改工作树）

同一份 `wcs_probe.cpp` 两遍编译, 只换被编译的 wcs_transform.cpp, 保证唯一变量。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
CODE = os.path.join(REPO, "实验/issue92-wcs-gaia-match/code")
BUILD = os.path.join(REPO, "实验/issue92-wcs-gaia-match/build")
SRC = "lib/algorithms/photometry/cpp/src/wcs_transform.cpp"
PREFIX_COMMIT = "38eaeba8b^"


def sh(cmd, **kw):
    p = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True, **kw)
    if p.returncode != 0:
        print("CMD FAILED:", " ".join(cmd))
        print(p.stdout[-3000:])
        print(p.stderr[-3000:])
        raise SystemExit(1)
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--p1wcs", required=True)
    ap.add_argument("--gaia-dir", default=os.path.join(REPO, "gaia", "GaiaDR3SP"))
    ap.add_argument("--out", required=True)
    ap.add_argument("--tag", default="frame1")
    ap.add_argument("--radius", type=float, default=0.9255)
    ap.add_argument("--mag-min", type=float, default=6.0)
    ap.add_argument("--details", type=int, default=20)
    a = ap.parse_args()
    os.makedirs(BUILD, exist_ok=True)

    params_inc = os.path.join(BUILD, "params_issue92.inc")
    consumed_json = os.path.join(a.out, f"30-wcs-consumed-{a.tag}.json")
    sh(["python3", os.path.join(CODE, "gen_params.py"), a.p1wcs,
        "--out", params_inc, "--json-out", consumed_json])

    # 真实 Gaia 星 (与生产同一查询口径)
    gaia_o = os.path.join(BUILD, "gaia_client_issue92.o")
    if not os.path.exists(gaia_o):
        sh(["gcc", "-std=c11", "-O2", "-fopenmp",
            "-I", "lib/infrastructure/gaia_xpsd_client/src",
            "-I", "lib/infrastructure/gaia_xpsd_client/include",
            "-I", "lib/include", "-c",
            "lib/infrastructure/gaia_xpsd_client/src/gaia_client.c", "-o", gaia_o])
    dump_exe = os.path.join(BUILD, "gaia_cone_dump")
    sh(["g++", "-std=c++17", "-O2", "-fopenmp",
        "-I", "lib/infrastructure/gaia_xpsd_client/src",
        "-I", "lib/infrastructure/gaia_xpsd_client/include", "-I", "lib/include",
        os.path.join(CODE, "gaia_cone_dump.cpp"), gaia_o,
        "-lz", "-lpthread", "-ldl", "-lm", "-o", dump_exe])
    msg = json.load(open(consumed_json))
    gaia_csv = os.path.join(BUILD, f"gaia_stars_{a.tag}.csv")
    log = sh([dump_exe, a.gaia_dir, repr(msg["crval1"]), repr(msg["crval2"]),
              repr(a.radius), repr(a.mag_min), gaia_csv])
    print(log.stderr.strip())
    # 入仓一份 (真实 Gaia 星坐标, 派生自 XPSD)
    import shutil
    shutil.copyfile(gaia_csv, os.path.join(a.out, f"40-gaia-stars-{a.tag}.csv"))

    # pre 源码
    pre_src = os.path.join(BUILD, "wcs_transform_prefix.cpp")
    with open(pre_src, "w") as f:
        f.write(sh(["git", "show", f"{PREFIX_COMMIT}:{SRC}"]).stdout)

    results = {}
    src_dir = os.path.join(REPO, "lib/algorithms/photometry/cpp/src")
    for tag, src in (("post", SRC), ("pre", pre_src)):
        exe = os.path.join(BUILD, f"wcs_probe_{tag}")
        sh(["g++", "-std=c++17", "-O2", "-I", CODE, "-I", BUILD, "-I", src_dir,
            os.path.join(CODE, "wcs_probe.cpp"),
            src if src.startswith("/") else os.path.join(REPO, src),
            "-lm", "-o", exe])
        run = sh([exe, gaia_csv, os.path.join(a.out, f"probe_{tag}_{a.tag}.json"),
                  str(a.details)])
        try:
            results[tag] = json.loads(run.stdout)
        except json.JSONDecodeError:
            results[tag] = {"raw": run.stdout, "stderr": run.stderr}
    out = {
        "tag": a.tag,
        "p1_wcs": a.p1wcs,
        "prefix_commit": PREFIX_COMMIT,
        "gaia_csv": gaia_csv,
        "radius_deg": a.radius,
        "mag_min": a.mag_min,
        "post": results["post"],
        "pre": results["pre"],
        "n_gaia": results["post"]["projection"]["real_apbp"]["n"],
        "n_in_frame_post": results["post"]["projection"]["real_apbp"]["n_in_frame"],
        "n_nan_post": results["post"]["projection"]["real_apbp"]["n_nan"],
        "n_in_frame_noap_post": results["post"]["projection"]["noap_iterate"]["n_in_frame"],
        "n_in_frame_pre": results["pre"]["projection"]["real_apbp"]["n_in_frame"],
    }
    json.dump(out, open(os.path.join(a.out, f"20-wcs-probe-{a.tag}.json"), "w"),
              indent=2, ensure_ascii=False)
    print(json.dumps({k: out[k] for k in
                      ("n_gaia", "n_in_frame_post", "n_nan_post",
                       "n_in_frame_noap_post", "n_in_frame_pre")}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
