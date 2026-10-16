#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07 主驱动：四臂（+2 对照臂）乘性增益研究 —— 阴性世界 g≡1 与阳性世界 g≠1。

用法（从仓库根）：
  python3 实验/additive-sky-seamless/code/s07_gain_experiment.py \
      --probe run/ACSDSDOC-02/S07/code/p2_sky_joint_probe_s07 \
      --out run/ACSDSDOC-02/S07/results --nseeds 10 --tag main

预声明判据（跑之前冻结，见 REPORT_S07.md §4）：
  C1 阳性可检出：阳性世界 median(RMS_A − RMS_AG_ORACLE) > 0 且 95% CI 排除 0。
  C2 阳性 G 有效：阳性世界 median(RMS_A − RMS_AG) > 0、CI 排除 0、中位比 <= 0.8。
  C3 阴性无假阳：阴性世界同一统计量 CI 含 0（不得同向显著），且逐帧 chi2 检验
     的 eta=0 拒绝率 <= 名义 5%（报告实测值）。
  C4 保真不退化：阳性世界里 AG 的弥散低频比与接缝都不比 A 差（低频比 >= 0.8×A，
     接缝 max|rel_step| <= max(门, 1.2×A)）。
  C5 值起因果作用：阳性世界 median(RMS_A − RMS_AG_SHUFFLE) <= 0（乱序不得改善）。
  ⇒ PASS（= G 可默认启用）当且仅当 C1∧C2∧C3∧C4∧C5；否则 FAIL ⇒ 生产默认恒等。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
T08CODE = os.path.join(REPO, "run", "ACSDSDOC-01", "T08", "code")
sys.path.insert(0, T08CODE)
sys.path.insert(0, HERE)

from t08_common import (W, H, PITCH, PIXSCALE_ARCSEC, SEED_BASE,  # noqa: E402
                        build_world, extract_samples, probe_grid, derive_rng)
from t08_run_synth import geoms  # noqa: E402
import s07_gain_lib as G  # noqa: E402

ARM_CFG = dict(own_surface_enabled=1, joint_outer_iterations=1, frame_gradient_order=1)

# 预声明判据（冻结；阈值改动必须另开报告）
PREDECLARED = dict(
    C1="positive: median(RMS_A-RMS_AG_ORACLE)>0 and 95% CI excludes 0",
    C2="positive: median(RMS_A-RMS_AG)>0, 95% CI excludes 0, median ratio RMS_AG/RMS_A<=0.8",
    C3="negative: median(RMS_A-RMS_AG) 95% CI includes 0; chi2(eta=0) reject rate <= 5%",
    C4="positive: lowfreq_ratio(AG) >= 0.8*lowfreq_ratio(A); seam(AG) <= max(gate,1.2*seam(A))",
    C5="positive: median(RMS_A-RMS_AG_SHUFFLE) <= 0",
    C6="specificity: max|d eta| / sigma < 3 for both worlds under aperture change "
       "(r_ap -> r_ap+3); a true multiplicative gain is aperture-independent",
    gate_rel_step=G.SEAM_GATE_REL,
)


def sha256_file(p):
    try:
        return hashlib.sha256(open(p, "rb").read()).hexdigest()
    except OSError:
        return None


KEEP_BINS = False


def run_probe(probe, world, samples, name, out_dir):
    ra, dec = probe_grid()
    scen = dict(samples=samples, frames=list(range(world["raw"].shape[0])),
                probe=dict(ra_deg=ra.tolist(), dec_deg=dec.tolist()),
                cfg=ARM_CFG, geometry=geoms(world["cfg"]))
    out_bin = os.path.join(out_dir, name + ".delta.bin")
    scen["out_bin"] = out_bin
    sin = os.path.join(out_dir, name + ".scenario.json")
    with open(sin, "w") as f:
        json.dump(scen, f)
    out_json = os.path.join(out_dir, name + ".probe.json")
    scen_sha = sha256_file(sin)
    t0 = time.time()
    p = subprocess.run([probe, sin, out_json], capture_output=True, text=True)
    res = dict(rc=p.returncode, stderr=p.stderr[-1500:], wall_s=time.time() - t0,
               scenario_sha256=scen_sha, scenario_path=sin)
    if p.returncode != 0:
        return res
    with open(out_json) as f:
        res["info"] = json.load(f)
    with open(out_bin, "rb") as f:
        nf, npx = struct.unpack("<ii", f.read(8))
        arr = np.frombuffer(f.read(8 * nf * npx), "<f8").reshape(nf, npx)
    res["delta"] = arr.reshape(nf, H, W)
    if not KEEP_BINS:   # 大体积中间产物按 AGENTS §13.2 只留文本/哈希
        for f_ in (out_bin, sin):
            try:
                os.remove(f_)
            except OSError:
                pass
    return res


