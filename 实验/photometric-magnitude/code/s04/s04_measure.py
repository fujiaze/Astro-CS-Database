# -*- coding: utf-8 -*-
"""S04 STEP-2 · 旧读数复现（修订前后）+ 合规帧集逐帧测量

修订对照（`git log` 核实，只读操作）
------------------------------------
`实验/photometric-magnitude/code/step8_real_frame.py` 在 HEAD 之前有两次相关改动：

* `ceec03552`（P1-B01 订正）：σ_flat 由**自指** `calibrate()['delta_after_m']` 改为
  仓内约定常数 `SIGMA_FLAT_HF_CANONICAL = 0.0007`。⇒ 上界从 PASS 翻成 ABOVE_CEILING。
* `410f05f13`（T05 E1 订正）：真实帧的逐像素方差由 `(img+RN²)/g²` 改为
  `img/g + (RN/g)²`（前者把 ADU 图当电子期望图，散粒项低估 √g = 1.1547 倍）。
  ⇒ 拟合权重变化 ⇒ χ²<4 质量筛选的样本集变化 ⇒ n 由 157 变 178、σ_obs 由
  0.02651984 变 0.02687307。

本脚本把两个历史版本**原样取出**（`git show <sha>:<path>`，不修改工作树）放进 S04
作用域后分别执行，从而给出「修订前后」在**同一合规帧集**上的逐位读数。三轨分开：

  (A) PRE  = `ceec03552` 版脚本（`var=(img+RN²)/g²`，σ_flat=0.0007）⇒ 报告旧读数
  (B) HEAD = 工作树 HEAD 版脚本（`var=img/g+(RN/g)²`，σ_flat=0.0007）⇒ 现行读数
  (C) SELFREF = 自指 σ_flat = `delta_after_m`（P1-B01 之前的预算口径），在一手归档
      `run/FINAL-07/审核包/科研审查/evidence-P1/results/step8_real_frame.json` 同口径对照

输出：results/step8_repro_<variant>.json、results/repro_comparison.json、
      results/perstar/<frame>__<var>.npz/.csv、results/frames_summary.json
"""
from __future__ import annotations

import csv
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

PERSTAR = os.path.join(C.RESULTS, "perstar")
ARCHIVE = os.path.join(C.REPO, "run", "FINAL-07", "审核包", "科研审查",
                       "evidence-P1", "results", "step8_real_frame.json")
FIXED_TAG = "s04"                       # 跨变体比较的固定 MC tag（T04 U7）
STEP8_REL = "实验/photometric-magnitude/code/step8_real_frame.py"
PRE_SHA = "ceec03552"                   # P1-B01 订正后的版本（T05-E1 之前）
POST_SHA = "410f05f13"                  # T05-E1 订正


def git_show(rev: str, rel: str) -> str:
    r = subprocess.run(["git", "show", f"{rev}:{rel}"], cwd=C.REPO,
                       capture_output=True, text=True, check=True)
    return r.stdout


def run_step8_variant(variant: str, script_path: str):
    """运行某个版本的 step8_real_frame.main()（路径重定向到 S04 作用域）。"""
    import scia_common as sc
    scope = os.path.join(C.RESULTS, "_sc_results", variant)
    os.makedirs(scope, exist_ok=True)
    sc.RESULTS = scope
    spec = importlib.util.spec_from_file_location(f"step8_{variant}", script_path)
    mod = importlib.util.module_from_spec(spec)
    t0 = time.perf_counter()
    spec.loader.exec_module(mod)
    mod.main()
    dt = time.perf_counter() - t0
    src = os.path.join(scope, "step8_real_frame.json")
    dst = os.path.join(C.RESULTS, f"step8_repro_{variant}.json")
    shutil.copy2(src, dst)
    with open(dst, encoding="utf-8") as f:
        doc = json.load(f)
    print(f"[{variant}] {dt:.1f}s → {os.path.relpath(dst, C.REPO)}")
    return doc, dst, dt


def gate_of(doc):
    g = doc["single_frame_gate"]
    it = g.get("items", {})
    return dict(n=g["n"], n_inliers=g["n_inliers"], k_photo=g.get("k_photo"),
                sigma_obs_mag=g["sigma_obs_mag"], sigma_floor=g["sigma_floor"],
                sigma_ceiling=g["sigma_ceiling"], verdict=g["verdict"],
                gate_scope=g.get("gate_scope"),
                sigma_psfsys_inframe=it.get("sigma_psfsys_inframe"),
                sigma_flat=it.get("sigma_flat"),
                delta_after_m_diag=it.get("delta_after_m_diagnostic"),
                primary_frame=doc.get("primary_frame"),
                guided=doc.get("guided_vs_blind_real"))


