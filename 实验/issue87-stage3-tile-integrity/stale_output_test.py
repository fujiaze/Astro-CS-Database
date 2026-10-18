#!/usr/bin/env python3
"""Issue #87 / E2：同一 output_dir 的失败 run 不得留下上一次成功产物。

场景：同一 output_dir 先跑合法成功（发布 FITS/bin/manifest complete），再跑
声明内缺失（必须 fail-closed）。修复后 properties 节点在写本 run 产物前失效同名
成功 artifact ⇒ 失败 run 结束后 output_dir 内无 output_phase3.fits / bin /
resampled.json / writer.json / verify.json，只有 p3_tile_failures.json 与
incomplete run manifest。修复前 A/B：第二次运行直接发布新的 complete FITS。

用法:
  python3 实验/issue87-stage3-tile-integrity/stale_output_test.py \
      --acsd build/linux-dev/acsd --tag fixed
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(HERE, "work")
EVID = os.path.join(HERE, "evidence", "stale")
sys.path.insert(0, HERE)
from make_fixtures import tile_center  # noqa: E402

SUCCESS_ARTIFACTS = ["output_phase3.fits", "p3_resampled.bin", "p3_resampled.json",
                     "p3_writer.json", "p3_verify.json"]


def run(acsd: str, cfg: dict, cfg_path: str, log_path: str) -> int:
    json.dump(cfg, open(cfg_path, "w", encoding="utf-8"), indent=2)
    with open(log_path, "w", encoding="utf-8") as lf:
        return subprocess.run([acsd, "export", "--json", cfg_path, "-y"],
                              stdout=lf, stderr=subprocess.STDOUT).returncode


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--acsd", required=True)
    ap.add_argument("--tag", default="fixed")
    args = ap.parse_args()
    os.makedirs(EVID, exist_ok=True)
    out_dir = os.path.join(WORK, "out_stale", args.tag)
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir, exist_ok=True)

    t4 = tile_center(0, 4)
    t5 = tile_center(0, 5)
    ok_cfg = {"schema_version": "1",
              "source": {"hips_dir": os.path.join(WORK, "fixtures", "legal_full")},
              "output_dir": out_dir,
              "center": {"ra_deg": t4[0], "dec_deg": t4[1]},
              "width_px": 64, "height_px": 64, "scale_deg_per_px": 0.01,
              "sampler": "nearest", "output_mode": "surface_brightness",
              "sub_block_px": 64}
    fail_cfg = dict(ok_cfg)
    fail_cfg["source"] = {"hips_dir": os.path.join(WORK, "fixtures", "declared_missing")}
    fail_cfg["center"] = {"ra_deg": t5[0], "dec_deg": t5[1]}

    rc_ok = run(args.acsd, ok_cfg, os.path.join(WORK, "configs", f"stale_ok_{args.tag}.json"),
                os.path.join(EVID, f"{args.tag}_ok.log"))
    after_ok = [f for f in SUCCESS_ARTIFACTS if os.path.isfile(os.path.join(out_dir, f))]
    rc_fail = run(args.acsd, fail_cfg,
                  os.path.join(WORK, "configs", f"stale_fail_{args.tag}.json"),
                  os.path.join(EVID, f"{args.tag}_fail.log"))
    files = sorted(os.listdir(out_dir))
    rec = {
        "tag": args.tag, "out_dir": out_dir,
        "success_exit": rc_ok, "success_artifacts": after_ok,
        "fail_exit": rc_fail, "artifacts_after_fail": files,
        "success_artifacts_after_fail": [f for f in SUCCESS_ARTIFACTS
                                         if os.path.isfile(os.path.join(out_dir, f))],
        "tile_failures_present": os.path.isfile(os.path.join(out_dir, "p3_tile_failures.json")),
        "tmp_leftovers": [f for f in files if ".tmp" in f],
    }
    # run manifest status
    rec["manifest_status"] = None
    for f in files:
        if f.startswith("acsd_run_") and f.endswith(".json"):
            m = json.load(open(os.path.join(out_dir, f), encoding="utf-8"))
            rec["manifest_status"] = m.get("status")
            rec["manifest_summary"] = (m.get("summary") or "")[:300]
    reasons = []
    if rc_ok != 0:
        reasons.append(f"success exit {rc_ok}")
    if rc_fail == 0:
        reasons.append("fail exit 0")
    if rec["success_artifacts_after_fail"]:
        reasons.append(f"stale success artifacts remain {rec['success_artifacts_after_fail']}")
    if not rec["tile_failures_present"]:
        reasons.append("p3_tile_failures.json missing")
    if rec["manifest_status"] != "incomplete":
        reasons.append(f"manifest status {rec['manifest_status']} != incomplete")
    if rec["tmp_leftovers"]:
        reasons.append(f"tmp leftovers {rec['tmp_leftovers']}")
    rec["verdict"] = "PASS" if not reasons else "FAIL"
    rec["verdict_reason"] = "ok" if not reasons else "; ".join(reasons)
    json.dump(rec, open(os.path.join(EVID, f"results_{args.tag}.json"), "w",
                        encoding="utf-8"), indent=2)
    print(json.dumps(rec, indent=2, ensure_ascii=False))
    print(f"[stale] {args.tag} -> {rec['verdict']}"
          + ("" if rec["verdict"] == "PASS" else f" :: {rec['verdict_reason']}"))
    return 0 if rec["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
