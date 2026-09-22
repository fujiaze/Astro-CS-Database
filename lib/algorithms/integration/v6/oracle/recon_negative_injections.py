#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""recon_negative_injections.py — 重建算子判据的负例注入（能红能绿）。

对每个注入：记录源码 sha256 → 打补丁 → 重建 oracle → 跑判据 → **断言指定门判红**
→ 还原源码 → 复核 sha256 逐位一致。任何一步不符即整体失败。

注入清单（每条都对应一个在正常构建下判绿的门）:
  INJ-1 clip_removed                移除值域钳制
        ⇒ G4（病态网格 E 有界）判红：E 由 1.007 / 0.294 爆到 1e4 量级
  INJ-2 mesh_median_as_global_default  把 mesh 中值档设为全局默认
        ⇒ G6（默认目标域默认算子优于滤波档）判红
  INJ-3 cell_center_gate_removed    移除 cell 中心几何门
        ⇒ G7（角点锚定网格被拒绝）判红
  INJ-4 negative_sigma_not_intercepted  同时移除钳制与正值守卫（"负 σ 未被拦截"）
        ⇒ G5（重建场严格为正且在值域内）判红

用法:
  python3 recon_negative_injections.py --build-dir <oracle 构建目录> [--crop 256]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
_SRC = os.path.join(_HERE, "..", "src", "weight_chain.cpp")
_PARITY = os.path.join(_HERE, "recon_exp04_parity.py")
_REPO = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))