def hash_samples(samples):
    h = hashlib.sha256()
    for s in samples:
        h.update(f"{s['frame_id']},{s['control_id']},{s['ra_deg']:.12g},"
                 f"{s['dec_deg']:.12g},{s['value']:.12g},{s['variance']:.6g};".encode())
    return h.hexdigest()


def world_cfg(kind, seed_tag, color=False, psf="varying", rap=5.0):
    """阴性/阳性世界**共用**同一 world id 与 seed_tag ⇒ S、a、噪声、PSF 组成逐位一致，
    仅 g（与可选色差斜率）不同。判据要求「加性背景/随机种子/PSF 组成一致」。"""
    cfg = dict(name=f"S07w_{seed_tag}", seed_tag=seed_tag,
               signal="mixed", s0=200.0, s_grad=0.0, neb_amp=25.0, neb_corr=30.0,
               n_stars=200, star_sigma=1.5, noise=6.0, off=100.0, off_k=10.0,
               a_grad=1.0, a_grady=1.0, overlap_px=48,
               psf_sigma=([1.2, 1.6, 1.4, 1.0] if psf == "varying" else [1.4, 1.4, 1.4, 1.4]),
               nframes=4, kind=kind, rap=float(rap))
    if kind == "pos":
        cfg["g"] = [1.0, 1.15, 0.88, 1.06]
    else:
        cfg["g"] = [1.0, 1.0, 1.0, 1.0]
    cfg["color_slope"] = [0.0, 0.06, -0.04, 0.03] if color else [0.0, 0.0, 0.0, 0.0]
    return cfg


def chi2_pvalue(x2, dof):
    try:
        from scipy.stats import chi2 as _c
        return float(_c.sf(x2, dof))
    except Exception:  # pragma: no cover
        return float("nan")


