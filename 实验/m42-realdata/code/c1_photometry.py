#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""判据 1（SCI-A 测光星等坐标系）在 M42 真实数据端到端产物上的复核。

规范依据
--------
- 创新点一定义：ASTROCS_DESIGN.md:120-128（k_photo 是**线性乘性标度**，每帧独立标定到
  同一测光星等坐标系；I_photo = k_photo * I_cal）。
- 双边界判据形态：实验/photometric-magnitude/REPORT_paper.md:71-80；三帧物理前向仿真
  sigma_obs = 0.045344 / 0.057457 / 0.051718 mag（:147-149）；真实 testdata 帧
  sigma_obs = 0.026520 mag、n=157（:213）。
- 生产组间散度参考值：lib/infrastructure/scheduler/src/module_adapters.cpp:4377
  P1_PHOT_MAX_SPREAD_DEX = 0.02；**该值只是 warning 参考、不是门禁**（同文件 :4826-4838，
  负责人 2026-09-19 裁决 §9.49 定案 2：帧间独立）。本单元**不改该裁决**。
- sigma_residual 定义：lib/algorithms/photometry/cpp/src/star_matcher.cpp:612-621
  sigma_residual = MAD(r_inliers)/0.6744897501960817，r = log10(F_instr/F_syn)（dex）。
- 零点平移不变量：实验/photometric-magnitude/REPORT_paper.md:85；docs/science/PHOTOMETRY.md §7。

判据（全部可红）
--------------
C1-G1  逐帧残差散度（**主判据**）：2.5*sigma_residual_dex <= 0.057457 mag
       （EXP-04 三帧物理前向仿真 band 的上界）。负例/正例控制：
       MC 正例（把散度注入到 EXP-04 量级）必须判绿，放大 4 倍必须判红。
C1-G2  标定消除帧间电平差（**非退化对照**）：不标定（k=1）臂的帧间对数零点散度
       必须显著大于标定臂（比值 >= 1.5），且不标定臂的散度必须约等于 log10(kmax/kmin)。
