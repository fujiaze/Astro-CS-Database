#!/usr/bin/env python3
"""#80 P0-A1 — 汇总证据：输入身份复核、修复前后 A/B、DEV_MODE 等价性。

只读既有 runs/ 与 arms.json，产出三份轻量 JSON 到 evidence/：
  input_identity_recheck.json / ab_no_regression.json / dev_mode_equivalence.json
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
UNIT = os.path.join(REPO, "实验/issue80-p5-a1-production-fix/evidence")
E0 = os.path.join(REPO, "实验/issue80-p5-nonspd-e0/evidence/input_index.json")
SCRATCH = "/workspace/issue80-scratch/runs_a1"

FINAL_SCIENCE_FIELDS = ("properties", "hips_root")
PATH_KEYS = ("hips_root", "phase2_audit.solvers.sky_plane.artifact",
             "phase2_audit.sources.sky_plane", "properties")


def flat(d, p=""):
    out = {}
    for k, v in d.items():
        if isinstance(v, dict):
            out.update(flat(v, p + k + "."))
        else:
            out[p + k] = v
    return out


def check_inputs() -> dict:
    idx = json.load(open(E0))
    res = {"schema": "acsd.issue80.a1.input-identity/v1",
           "e0_git_head": idx["git_head"], "frames": {}, "n_files_total": 0,
           "n_mismatched": 0}
    for name, fr in idx["frames"].items():
        bad, n = 0, 0
        for rel, rec in fr["tree"]["files"].items():
            p = os.path.join(fr["dir"], rel)
            n += 1
            if not os.path.exists(p):
                bad += 1
                continue
            if hashlib.sha256(open(p, "rb").read()).hexdigest() != rec["sha256"]:
                bad += 1
        res["frames"][name] = {"dir": fr["dir"], "n_files": n, "n_mismatched": bad}
        res["n_files_total"] += n
        res["n_mismatched"] += bad
    res["verdict"] = "ALL_FROZEN_INPUTS_UNCHANGED" if res["n_mismatched"] == 0 else "MISMATCH"
    return res


def ab_no_regression() -> dict:
    pre = {r["arm"]: r for r in json.load(open(os.path.join(UNIT, "prefix_clean_head/arms.json")))}
    post = {r["arm"]: r for r in json.load(open(os.path.join(UNIT, "postfix_release/arms.json")))}
    out = {"schema": "acsd.issue80.a1.ab-no-regression/v1", "arms": {}}
    for arm in post:
        a, b = pre.get(arm), post[arm]
        rec = {"pre_exit": a["exit"] if a else None, "post_exit": b["exit"],
               "solve_artifact_same": None, "p2_final_scientific_fields_same": None}
        if a:
            ka, kb = a["artifacts"], b["artifacts"]
            rec["solve_artifact_same"] = ka.get("p2_sky_plane.bin") == kb.get("p2_sky_plane.bin")
            pa = os.path.join(a["output_dir"], "p2_final.json")
            pb = os.path.join(b["output_dir"], "p2_final.json")
            diffs = []
            if os.path.exists(pa) and os.path.exists(pb):
                fa, fb = flat(json.load(open(pa))), flat(json.load(open(pb)))
                diffs = sorted(k for k in set(fa) | set(fb) if fa.get(k) != fb.get(k))
            rec["p2_final_diff_keys"] = diffs
            rec["p2_final_scientific_fields_same"] = all(k in PATH_KEYS for k in diffs)
            rec["pre_sky_plane_line"] = a.get("sky_plane_line")
            rec["post_sky_plane_line"] = b.get("sky_plane_line")
        out["arms"][arm] = rec
    return out


def dev_mode_equivalence() -> dict:
    sci = {r["arm"]: r for r in json.load(open(os.path.join(UNIT, "postfix_release/arms.json")))}
    dev = {r["arm"]: r for r in json.load(open(os.path.join(UNIT, "postfix_dev/arms.json")))}
    out = {"schema": "acsd.issue80.a1.dev-mode-equivalence/v1", "arms": {}}
    all_same = True
    for arm in sci:
        a, b = sci[arm], dev[arm]
        same = a["artifacts"].get("p2_sky_plane.bin") == b["artifacts"].get("p2_sky_plane.bin")
        diffs = []
        pa = os.path.join(a["output_dir"], "p2_final.json")
        pb = os.path.join(b["output_dir"], "p2_final.json")
        if os.path.exists(pa) and os.path.exists(pb):
            fa, fb = flat(json.load(open(pa))), flat(json.load(open(pb)))
            diffs = sorted(k for k in set(fa) | set(fb) if fa.get(k) != fb.get(k))
        out["arms"][arm] = {"exit_sci": a["exit"], "exit_dev": b["exit"],
                            "solve_artifact_same": same,
                            "extra_files_dev": (b["artifacts"]["_n_files_total"]
                                                - a["artifacts"]["_n_files_total"]),
                            "p2_final_diff_keys": diffs,
                            "diff_is_only_absolute_output_paths": all(k in PATH_KEYS for k in diffs),
                            "sci_sky_plane_line": a.get("sky_plane_line"),
                            "dev_sky_plane_line": b.get("sky_plane_line")}
        all_same = all_same and same and a["exit"] == b["exit"]
    out["verdict"] = "ON_OFF_SOLVE_IDENTICAL" if all_same else "DIFFERS"
    return out


def main() -> int:
    for name, fn in (("input_identity_recheck.json", check_inputs),
                     ("ab_no_regression.json", ab_no_regression),
                     ("dev_mode_equivalence.json", dev_mode_equivalence)):
        d = fn()
        p = os.path.join(UNIT, name)
        with open(p, "w") as fh:
            json.dump(d, fh, indent=1, ensure_ascii=False)
        print("wrote", p, d.get("verdict", ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