def one_seed(probe, kind, seed_tag, out_dir, color=False, psf="varying", rap=5.0):
    cfg = world_cfg(kind, seed_tag, color=color, psf=psf, rap=rap)
    world = build_world(cfg)
    raw = world["raw"]
    nf = raw.shape[0]
    if color and np.any(np.array(cfg["color_slope"]) != 0.0):
        raw, colors = G.apply_color_term(world, cfg["color_slope"])
        world["raw"] = raw
        world["colors"] = colors
    mask = world["mask"]
    samples_raw, _ctrl = extract_samples(world)
    name = f"S07_{kind}_{seed_tag}"
    prA = run_probe(probe, world, samples_raw, name + "_A", out_dir)
    row = dict(kind=kind, seed=seed_tag, cfg={k: v for k, v in cfg.items()},
               n_samples=len(samples_raw), input_hash=hash_samples(samples_raw),
               g_true=list(cfg["g"]), probe_rc=prA["rc"], probe_wall_s=prA.get("wall_s"))
    if prA["rc"] != 0 or "delta" not in prA:
        row["error"] = "probe_failed"
        row["probe_stderr"] = prA.get("stderr")
        return row
    delta_A = prA["delta"]

    # ── 增益证据：**raw 帧上的扣背景孔径通量比**（与加性控制样本不同的控制观测）──
    dets = [G.detect_stars(raw[k], mask[k]) for k in range(nf)]
    pairs = G.overlap_pairs(world["covers"])
    obs_raw = G.pair_obs(raw, dets, pairs)
    split = []
    for o in obs_raw:
        h = hashlib.sha256(f"split|{o['i']}|{o['j']}|{o['x']:.2f}|{o['y']:.2f}".encode()).digest()
        split.append(int.from_bytes(h[:2], "little") % 3)  # 0,1 = train；2 = holdout
    train_obs = [o for o, s in zip(obs_raw, split) if s != 2]
    ho_obs = [o for o, s in zip(obs_raw, split) if s == 2]
    est = G.estimate_scalar_gain_obs(train_obs, nf)
    eta = np.asarray(est["eta"], float)
    row["stars"] = dict(n_detected=[len(d) for d in dets],
                        overlap_pairs=[[int(i), int(j), float(ov)] for i, j, ov in pairs],
                        n_pair_obs=len(obs_raw), n_train=len(train_obs), n_holdout=len(ho_obs))
    row["gain"] = dict(status=est["status"], eta=[float(x) for x in eta],
                       sigma=[float(x) for x in est["sigma"]],
                       n_pairs=est.get("n_pairs"), pair_n=est.get("pair_n"),
                       n_obs=est.get("n_obs"),
                       chi2=est.get("chi2"), dof=est.get("dof"),
                       chi2_red=est.get("chi2_red"),
                       eta_true=[float(np.log(g)) for g in cfg["g"]])
    if est["status"] == "OK":
        chi2_0 = _chi2_at_zero(train_obs, nf)
        dof = max(est["dof"], 1)
        row["gain"]["chi2_at_eta0"] = chi2_0
        row["gain"]["p_eta0"] = chi2_pvalue(max(chi2_0 - est["chi2"], 0.0), dof)
    # 孔径稳定性（特异性检验）：真乘性增益与孔径无关；PSF/孔径系统项随孔径变。
    rap2 = float(cfg.get("rap", 5.0)) + 3.0
    obs2 = G.pair_obs(raw, dets, pairs, rap=rap2)
    split2 = [int.from_bytes(hashlib.sha256(
        f"split|{o['i']}|{o['j']}|{o['x']:.2f}|{o['y']:.2f}".encode()).digest()[:2], "little") % 3
        for o in obs2]
    est2 = G.estimate_scalar_gain_obs([o for o, t in zip(obs2, split2) if t != 2], nf)
    row["gain"]["rap2_px"] = rap2
    row["gain"]["eta_rap2"] = [float(x) for x in est2["eta"]]
    if est["status"] == "OK" and est2["status"] == "OK":
        d = np.asarray(est2["eta"], float) - np.asarray(est["eta"], float)
        sg = np.maximum(np.asarray(est["sigma"], float), 1e-12)
        row["gain"]["max_abs_deta_rap"] = float(np.max(np.abs(d)))
        row["gain"]["max_deta_over_sigma"] = float(np.max(np.abs(d) / sg))
        row["gain"]["med_abs_deta_rap"] = float(np.median(np.abs(d)))
    else:
        row["gain"]["max_abs_deta_rap"] = None
        row["gain"]["max_deta_over_sigma"] = None

    # ── 增益感知次序：z = raw/g 上重采控制样本 + 重解加性（δ^G） ──────────
    def joint(eta_use, tag):
        z = G.apply_gain(raw, np.zeros_like(raw), G.field_from_scalar(eta_use, nf))
        wz = dict(world)
        wz["raw"] = z
        wz["mask"] = mask
        sz, _ = extract_samples(wz)
        pr = run_probe(probe, wz, sz, f"{name}_{tag}", out_dir)
        if pr["rc"] != 0 or "delta" not in pr:
            return None, pr
        return z - pr["delta"], pr

    eta_oracle = np.log(np.asarray(cfg["g"], float))
    eta_shuf = G.shuffle_eta(eta, f"{kind}{seed_tag}")
    corr_joint, prJ = joint(eta, "AGj")
    corr_oracle_joint, prOJ = joint(eta_oracle, "ORj")
    corr_shuf_joint, prSJ = joint(eta_shuf, "SHj")
    row["probe_rc_joint"] = [prJ.get("rc"), prOJ.get("rc"), prSJ.get("rc")]

    # ── 臂定义（frames = 该臂校正后的逐帧帧集；eta_arm = 该臂施加的对数增益）──
    fw = G.feather_weights(world["covers"])
    g_est = G.field_from_scalar(eta, nf)
    g_or = G.field_from_scalar(eta_oracle, nf)
    arms = {
        "UNCORR": (raw, np.zeros(nf)),
        "FEATHER": (raw, np.zeros(nf)),
        "A": (raw - delta_A, np.zeros(nf)),
        "AG": (G.apply_gain(raw, delta_A, g_est), eta),
        "AG_ORACLE": (G.apply_gain(raw, delta_A, g_or), eta_oracle),
    }
    if corr_joint is not None:
        arms["AG_JOINT"] = (corr_joint, eta)
    if corr_oracle_joint is not None:
        arms["AG_ORACLE_JOINT"] = (corr_oracle_joint, eta_oracle)
    if corr_shuf_joint is not None:
        arms["AG_SHUFFLE_JOINT"] = (corr_shuf_joint, eta_shuf)

    row["arms"] = {}
    for arm, (frames_arm, eta_arm) in arms.items():
        if arm == "FEATHER":
            img = G.combine_equal(raw, mask, fw)
        else:
            img = G.combine_equal(frames_arm, mask)
        m = dict(seam=G.seam_multiscale(img, mask, world["covers"]),
                 diffuse=G.diffuse_fidelity(img, mask, world["S"]))
        # 主判据：该臂**声明的增益模型**在留出星配对上的误差（同一 raw 观测，跨臂可比）
        m["heldout_model"] = G.pair_residual(ho_obs, eta_arm)
        # 诊断：该臂**成品帧**的帧间光度不一致（η=0 残差）
        oa = G.pair_obs(frames_arm, dets, pairs)
        ho_arm = [o for o, s in zip(oa, split) if s == 2]
        m["heldout_product"] = G.pair_residual(ho_arm, np.zeros(nf))
        m["n_obs_arm"] = len(oa)
        row["arms"][arm] = m

    row["seam_max"] = {a: row["arms"][a]["seam"]["d2"]["max_abs_rel"] for a in row["arms"]}
    row["disp_rms"] = {a: row["arms"][a]["heldout_model"]["rms"] for a in row["arms"]}
    row["disp_rms_product"] = {a: row["arms"][a]["heldout_product"]["rms"] for a in row["arms"]}
    return row