# (注入名, [(原文, 替换)], 期望判红的门名前缀, 期望判绿的门名前缀)
INJECTIONS = [
    ("INJ-1_clip_removed",
     [("  if (clips_) {\n    v = std::min(std::max(v, clip_low_), clip_high_);\n  }",
       "  if (false) {  /* INJECTED: clip removed */\n    v = std::min(std::max(v, clip_low_), clip_high_);\n  }")],
     ["G4_E_bounded_"], ["G7_corner_anchored_grid_rejected", "G7b_cell_centered_grid_accepted"]),
    ("INJ-2_mesh_median_as_global_default",
     [("    op_ = has_decl ? declared : SparseReconOperator::kNaturalBicubicSplineClip;",
       "    op_ = has_decl ? declared : SparseReconOperator::kNaturalBicubicSplineClipMeshMedian;"
       "  /* INJECTED: mesh median as global default */")],
     ["G6_default_beats_mesh_on_default_domain"],
     ["G7_corner_anchored_grid_rejected", "G1_default_spline_clip"]),
    ("INJ-3_cell_center_gate_removed",
     [("  if (!(cc_off <= tol_geom)) {", "  if (false) {  /* INJECTED: cell-center gate removed */")],
     ["G7_corner_anchored_grid_rejected"], ["G7b_cell_centered_grid_accepted"]),
    ("INJ-4_negative_sigma_not_intercepted",
     [("  if (clips_) {\n    v = std::min(std::max(v, clip_low_), clip_high_);\n  }",
       "  if (false) {  /* INJECTED: clip removed */\n    v = std::min(std::max(v, clip_low_), clip_high_);\n  }"),
      ("  if (!positive_finite(v)) {\n    set(\"reconstructed intra-frame SNR non-finite/non-positive\");",
       "  if (false) {  /* INJECTED: positivity guard removed */\n    set(\"reconstructed intra-frame SNR non-finite/non-positive\");")],
     ["G5_positive_in_range_"],
     ["G7_corner_anchored_grid_rejected", "G7b_cell_centered_grid_accepted"]),
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        h.update(fh.read())
    return h.hexdigest()


def run_parity(dump_bin, out_json, tmpdir, crop, oracle_json):
    cmd = [sys.executable, _PARITY, "--dump", dump_bin, "--out", out_json,
           "--tmpdir", tmpdir, "--crop", str(crop)]
    if oracle_json:
        cmd += ["--oracle-json", oracle_json]
    p = subprocess.run(cmd, capture_output=True, text=True)
    with open(out_json, encoding="utf-8") as fh:
        return json.load(fh), p.returncode, p.stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", required=True, help="oracle 构建目录（含 recon_dump）")
    ap.add_argument("--crop", type=int, default=256, help="注入实验的面边长（加速）")
    ap.add_argument("--out", default=os.path.join(_HERE, "recon_negative_injections.json"))
    ap.add_argument("--oracle-json", default=None, help="weight_chain_oracle.py 的参考值 JSON")
    args = ap.parse_args()

    dump_bin = os.path.join(args.build_dir, "recon_dump")
    src_orig = open(_SRC, encoding="utf-8").read()
    sha_before = sha256(_SRC)
    tmpdir = tempfile.mkdtemp(prefix="recon_inj_")

    # 基线：未注入时必须全绿（除按 crop 显式 SKIP 的门）
    base_json = os.path.join(tmpdir, "baseline.json")
    base, base_rc, base_out = run_parity(dump_bin, base_json, tmpdir, args.crop, args.oracle_json)
    print("baseline: %d passed, %d failed, %d skipped (rc=%d)"
          % (base["n_pass"], base["n_fail"], base["n_skip"], base_rc), flush=True)
    baseline_green = base["n_fail"] == 0

    results = []
    ok_all = baseline_green
    for name, patches, expect_red, expect_green in INJECTIONS:
        text = src_orig
        applied = []
        for old, new in patches:
            if text.count(old) != 1:
                raise SystemExit("注入锚点不唯一/未命中（%s）：%r 命中 %d 次"
                                 % (name, old[:60], text.count(old)))
            text = text.replace(old, new)
            applied.append(old.strip().splitlines()[0][:70])
        with open(_SRC, "w", encoding="utf-8") as fh:
            fh.write(text)
        sha_inj = sha256(_SRC)
        build = subprocess.run(["cmake", "--build", args.build_dir],
                               capture_output=True, text=True)
        if build.returncode != 0:
            shutil.copyfile(_SRC, os.path.join(tmpdir, name + "_src.cpp"))
            with open(_SRC, "w", encoding="utf-8") as fh:
                fh.write(src_orig)
            raise SystemExit("注入后构建失败（%s）：\n%s" % (name, build.stderr[-2000:]))
        out_json = os.path.join(tmpdir, name + ".json")
        res, rc, stdout = run_parity(dump_bin, out_json, tmpdir, args.crop, args.oracle_json)
        by_name = {g["gate"]: g for g in res["gates"]}
        red_ok = {}
        for pre in expect_red:
            hits = [g for n2, g in by_name.items() if n2.startswith(pre)]
            red_ok[pre] = bool(hits) and all(g["verdict"] == "FAIL" for g in hits)
        green_ok = {}
        for pre in expect_green:
            hits = [g for n2, g in by_name.items() if n2.startswith(pre)]
            green_ok[pre] = bool(hits) and all(g["verdict"] == "PASS" for g in hits)
        # 还原
        with open(_SRC, "w", encoding="utf-8") as fh:
            fh.write(src_orig)
        sha_restored = sha256(_SRC)
        restored = (sha_restored == sha_before)
        subprocess.run(["cmake", "--build", args.build_dir], capture_output=True, text=True)
        sha_rebuilt = sha256(_SRC)
        entry = {"injection": name, "patches": applied, "sha256_injected": sha_inj,
                 "expected_red": red_ok, "expected_green": green_ok,
                 "all_expected_red": all(red_ok.values()),
                 "all_expected_green": all(green_ok.values()),
                 "source_restored_bitwise": restored and sha_rebuilt == sha_before,
                 "sha256_source": sha_before,
                 "red_gate_details": {p: [g["detail"] for n2, g in by_name.items()
                                          if n2.startswith(p)] for p in expect_red},
                 "rc_under_injection": rc}
        results.append(entry)
        good = (entry["all_expected_red"] and entry["all_expected_green"]
                and entry["source_restored_bitwise"])
        ok_all = ok_all and good
        print("[%s] %s  red=%s green=%s restored=%s"
              % ("OK" if good else "BAD", name, red_ok, green_ok,
                 entry["source_restored_bitwise"]), flush=True)

    # 还原后再跑一次基线，证明"还原 ⇒ 判绿"（能红能绿闭环）
    post_json = os.path.join(tmpdir, "post_restore.json")
    post, post_rc, post_out = run_parity(dump_bin, post_json, tmpdir, args.crop, args.oracle_json)
    restored_green = (post["n_fail"] == 0 and sha256(_SRC) == sha_before)
    print("after restore: %d passed, %d failed, %d skipped (rc=%d), sha_ok=%s"
          % (post["n_pass"], post["n_fail"], post["n_skip"], post_rc,
             sha256(_SRC) == sha_before), flush=True)
    ok_all = ok_all and restored_green

    result = {"oracle": "recon_negative_injections.py",
              "crop": args.crop,
              "source_file": os.path.relpath(_SRC, _REPO),
              "sha256_source_before": sha_before,
              "sha256_source_after": sha256(_SRC),
              "baseline_all_green": baseline_green,
              "baseline": {"n_pass": base["n_pass"], "n_fail": base["n_fail"],
                           "n_skip": base["n_skip"]},
              "restored_all_green": restored_green,
              "injections": results,
              "verdict": "PASS" if ok_all else "FAIL"}
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, ensure_ascii=False)
    print("\n== negative injections: %s ==" % result["verdict"])
    print("evidence -> " + args.out)
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())