def archive_gate():
    if not os.path.exists(ARCHIVE):
        return dict(exists=False)
    with open(ARCHIVE, encoding="utf-8") as f:
        a = json.load(f)["single_frame_gate"]
    return dict(exists=True, path=os.path.relpath(ARCHIVE, C.REPO),
                n=a["n"], n_inliers=a["n_inliers"], sigma_obs_mag=a["sigma_obs_mag"],
                sigma_floor=a["sigma_floor"], sigma_ceiling=a["sigma_ceiling"],
                verdict=a["verdict"], sigma_psfsys_inframe=a["items"]["sigma_psfsys_inframe"],
                sigma_flat=a["items"]["sigma_flat"],
                role=("HISTORICAL_ONLY / AGENT_REPORTED：该归档为 **P1-B01 之前**的口径"
                      "（σ_flat = 自指 delta_after_m）且为 **T05-E1 之前**的方差式，"
                      "其 σ_ceiling/verdict 不是本轮权威值，仅作旧读数对照"))


def measure_compliant(inv, var_map_mode: str):
    os.makedirs(PERSTAR, exist_ok=True)
    curves = C.default_curves()
    frames = []
    t0 = time.perf_counter()
    for k, rec in enumerate(inv["compliant_set"]):
        p = os.path.join(C.REPO, rec["location"])
        fid = os.path.basename(rec["location"]).replace(".fts", "")
        print(f"[compliant/{var_map_mode}] ({k+1}/{len(inv['compliant_set'])}) {fid}")
        m = C.measure_frame(p, curves=curves, tag=FIXED_TAG, var_map_mode=var_map_mode)
        m["frame_id"] = fid
        m["observer"] = rec["observer"]
        m["sep_from_reference_deg"] = rec["sep_from_reference_deg"]
        ps = m.pop("perstar")
        cand = m.pop("candidates")
        stem = f"{fid}__{var_map_mode}"
        np.savez_compressed(os.path.join(PERSTAR, stem + ".npz"), **ps)
        np.savez_compressed(os.path.join(PERSTAR, stem + "__cand.npz"), **cand)
        cols = [k2 for k2 in ps if k2 not in ("spectra", "wl_nm")]
        with open(os.path.join(PERSTAR, stem + ".csv"), "w", newline="",
                  encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(cols)
            for i in range(len(ps[cols[0]])):
                w.writerow([(("%.10g" % ps[c][i]) if isinstance(ps[c][i], (float, np.floating))
                             else int(ps[c][i])) for c in cols])
        m["perstar_npz"] = os.path.relpath(os.path.join(PERSTAR, stem + ".npz"), C.REPO)
        m["perstar_csv"] = os.path.relpath(os.path.join(PERSTAR, stem + ".csv"), C.REPO)
        frames.append(m)
        print(f"     n={m['gate']['n']} inlier={m['gate']['n_inliers']} "
              f"sigma_obs={m['gate']['sigma_obs_mag']!r} "
              f"ceil(0.0007)={m['gate']['sigma_ceiling']!r} {m['gate']['verdict']} "
              f"ceil(selfref={m['gate']['pre_revision']['sigma_flat_used']:.6f})="
              f"{m['gate']['pre_revision']['sigma_ceiling']!r} "
              f"{m['gate']['pre_revision']['verdict']} "
              f"ceil(flat=0)={m['gate']['sigma_flat0']['sigma_ceiling']!r} [{m['wall_time_s']:.1f}s]")
    return frames, float(time.perf_counter() - t0)


def tag_crosscheck(ref_path, step8_pre, step8_head):
    """把合规路径的参照帧改用 step8 原生 MC tag（"real"）重测，验 σ_ceiling 的 tag 依赖。

    T04 U7：`mc_sigma_obs` 的 RNG 由 tag 经 SHA256 派生 ⇒ σ_fit_robust（进而 σ_ceiling）
    不是 tag 无关的确定量。本函数把它量化为「合规路径 vs step8 原生输出」的差。
    """
    out = {}
    for mode, native in (("prerev", step8_pre), ("head", step8_head)):
        m = C.measure_frame(ref_path, curves=C.default_curves(), tag="real",
                            var_map_mode=mode)
        out[mode] = dict(
            native_tag_sigma_ceiling=float(m["gate"]["sigma_ceiling"]),
            native_tag_sigma_fit_robust=float(m["gate"]["budget"]["sigma_fit_robust"]),
            native_tag_sigma_obs=float(m["gate"]["sigma_obs_mag"]),
            step8_reported_sigma_ceiling=float(native["sigma_ceiling"]),
            step8_reported_sigma_obs=float(native["sigma_obs_mag"]),
            ceiling_bitwise_equal=bool(float(m["gate"]["sigma_ceiling"])
                                       == float(native["sigma_ceiling"])),
            obs_bitwise_equal=bool(float(m["gate"]["sigma_obs_mag"])
                                   == float(native["sigma_obs_mag"])),
            ceiling_rel_diff_vs_step8=float((m["gate"]["sigma_ceiling"]
                                             - native["sigma_ceiling"])
                                            / native["sigma_ceiling"]))
    # 固定 tag（合规路径）与原生 tag 的差 ⇒ tag 依赖幅度
    fixed = C.measure_frame(ref_path, curves=C.default_curves(), tag=FIXED_TAG,
                            var_map_mode="prerev")
    out["tag_dependence"] = dict(
        fixed_tag=FIXED_TAG, native_tag="real",
        sigma_ceiling_fixed_tag=float(fixed["gate"]["sigma_ceiling"]),
        sigma_ceiling_native_tag=out["prerev"]["native_tag_sigma_ceiling"],
        sigma_fit_robust_fixed_tag=float(fixed["gate"]["budget"]["sigma_fit_robust"]),
        sigma_fit_robust_native_tag=out["prerev"]["native_tag_sigma_fit_robust"],
        rel_diff_ceiling=float((fixed["gate"]["sigma_ceiling"]
                                - out["prerev"]["native_tag_sigma_ceiling"])
                               / out["prerev"]["native_tag_sigma_ceiling"]),
        rel_diff_sigma_fit_robust=float(
            (fixed["gate"]["budget"]["sigma_fit_robust"]
             - out["prerev"]["native_tag_sigma_fit_robust"])
            / out["prerev"]["native_tag_sigma_fit_robust"]),
        note=("σ_obs / n / n_inliers / σ_psfsys 与 step8 原生输出**逐位相同**；"
              "σ_fit_robust（⇒ σ_ceiling）随 tag 变化 ⇒ 跨变体比较必须固定同一 tag"
              "（T04 U7 的独立复现）"))
    return out


def main():
    t0 = time.perf_counter()
    inv = json.load(open(os.path.join(C.RESULTS, "frame_inventory.json"), encoding="utf-8"))
    cache = C.prepare_cone_cache()

    # ---- (A)(B) 两个历史版本原样取出 ----
    cache_scripts = os.path.join(C.CACHE, "hist_step8")
    os.makedirs(cache_scripts, exist_ok=True)
    variants = {}
    for name, rev, note in (("prerev", PRE_SHA, "P1-B01 后 / T05-E1 前"),
                            ("head", None, "工作树 HEAD（= T05-E1 后）")):
        if rev is None:
            sp = os.path.join(C.REPO, STEP8_REL)
            src = "working-tree"
        else:
            sp = os.path.join(cache_scripts, f"step8_real_frame__{rev}.py")
            with open(sp, "w", encoding="utf-8") as f:
                f.write(git_show(rev, STEP8_REL))
            src = f"git:{rev}"
        variants[name] = dict(script=sp, source=src, note=note,
                              sha256=C.sha256_file(sp))
    for name in ("prerev", "head"):
        print(f"[{name}] source={variants[name]['source']} "
              f"sha256={variants[name]['sha256'][:16]}…")
    doc_pre, path_pre, dt_pre = run_step8_variant("prerev", variants["prerev"]["script"])
    doc_head, path_head, dt_head = run_step8_variant("head", variants["head"]["script"])

    # ---- (C) 合规帧集：两套方差口径各跑一遍 ----
    frames_head, dt_ch = measure_compliant(inv, "head")
    frames_pre, dt_cp = measure_compliant(inv, "prerev")
    tcx = time.perf_counter()
    cross = tag_crosscheck(os.path.join(C.REPO, inv["reference_frame"]["location"]),
                           gate_of(doc_pre), gate_of(doc_head))
    print(f"[tag-crosscheck] {time.perf_counter()-tcx:.1f}s "
          f"tag_dep_ceiling={cross['tag_dependence']['rel_diff_ceiling']:+.5f}")

    out = dict(
        step="S04-2 reproduce old readings + compliant per-frame measurement",
        base_sha=inv["base_sha"],
        fixed_mc_tag=FIXED_TAG,
        seed=int(C.sc.SCIA_SEED),
        cone_cache=cache,
        instrument_hardcode=C.INSTRUMENT_HARDCODE,
        instrument_source=("step8_real_frame.py:110-112 硬编码；注释指向 run/RELEASE-02/"
                           "parallel/out/ptc2.json（不在帧内、不在跟踪树）⇒ 来源等级"
                           "UNVERIFIED（T04 U6）"),
        revisions=dict(
            prerev=dict(git=dict(rev=PRE_SHA, path=STEP8_REL), **variants["prerev"]),
            head=dict(git=dict(rev="HEAD", path=STEP8_REL), **variants["head"]),
            order=["ceec03552（σ_flat 自指→0.0007）", "410f05f13（var: (img+RN²)/g² → img/g+(RN/g)²）"]),
        step8_original=dict(
            pre=dict(path=os.path.relpath(path_pre, C.REPO), wall_time_s=dt_pre,
                     gate=gate_of(doc_pre)),
            head=dict(path=os.path.relpath(path_head, C.REPO), wall_time_s=dt_head,
                      gate=gate_of(doc_head)),
            selection_rule=("step8_real_frame.py:59-65 文件名子串 M42/M1/T2/Red ⇒ 违反 "
                            "AGENTS §13.1；数值可复现，但输入合格性以 compliant 轨为准")),
        archive=archive_gate(),
        compliant_run=dict(n_frames=len(frames_head),
                           frame_set=inv["compliant_frame_ids"],
                           wall_time_s_head=dt_ch, wall_time_s_prerev=dt_cp),
        tag_crosscheck=cross,
        frames_head=frames_head,
        frames_prerev=frames_pre,
    )
    p = C.jdump(out, "repro_comparison.json")

    rows = []
    for mh, mp in zip(frames_head, frames_pre):
        rows.append(dict(
            frame_id=mh["frame_id"], observer=mh["observer"], date_obs=mh["date_obs"],
            file_sha256=mh["file_sha256"], crval=mh["wcs_from_header"]["crval"],
            sep_from_reference_deg=mh["sep_from_reference_deg"],
            n_head=mh["gate"]["n"], n_inliers_head=mh["gate"]["n_inliers"],
            n_prerev=mp["gate"]["n"], n_inliers_prerev=mp["gate"]["n_inliers"],
            sigma_obs_head=mh["gate"]["sigma_obs_mag"],
            sigma_obs_head_repr=repr(mh["gate"]["sigma_obs_mag"]),
            sigma_obs_prerev=mp["gate"]["sigma_obs_mag"],
            sigma_obs_prerev_repr=repr(mp["gate"]["sigma_obs_mag"]),
            sigma_ceiling_head=mh["gate"]["sigma_ceiling"],
            verdict_head=mh["gate"]["verdict"],
            sigma_ceiling_prerev=mp["gate"]["sigma_ceiling"],
            verdict_prerev=mp["gate"]["verdict"],
            sigma_ceiling_selfref=mh["gate"]["pre_revision"]["sigma_ceiling"],
            verdict_selfref=mh["gate"]["pre_revision"]["verdict"],
            sigma_flat_used_selfref=mh["gate"]["pre_revision"]["sigma_flat_used"],
            sigma_ceiling_flat0=mh["gate"]["sigma_flat0"]["sigma_ceiling"],
            n_saturated_stamp=int(np.sum(np.load(os.path.join(
                C.REPO, mh["perstar_npz"]))["saturated"])),
        ))
    C.jdump(dict(step="S04-2 frames summary", base_sha=inv["base_sha"],
                 fixed_mc_tag=FIXED_TAG, rows=rows), "frames_summary.json")
    print(f"[done] total {time.perf_counter()-t0:.1f}s → "
          f"{os.path.relpath(p, C.REPO)}")


if __name__ == "__main__":
    main()