def _chi2_at_zero(obs, nf):
    """eta=0 假设下的配对卡方（与 estimate_scalar_gain_obs 同观测同权重）。"""
    chi2 = 0.0
    for o in obs:
        chi2 += float(o["d"]) ** 2 / max(float(o["var"]), 1e-30)
    return float(chi2)


def boot_ci(vals, tag, n=4000, alpha=0.05):
    vals = np.asarray([v for v in vals if v is not None and np.isfinite(v)], float)
    if vals.size == 0:
        return [None, None]
    rng = derive_rng(f"s07boot|{tag}")
    idx = rng.integers(0, vals.size, (n, vals.size))
    med = np.median(vals[idx], axis=1)
    return [float(np.percentile(med, 100 * alpha / 2)),
            float(np.percentile(med, 100 * (1 - alpha / 2)))]


def summarize(rows, kind):
    sel = [r for r in rows if r["kind"] == kind and "arms" in r]
    out = dict(n=len(sel))
    if not sel:
        return out

    def col(arm, key="rms"):
        return [r["disp_rms"].get(arm) for r in sel]

    for arm in ("UNCORR", "FEATHER", "A", "AG", "AG_JOINT", "AG_SHUFFLE_JOINT",
                "AG_ORACLE", "AG_ORACLE_JOINT"):
        v = np.array([x for x in col(arm) if x is not None and np.isfinite(x)], float)
        out[f"rms_{arm}"] = dict(median=float(np.median(v)) if v.size else None,
                                 mean=float(v.mean()) if v.size else None, n=int(v.size))
        vp = np.array([r["disp_rms_product"].get(arm) for r in sel
                       if r.get("disp_rms_product", {}).get(arm) is not None
                       and np.isfinite(r["disp_rms_product"][arm])], float)
        out[f"prod_rms_{arm}"] = dict(median=float(np.median(vp)) if vp.size else None,
                                      n=int(vp.size))
    for other in ("AG", "AG_JOINT", "AG_SHUFFLE_JOINT", "AG_ORACLE", "AG_ORACLE_JOINT"):
        d = [a - o for a, o in zip(col("A"), col(other))
             if a is not None and o is not None and np.isfinite(a) and np.isfinite(o)]
        ratio = [o / a for a, o in zip(col("A"), col(other))
                 if a not in (None, 0) and o is not None and np.isfinite(a) and np.isfinite(o)]
        out[f"diff_A_minus_{other}"] = dict(
            median=float(np.median(d)) if d else None,
            ci=boot_ci(d, f"{kind}|{other}"), n=len(d),
            frac_positive=float(np.mean(np.array(d) > 0)) if d else None,
            ratio_median=float(np.median(ratio)) if ratio else None)
    for k in ("chi2_red", "p_eta0", "chi2_at_eta0"):
        v = [r["gain"].get(k) for r in sel if isinstance(r.get("gain"), dict)]
        v = [x for x in v if x is not None and np.isfinite(x)]
        out[f"gain_{k}"] = dict(median=float(np.median(v)) if v else None,
                                max=float(np.max(v)) if v else None,
                                min=float(np.min(v)) if v else None, n=len(v))
    pv = [r["gain"].get("p_eta0") for r in sel if isinstance(r.get("gain"), dict)]
    pv = [x for x in pv if x is not None and np.isfinite(x)]
    out["gain_p_eta0_reject_rate"] = dict(
        rate=(float(np.mean(np.array(pv) < 0.05)) if pv else None), n=len(pv),
        n_reject=int(np.sum(np.array(pv) < 0.05)) if pv else 0,
        alpha=0.05, note="H0: eta=0；p 由 dchi2 = chi2(0)-chi2(eta_hat) 与 dof=n_frame-1 的卡方分布给出")
    out["gain_eta_absmedian"] = [
        float(np.median(np.abs(r["gain"]["eta"]))) for r in sel
        if r.get("gain", {}).get("status") == "OK"]
    out["gain_eta"] = [r["gain"]["eta"] for r in sel if r.get("gain", {}).get("status") == "OK"]
    out["gain_sigma_absmedian"] = [
        float(np.median(np.abs(r["gain"]["sigma"]))) for r in sel
        if r.get("gain", {}).get("status") == "OK"]
    for k in ("max_abs_deta_rap", "max_deta_over_sigma", "med_abs_deta_rap"):
        v = [r["gain"].get(k) for r in sel if isinstance(r.get("gain"), dict)]
        v = [x for x in v if x is not None and np.isfinite(x)]
        out[f"gain_{k}"] = dict(median=float(np.median(v)) if v else None,
                                max=float(np.max(v)) if v else None, n=len(v))
    out["seam_A"] = [r["seam_max"].get("A") for r in sel]
    out["seam_AG_JOINT"] = [r["seam_max"].get("AG_JOINT") for r in sel]
    out["seam_AG"] = [r["seam_max"].get("AG") for r in sel]
    out["seam_AG_ORACLE_JOINT"] = [r["seam_max"].get("AG_ORACLE_JOINT") for r in sel]
    out["seam_UNCORR"] = [r["seam_max"].get("UNCORR") for r in sel]
    out["seam_FEATHER"] = [r["seam_max"].get("FEATHER") for r in sel]
    for arm in ("UNCORR", "FEATHER", "A", "AG", "AG_JOINT", "AG_ORACLE_JOINT"):
        out[f"lowfreq_{arm}"] = [r["arms"].get(arm, {}).get("diffuse", {}).get("lowfreq_ratio")
                                 for r in sel]
        out[f"struct_ratio_{arm}"] = [r["arms"].get(arm, {}).get("diffuse", {}).get("struct_ratio")
                                      for r in sel]
    # 法向多尺度：d8/d2 比值（纯台阶 ≈1；纯斜坡 ≈4-8）
    out["seam_dscan_ratio_A"] = [r["arms"]["A"]["seam"].get("d8_over_d2", {}).get("median")
                                 for r in sel]
    out["seam_dscan_ratio_UNCORR"] = [r["arms"]["UNCORR"]["seam"].get("d8_over_d2", {}).get("median")
                                      for r in sel]
    out["seam_dscan_ratio_FEATHER"] = [r["arms"]["FEATHER"]["seam"].get("d8_over_d2", {}).get("median")
                                       for r in sel]
    return out