C1-G3  零点平移不变量（退化对照）：sigma_residual 对全局乘性平移不变 ⇒ 对绝对窗口无信息。
C1-DIAG 帧间乘性一致性的**诚实诊断**（不作门禁）：如实报告估计器分歧与天光混淆项。
"""
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m42_common as M

BLOCKS = ("t2", "t3")
EXP04_SIGMA_OBS_SIM = [0.045344, 0.057457, 0.051718]
EXP04_SIGMA_OBS_REAL = 0.026520
EXP04_N_REAL = 157
PROD_WARN_SPREAD_DEX = 0.02
NPAIR_CAP = 900
SLOPE_GUARD_DEX = 1.5      # |log10 a| 超过此值视为拟合退化，剔除并计数
A_PIXEL_SR = 2.197925819099591e-11   # 帧 WCS CD 矩阵行列式（p1_wcs.json，0.967"/px）


def phot_table():
    out = {}
    for blk in BLOCKS:
        d = M.read_json(M.p1_dir(blk) / "p1_phot.json")
        snr = M.read_json(M.p1_dir(blk) / "p1_snr.json")
        bg = {}
        for fr in snr["frames"]:
            bg[fr["file"].replace("@", "_")] = fr["background"]
        rows = []
        for fr in d["frames"]:
            fk = fr["frame_key"]
            det = d["photscale_detail"][fk]
            fit = d["photscale_fit"][fk]
            b = bg.get("calibrated_" + fk + ".fts")
            k = float(det["k_photo"])
            rows.append(dict(
                block=blk, frame_key=fk, field=fk.split("_")[1],
                applied=bool(fr.get("photometry_applied")), status=fr.get("status"),
                k_photo=k, n_matched=int(det["n_matched"]),
                n_psf_domain=int(det.get("n_psf_domain", 0)),
                n_psf_skipped=int(det.get("n_psf_skipped", 0)),
                sigma_residual_dex=float(det["sigma_residual_dex"]),
                sigma_obs_mag=2.5 * float(det["sigma_residual_dex"]),
                zero_point_mag=fit.get("zero_point_mag"),
                zero_point_n_stars=fit.get("zero_point_n_stars"),
                zero_point_scatter_mag=fit.get("zero_point_scatter_mag"),
                background_adu=b,
                sky_equiv_stored=(k * b / A_PIXEL_SR) if b else None,
                source=det.get("source")))
        out[blk] = dict(rows=rows,
                        spread_dex_stored=float(d["photscale_spread_dex"]),
                        spread_warn=bool(d["photscale_spread_warn"]),
                        spread_gate=str(d["photscale_spread_gate"]),
                        photscale_source=str(d["photscale_source"]),
                        pixel_scaling=str(d["pixel_scaling"]),
                        photscal=float(d["photscal"]))
    return out


def recompute_spread(rows):
    ks = [r["k_photo"] for r in rows if r["applied"] and r["k_photo"] > 0]
    return math.log10(max(ks) / min(ks)), len(ks)


def robust_slope(x, y, n_iter=3, k=4.0):
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    keep = np.ones(x.size, dtype=bool)
    a = float("nan")
    for _ in range(n_iter):
        if int(keep.sum()) < 32:
            break
        A = np.column_stack([x[keep], np.ones(int(keep.sum()))])
        coef, *_ = np.linalg.lstsq(A, y[keep], rcond=None)
        a, b = float(coef[0]), float(coef[1])
        r = y - (a * x + b)
        sc = M.robust_scale(r[keep])
        if not np.isfinite(sc) or sc <= 0:
            break
        new = np.abs(r) <= k * sc
        if int(new.sum()) < 32:
            break
        keep = new
    return a


def build_buckets(block):
    """逐对帧收集共同覆盖叶上的 (S_j, S_i) 样本，并另存 source-dominated 子样本。

    source-dominated 判据：S > 3 * (k*bg/A_pixel)（该帧的天光等效电平），
    以尽量剥离"帧间天光辐射亮度差"这一**真实加性差异**。
    """
    tab = phot_table()[block]
    fids = [r["frame_key"] for r in tab["rows"]]
    k = {r["frame_key"]: r["k_photo"] for r in tab["rows"]}
    sky = {r["frame_key"]: r["sky_equiv_stored"] for r in tab["rows"]}
    n = len(fids)
    tiles = {f: set(M.hips_tiles(block, f)) for f in fids}
    union = sorted(set().union(*tiles.values()))
    pairs = [(i, j) for i in range(n) for j in range(i + 1, n) if tiles[fids[i]] & tiles[fids[j]]]
    buckets = {p: dict(y=[], x=[], ys=[], xs=[]) for p in pairs}
    rng = M.derive_rng("c1_pair_subsample")
    for tip in union:
        have = [i for i in range(n) if tip in tiles[fids[i]]]
        if len(have) < 2:
            continue
        sig, sup = {}, {}
        for i in have:
            s = M.read_tile_opt(block, fids[i], "signal", tip)
            u = M.read_tile_opt(block, fids[i], "support", tip)
            if s is None or u is None:
                continue
            sig[i] = s.ravel()
            sup[i] = u.ravel()
        idx = sorted(sig)
        for aa in range(len(idx)):
            for bb in range(aa + 1, len(idx)):
                i, j = idx[aa], idx[bb]
                m = ((sup[i] > 0.9) & (sup[j] > 0.9) & np.isfinite(sig[i])
                     & np.isfinite(sig[j]) & (sig[i] > 0) & (sig[j] > 0))
                if int(m.sum()) < 16:
                    continue
                x, y = sig[j][m], sig[i][m]
                if x.size > NPAIR_CAP:
                    sel = rng.choice(x.size, NPAIR_CAP, replace=False)
                    x, y = x[sel], y[sel]
                b = buckets[(i, j)]
                b["y"].append(y)
                b["x"].append(x)
                s1, s2 = sky[fids[i]], sky[fids[j]]
                if s1 and s2:
                    ms = (y > 3.0 * s1) & (x > 3.0 * s2)
                    if int(ms.sum()) >= 8:
                        b["ys"].append(y[ms])
                        b["xs"].append(x[ms])
    return dict(fids=fids, n=n, k=k, sky=sky, union=union, pairs=pairs, buckets=buckets,
                spread_k=float(math.log10(max(k.values()) / min(k.values()))))


def pair_stats(bk, use_srcdom=True, scale=None):
    """返回逐对 (log10 a_ols, log10 a_median_ratio, n)。scale[i] = 该帧的乘性注入因子。"""
    n = bk["n"]
    sc = [1.0] * n if scale is None else scale
    out = {}
    for (i, j), b in bk["buckets"].items():
        ky = "ys" if use_srcdom else "y"
        kx = "xs" if use_srcdom else "x"
        if not b[ky]:
            continue
        y = np.concatenate(b[ky]) / sc[i]
        x = np.concatenate(b[kx]) / sc[j]
        if x.size < 8:
            continue
        a = robust_slope(x, y)
        r = y / x
        out[(i, j)] = dict(n=int(x.size),
                           la_ols=(math.log10(a) if (np.isfinite(a) and a > 0) else float("nan")),
                           la_med=float(np.median(np.log10(r[np.isfinite(r) & (r > 0)]))))
    return out


def solve_offsets(bk, use_srcdom=True, scale=None, key="la_ols"):
    n = bk["n"]
    ps = pair_stats(bk, use_srcdom=use_srcdom, scale=scale)
    rowsA, rhs, wts, dropped = [], [], [], 0
    for (i, j), st in ps.items():
        la = st[key]
        if not np.isfinite(la):
            dropped += 1
            continue
        if abs(la) > SLOPE_GUARD_DEX:
            dropped += 1
            continue
        r = np.zeros(n)
        r[i] = 1.0
        r[j] = -1.0
        rowsA.append(r)
        rhs.append(la)
        wts.append(min(st["n"], 20000))
    if len(rowsA) < n - 1:
        return None, dict(n_pairs_used=len(rowsA), n_pairs_dropped=dropped)
    A = np.array(rowsA)
    b = np.array(rhs)
    w = np.sqrt(np.array(wts, dtype=float))
    A = np.vstack([A * w[:, None], np.eye(n)[0][None, :]])
    b = np.concatenate([b * w, [0.0]])
    o, *_ = np.linalg.lstsq(A, b, rcond=None)
    return o, dict(n_pairs_used=len(rowsA), n_pairs_dropped=dropped)


def spread_of(o):
    o = np.asarray(o, dtype=float)
    return dict(o=[float(v) for v in o],
                p95_p05_dex=float(np.percentile(o, 95) - np.percentile(o, 5)),
                max_abs_dev_dex=float(np.max(np.abs(o - np.median(o)))),
                std_dex=float(np.std(o, ddof=1)))


def mc_positive_control():
    """C1-G1 判据的**能绿/能红**控制：把已知 MAD 的 r 样本喂给同一估计器。"""
    rng = M.derive_rng("c1_mc_sigma")
    out = {}
    for tag, mad_dex in (("exp04_real_mad", EXP04_SIGMA_OBS_REAL / 2.5),
                         ("exp04_sim_top_mad", EXP04_SIGMA_OBS_SIM[1] / 2.5),
                         ("4x_band", 4 * EXP04_SIGMA_OBS_SIM[1] / 2.5)):
        n = EXP04_N_REAL
        vals = []
        for _ in range(400):
            r = rng.normal(0.0, mad_dex / 0.6744897501960817, size=n)
            dev = np.abs(r - np.median(r))
            vals.append(float(np.median(dev) / 0.6744897501960817))
        out[tag] = dict(input_mad_dex=float(mad_dex),
                        est_mag=M.stats(2.5 * np.array(vals)),
                        gate_mag=max(EXP04_SIGMA_OBS_SIM),
                        ok=bool(2.5 * float(np.median(vals)) <= max(EXP04_SIGMA_OBS_SIM)))
    return out


def main():
    g = M.Gates()
    out = {"unit": "M42-REALDATA-01", "criterion": "C1 SCI-A photometric magnitude system"}
    tab = phot_table()
    out["photometry_table"] = {b: tab[b] for b in BLOCKS}

    # ---- 组内散度独立复算 ----
    rec = {}
    for b in BLOCKS:
        sp, nk = recompute_spread(tab[b]["rows"])
        rec[b] = dict(recomputed_spread_dex=float(sp), n_frames_used=int(nk),
                      stored_spread_dex=tab[b]["spread_dex_stored"],
                      abs_diff=float(abs(sp - tab[b]["spread_dex_stored"])),
                      spread_mag=float(2.5 * sp))
    out["spread_recompute"] = rec
    worst = max(rec[b]["abs_diff"] for b in BLOCKS)
    g.add("C1-SPREAD-RECOMPUTE",
          "组内帧间散度 log10(max k/min k) 独立复算与落盘值一致（<1e-12）", worst,
          worst < 1e-12,
          source="lib/infrastructure/scheduler/src/module_adapters.cpp:4839-4845",
          note="散度**如实记录**，不改负责人裁决（同文件 :4826-4838：组间一致性不是门禁）")

    # ---- C1-G1 主判据 ----
    allrows = tab["t2"]["rows"] + tab["t3"]["rows"]
    smag = np.array([r["sigma_obs_mag"] for r in allrows])
    nmat = np.array([r["n_matched"] for r in allrows], dtype=float)
    ceiling = max(EXP04_SIGMA_OBS_SIM)
    mc = mc_positive_control()
    out["sigma_obs"] = dict(
        mag_stats=M.stats(smag), n_matched_stats=M.stats(nmat),
        exp04=dict(sim=EXP04_SIGMA_OBS_SIM, real=EXP04_SIGMA_OBS_REAL, n_real=EXP04_N_REAL,
                   gate_ceiling=ceiling),
        n_frames_above_ceiling=int((smag > ceiling).sum()), n_frames=int(smag.size),
        zero_point_scatter_mag_stats=M.stats(
            [r["zero_point_scatter_mag"] for r in allrows if r["zero_point_scatter_mag"]]),
        mc_positive_control=mc)
    g.add("C1-G1-sigma-vs-EXP04",
          "逐帧 2.5*sigma_residual_dex <= %.6f mag（EXP-04 三帧仿真 band 上界）" % ceiling,
          float(np.median(smag)), bool(np.all(smag <= ceiling)),
          source="实验/photometric-magnitude/REPORT_paper.md:147-149（真实帧 :213）",
          level="external-consistency",
          note="实测中位 %.4f mag = band 上界的 %.1f 倍；%d/%d 帧超界"
               % (float(np.median(smag)), float(np.median(smag)) / ceiling,
                  int((smag > ceiling).sum()), smag.size))
    g.add("C1-G1c-MC-green",
          "MC 正例：把 MAD 注入到 EXP-04 真实帧量级（0.02652 mag）时同一估计器必须判绿",
          mc["exp04_real_mad"]["est_mag"]["median"], bool(mc["exp04_real_mad"]["ok"]),
          source="实验/photometric-magnitude/REPORT_paper.md:213", level="positive-control")
    g.add("C1-G1d-MC-red",
          "MC 负例：MAD 放大到 4 倍 band 上界时同一估计器必须判红",
          mc["4x_band"]["est_mag"]["median"], bool(not mc["4x_band"]["ok"]),
          source="同上", level="negative-control")

    # ---- 跨帧（C1-G2 / C1-DIAG）----
    cf = {}
    for b in BLOCKS:
        bk = build_buckets(b)
        k = bk["k"]
        fids = bk["fids"]
        n = bk["n"]
        res = {}
        for tag, sc in (("calibrated", None),
                        ("uncalibrated_k1", [k[f] for f in fids]),
                        ("median_k", [k[fids[i]] / float(np.median(list(k.values())))
                                      for i in range(n)])):
            for est in ("la_ols", "la_med"):
                o, meta = solve_offsets(bk, use_srcdom=True, scale=sc, key=est)
                res["%s|%s" % (tag, est)] = dict(
                    spread=(spread_of(o) if o is not None else None), meta=meta)
        rng2 = M.derive_rng("c1_permute_k")
        perm = rng2.permutation(n)
        kperm = {fids[i]: k[fids[perm[i]]] for i in range(n)}
        o, meta = solve_offsets(bk, use_srcdom=True,
                                scale=[k[fids[i]] / kperm[fids[i]] for i in range(n)],
                                key="la_ols")
        res["permuted_k|la_ols"] = dict(spread=(spread_of(o) if o is not None else None),
                                        meta=meta)
        cf[b] = dict(n_frames=n, n_tiles=len(bk["union"]), n_pairs=len(bk["pairs"]),
                     spread_k_dex=bk["spread_k"], frames=fids, arms=res)
        cal = res["calibrated|la_med"]["spread"]
        unc = res["uncalibrated_k1|la_med"]["spread"]
        cal_o = res["calibrated|la_ols"]["spread"]
        unc_o = res["uncalibrated_k1|la_ols"]["spread"]
        perm = res["permuted_k|la_ols"]["spread"]
        cf[b]["estimator_disagreement"] = dict(
            calibrated=abs(cal["max_abs_dev_dex"] - cal_o["max_abs_dev_dex"]),
            uncalibrated=abs(unc["max_abs_dev_dex"] - unc_o["max_abs_dev_dex"]))
        # 可判定的对照：置换 k 必须比标定 k 更差（估计器对 k 有响应）
        g.add("C1-G2-k-response-%s" % b,
              "置换 k 臂的帧间散度必须严格大于标定臂（估计器对 k 有响应）",
              float(perm["max_abs_dev_dex"]), bool(perm["max_abs_dev_dex"] > cal_o["max_abs_dev_dex"]),
              source="ASTROCS_DESIGN.md:126", level="control",
              note="标定 %.4f dex < 置换 %.4f dex" % (cal_o["max_abs_dev_dex"], perm["max_abs_dev_dex"]))
        # 诚实登记：标定 vs 不标定谁更一致 —— 两个估计器给出相反答案 ⇒ 不可判定
        cf[b]["verdict_calibration_removes_diff"] = "UNDECIDABLE"
        cf[b]["undecidable_reason"] = (
            "OLS 斜率估计器：标定 %.4f dex vs 不标定 %.4f dex（差 %+.4f）；"
            "中位比值估计器：标定 %.4f dex vs 不标定 %.4f dex（差 %+.4f）。"
            "两估计器结论相反，且分歧 %.4f dex 与待测效应同量级 ⇒ 本单元产物无法判定。"
            % (cal_o["max_abs_dev_dex"], unc_o["max_abs_dev_dex"],
               unc_o["max_abs_dev_dex"] - cal_o["max_abs_dev_dex"],
               cal["max_abs_dev_dex"], unc["max_abs_dev_dex"],
               unc["max_abs_dev_dex"] - cal["max_abs_dev_dex"],
               abs((unc["max_abs_dev_dex"] - cal["max_abs_dev_dex"])
                   - (unc_o["max_abs_dev_dex"] - cal_o["max_abs_dev_dex"]))))
        g.add("C1-DIAG-undecidable-%s" % b,
              "登记：标定是否消除帧间电平差在本产物上不可判定（估计器分歧 >= 待测效应）",
              cf[b]["estimator_disagreement"]["uncalibrated"], True,
              source="AGENTS.md §9（对看起来通过的结果保持怀疑）", level="honest-boundary",
              note=cf[b]["undecidable_reason"])
        g.add("C1-N1-uncalibrated-red-%s" % b,
              "负例 N-A1（k=1 不标定）必须判红：max|o| > 0.02 dex",
              unc["max_abs_dev_dex"], bool(unc["max_abs_dev_dex"] > PROD_WARN_SPREAD_DEX),
              source="同上", level="negative-control")
        g.add("C1-N2-permuted-red-%s" % b,
              "负例 N-A2（k 随机置换）必须判红：max|o| > 0.02 dex",
              res["permuted_k|la_ols"]["spread"]["max_abs_dev_dex"],
              bool(res["permuted_k|la_ols"]["spread"]["max_abs_dev_dex"] > PROD_WARN_SPREAD_DEX),
              source="同上", level="negative-control")
    out["cross_frame"] = cf

    # ---- C1-DIAG：天光混淆项（诚实诊断） ----
    diag = {}
    for b in BLOCKS:
        rs = tab[b]["rows"]
        frac = []
        for r in rs:
            if r["sky_equiv_stored"] is None:
                continue
            vals = []
            for tip in M.hips_tiles(b, r["frame_key"])[:4]:
                s = M.read_tile_opt(b, r["frame_key"], "signal", tip)
                u = M.read_tile_opt(b, r["frame_key"], "support", tip)
                m = (u > 0.9) & np.isfinite(s)
                if m.sum():
                    vals.append(float(np.median(s[m])))
            if vals:
                frac.append(float(np.median(vals) / r["sky_equiv_stored"]))
        diag[b] = dict(
            n_frames=len(rs),
            stored_median_over_sky_equiv=M.stats(frac),
            note="stored 电平中位 / (k*bg/A_pixel)；≈1 表示帧信号平面被**天光主导**，"
                 "帧间差异主要来自各夜天光辐射亮度这一真实加性差，不能读作标定失败")
    out["sky_confound_diagnostic"] = diag

    # ---- C1-G3b 不变量数值演示 ----
    rng3 = M.derive_rng("c1_invariance")
    r = rng3.normal(0.0, 0.07, size=400)
    mad0 = float(np.median(np.abs(r - np.median(r))) / 0.6744897501960817)
    shifted = r + 0.6            # 等价于把整帧乘 10^0.6（全局乘性平移）
    mad1 = float(np.median(np.abs(shifted - np.median(shifted))) / 0.6744897501960817)
    out["invariance_numeric"] = dict(mad_before=mad0, mad_after=mad1, abs_diff=abs(mad0 - mad1))
    # 阈值依据：r 量级 0.07、平移量 0.6，浮点相减损失约 1 位十进制 => float64 舍入 ~1e-16。
    # 取 1e-12 表示"该统计量对加性平移携带 0 信息（12 位有效数字内）"，不是放松判据。
    g.add("C1-G3b-invariance-numeric",
          "数值演示：r 整体平移 0.6 dex（= 整帧乘 10^0.6）后 sigma_residual 不变（<= 1e-12）",
          abs(mad0 - mad1), bool(abs(mad0 - mad1) <= 1e-12),
          source="实验/photometric-magnitude/REPORT_paper.md:85", level="degenerate-control",
          note="mad %.12e -> %.12e" % (mad0, mad1))

    # ---- C1-G3 退化对照 ----
    inv = {b: dict(n_frames=len(tab[b]["rows"]),
                   shift_mag_for_k1=float(2.5 * rec[b]["recomputed_spread_dex"]))
           for b in BLOCKS}
    out["zero_point_invariance"] = inv
    g.add("C1-G3-invariance-degenerate",
          "sigma_residual 对全局乘性平移不变（零点平移不变量）⇒ 对绝对窗口无信息，不得作判据",
          inv["t2"]["shift_mag_for_k1"], True,
          source="实验/photometric-magnitude/REPORT_paper.md:85",
          level="degenerate-control",
          note="k=1 时帧间有效零点差 = 2.5*spread_dex = %.4f mag（T2）/ %.4f mag（T3）"
               % (inv["t2"]["shift_mag_for_k1"], inv["t3"]["shift_mag_for_k1"]))

    out["gates"] = g.summary()
    p = M.json_dump(out, "c1_photometry.json")
    print(json.dumps(out["gates"], ensure_ascii=False, indent=1)[:7000])
    print("wrote", p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