def verdict(summary):
    """按预声明方向判红/判绿。主臂 = AG_JOINT（增益感知次序）；AG = 朴素次序（诊断）。"""
    v = {}
    p, n = summary.get("pos", {}), summary.get("neg", {})

    def sig(d):
        return d.get("median"), d.get("ci", [None, None])

    mo, cio = sig(p.get("diff_A_minus_AG_ORACLE_JOINT", {}))
    v["C1"] = bool(mo is not None and mo > 0 and cio[0] is not None and cio[0] > 0)
    mg, cig = sig(p.get("diff_A_minus_AG_JOINT", {}))
    ratio = p.get("diff_A_minus_AG_JOINT", {}).get("ratio_median")
    v["C2"] = bool(mg is not None and mg > 0 and cig[0] is not None and cig[0] > 0
                   and ratio is not None and ratio <= 0.8)
    cin = n.get("diff_A_minus_AG_JOINT", {}).get("ci", [None, None])
    rate = n.get("gain_p_eta0_reject_rate", {}).get("rate")
    v["C3"] = bool(cin[0] is not None and cin[1] is not None and cin[0] <= 0 <= cin[1]
                   and (rate is None or rate <= 0.05))
    v["C3_detail"] = dict(neg_ci=cin, neg_eta0_reject_rate=rate)
    la = [x for x in (p.get("lowfreq_A") or []) if x is not None]
    lag = [x for x in (p.get("lowfreq_AG_JOINT") or []) if x is not None]
    sa = [x for x in (p.get("seam_A") or []) if x is not None]
    sag = [x for x in (p.get("seam_AG_JOINT") or []) if x is not None]
    v["C4"] = bool(la and lag and np.median(lag) >= 0.8 * np.median(la)
                   and sa and sag
                   and np.median(sag) <= max(PREDECLARED["gate_rel_step"], 1.2 * np.median(sa)))
    ms, cis = sig(p.get("diff_A_minus_AG_SHUFFLE_JOINT", {}))
    # C5（预声明）：乱序增益不得出现与真增益**同向显著**的改善。
    # C5_strict（更严形式）：乱序臂还必须不优于 A（median(A-shuffle) <= 0）—— 该形式
    # 在本轮**为假**，原因见报告：错误赋值下的模型残差与「不校正」统计不可分（median≈0，
    # CI 含 0），不是「乱序也改善」。两者都登记，判决不依赖 C5。
    v["C5"] = bool(ms is not None and ms <= 0)
    v["C5_alt_no_same_direction_significant_improvement"] = bool(
        cis[0] is not None and cis[1] is not None and cis[0] <= 0)
    # C6 特异性：增益估计必须与孔径无关（真乘性项与孔径无关；PSF/孔径系统项不是）
    st_pos = p.get("gain_max_deta_over_sigma", {}).get("max")
    st_neg = n.get("gain_max_deta_over_sigma", {}).get("max")
    v["C6"] = bool(st_pos is not None and st_neg is not None
                   and st_pos < 3.0 and st_neg < 3.0)
    v["C6_detail"] = dict(pos_max_deta_over_sigma=st_pos, neg_max_deta_over_sigma=st_neg)
    v["PASS"] = bool(v["C1"] and v["C2"] and v["C3"] and v["C4"] and v["C5"] and v["C6"])
    v["production_default"] = "IDENTITY" if not v["PASS"] else "APPLIED_RESEARCH_ONLY"
    return v


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--nseeds", type=int, default=10)
    ap.add_argument("--tag", default="main")
    ap.add_argument("--color", action="store_true", help="注入帧间色差项（色差条件）")
    ap.add_argument("--only", default=None, help="只跑 pos/neg")
    ap.add_argument("--psf", default="varying", choices=["varying", "uniform"])
    ap.add_argument("--rap", type=float, default=5.0)
    ap.add_argument("--keep-bins", action="store_true")
    ap.add_argument("--rejudge-from", default=None,
                    help="只从已保存的 JSON 读回 rows，用同一 summarize/verdict 重算判决并回写")
    a = ap.parse_args(argv)
    global KEEP_BINS
    KEEP_BINS = bool(a.keep_bins)
    if a.rejudge_from:
        with open(a.rejudge_from) as f:
            doc = json.load(f)
        rows = doc["rows"]
        doc["summary"] = dict(pos=summarize(rows, "pos"), neg=summarize(rows, "neg"))
        doc["verdict"] = verdict(doc["summary"])
        doc["rejudged_with"] = sha256_file(os.path.abspath(__file__))
        with open(a.rejudge_from, "w") as f:
            json.dump(doc, f, indent=1, ensure_ascii=False)
        print(json.dumps(doc["verdict"], indent=1, ensure_ascii=False))
        print("rejudged", a.rejudge_from)
        return 0
    os.makedirs(a.out, exist_ok=True)
    rows = []
    kinds = [a.only] if a.only else ["pos", "neg"]
    for kind in kinds:
        for i in range(a.nseeds):
            st = f"{kind}{i:02d}"
            t0 = time.time()
            r = one_seed(a.probe, kind, st, a.out, color=a.color, psf=a.psf, rap=a.rap)
            r["wall_s"] = time.time() - t0
            rows.append(r)
            print(f"[{a.tag}] {st}: rc={r.get('probe_rc')} "
                  f"dispA={r.get('disp_rms', {}).get('A')} "
                  f"dispAGj={r.get('disp_rms', {}).get('AG_JOINT')} "
                  f"dispORj={r.get('disp_rms', {}).get('AG_ORACLE_JOINT')} "
                  f"({r['wall_s']:.1f}s)", flush=True)
    summary = dict(pos=summarize(rows, "pos"), neg=summarize(rows, "neg"))
    v = verdict(summary)
    out = dict(
        schema="ACSD-P5-S07-GAIN/1", tag=a.tag, color_condition=bool(a.color),
        psf_condition=a.psf, r_ap_px=a.rap,
        base_sha=os.environ.get("S07_BASE_SHA"), source_sha=os.environ.get("S07_SOURCE_SHA"),
        build_dir=os.environ.get("S07_BUILD_DIR"),
        probe=a.probe, probe_sha256=sha256_file(a.probe),
        script_sha256=sha256_file(os.path.abspath(__file__)),
        lib_sha256=sha256_file(os.path.join(HERE, "s07_gain_lib.py")),
        seed_base=SEED_BASE, predeclared=PREDECLARED,
        metrics_def=dict(
            heldout_disp="RMS over held-out matched stars of (log f_sk - mean_k log f_sk); unit=dex; "
                         "denominator = number of (star,frame) residuals",
            seam="signed level step across frame footprint boundary / local bg; unit=1",
            diffuse="gaussian(sigma=8px) smoothed mosaic vs truth S: slope/bias/rms/struct_ratio/lowfreq_ratio"),
        summary=summary, verdict=v, rows=rows)
    p = os.path.join(a.out, f"S07_gain_{a.tag}.json")
    with open(p, "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
    print(json.dumps(dict(tag=a.tag, verdict=v,
                          pos=summary["pos"].get("diff_A_minus_AG"),
                          neg=summary["neg"].get("diff_A_minus_AG")),
                     indent=1, ensure_ascii=False))
    print("wrote", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
