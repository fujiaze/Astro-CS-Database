#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""判据 3（SCI-C 加性天光与无接缝叠加）在 M42 真实数据端到端产物上的复核。

规范依据
--------
- 创新点三：ASTROCS_DESIGN.md:139-145（信号校准到测光星等坐标系后帧间连续；接缝来自天光；
  天光可加性去除；UPM 联合建立连续绝对天光参考平面；多退少补；**保留背景**）。
- 纯加性冻结模型：docs/science/PHASE2_UPM.md:9-10 / :186-190（calibrated_f(p) = raw_f(p) - C_f(p)，
  C_f 为 8x8 control cell 双线性加性场；"本期决议：纯加性"，g_k == 1）。
- 权威接缝判据本体：实验/additive-sky-seamless/code/sci_c_common.py:356-411（本单元逐字副本
  code/seam_criterion.py，并由 SELFTEST 判据做逐位比对）。
- 非退化接缝判据的"全减背景=自欺"反证：实验/additive-sky-seamless/REPORT_paper.md:65-77。

判据（全部可红）
--------------
C3-SELFTEST  本单元 seam_criterion.py 与 sci_c_common.py 的 seam_steps 在随机输入上逐位一致。
C3-G1        真实导出图上的权威接缝判据：全部边界 |excess| <= 5*sigma_null（off-locus 对照标定）。
C3-N1        负例注入：在边界处注入已知加性阶跃 A，A >= A*（检测限）时必须判红。
C3-G2        加性天光保留公共平面：马赛克电平等于各帧 corrected 样本的 ivar 加权平均
             （相对残差 <= 1e-9），且**不是**取最大/取最小/算术平均。
C3-N2        负例：把合成换成算术平均 / 取最大 / 取最小，同一判据必须判红。
C3-G3        天光保留（非全减背景）：corrected 样本电平与帧自身天光电平之比必须 > 0.3；
             负例（把公共平面整体减掉）必须判红。
"""
from __future__ import annotations

import glob
import json
import math
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m42_common as M
import seam_criterion as SC

TILE = M.TILE_SPAN
A_PIXEL_SR = 2.197925819099591e-11
BLOCK_GRID = 512           # 导出图 4096 = 8x8 个 512 块（与 vis_report tile_px 一致）
INJ_AMPS = (0.0, 1e-6, 2e-6, 5e-6, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4)


# ---------------------------------------------------------------- SELFTEST
def selftest_seam():
    """逐位比对：本单元副本 vs 权威 sci_c_common.seam_steps。"""
    try:
        sys.path.insert(0, str(M.ROOT / "实验" / "additive-sky-seamless" / "code"))
        import sci_c_common as S
    except Exception as e:                                   # pragma: no cover
        return dict(ok=False, reason="import failed: %s" % e)
    rng = M.derive_rng("c3_selftest")
    worst = 0.0
    for _ in range(4):
        img = rng.normal(100.0, 5.0, size=(512, 512))
        b = [128, 192, 256, 320, 384, 448]
        a = SC.seam_steps(img, boundaries=b)
        c = S.seam_steps(img, boundaries=b)
        for ra, rc in zip(a, c):
            for key in ("step", "excess", "rel"):
                va, vc = ra[key], rc[key]
                if np.isnan(va) and np.isnan(vc):
                    continue
                worst = max(worst, abs(float(va) - float(vc)))
    return dict(ok=bool(worst == 0.0), max_abs_diff=worst,
                reason="" if worst == 0.0 else "copy diverges from sci_c_common")


# ---------------------------------------------------------------- 导出图
def load_export():
    from astropy.io import fits
    p = M.P3 / "output_phase3.fits"
    with fits.open(str(p), memmap=True) as h:
        names = [hd.name for hd in h]
        sig = np.asarray(h[0].data, dtype=np.float64)
    return sig, names, p


def null_threshold(prof_img, grid, n=200):
    """在**非边界**位置标定 excess 的零假设分布，取双边 5 sigma 阈值。"""
    rng = M.derive_rng("c3_null")
    nx = prof_img.shape[1]
    cand = np.arange(grid // 2, nx - grid // 2, 8)
    cand = cand[(cand % grid) != 0]
    pick = rng.choice(cand, size=min(n, cand.size), replace=False)
    st = SC.seam_steps(prof_img, boundaries=sorted(int(v) for v in pick))
    exc = np.array([s["excess"] for s in st if np.isfinite(s["excess"])])
    mu = float(np.median(exc))
    sd = float(M.robust_scale(exc))
    return dict(n=int(exc.size), mu=mu, sigma=sd, threshold=mu + 5.0 * sd), exc


def run_seam_on(img, grid, tag, out):
    nx = img.shape[1]
    bx = list(range(grid, nx, grid))
    stx = SC.seam_steps(img, boundaries=bx)
    sty = SC.seam_steps_axis(img, boundaries=bx, axis=1)
    cal, exc = null_threshold(img, grid)
    ex = np.array([s["excess"] for s in stx + sty if np.isfinite(s["excess"])])
    maxabs = float(np.max(np.abs(ex - cal["mu"]))) if ex.size else float("nan")
    d = dict(tag=tag, grid=grid, n_boundaries=len(bx) * 2,
             null=cal, max_abs_excess_dev=float(np.max(np.abs(ex - cal["mu"]))) if ex.size else None,
             steps_x=stx, steps_y=sty,
             verdict_red=bool(maxabs > 5.0 * cal["sigma"]))
    out[tag] = d
    return d


def block_consistency(img, grid, nb=4, tag=""):
    """逐行块一致性：真接缝在各行块上同号同量级；天体结构不会。"""
    nx = img.shape[1]
    bx = list(range(grid, nx, grid))
    ny = img.shape[0]
    edges = np.linspace(0, ny, nb + 1).astype(int)
    per_block = []
    for a, b in zip(edges[:-1], edges[1:]):
        m = np.nanmedian(img[a:b, :], axis=0)
        per_block.append(SC.steps_on_profile(m, bx))
    res = []
    for i, xb in enumerate(bx):
        ex = np.array([pb[i]["excess"] for pb in per_block])
        fin = ex[np.isfinite(ex)]
        if fin.size == 0:
            res.append(dict(x=int(xb), n_valid=0))
            continue
        sign = np.sign(fin)
        maj = float(np.median(fin))
        agree = int(np.count_nonzero(sign == np.sign(maj)))
        res.append(dict(x=int(xb), n_valid=int(fin.size), excess_by_block=[float(v) for v in ex],
                        median=float(maj), n_same_sign=int(agree),
                        consistent=bool(agree == fin.size and fin.size >= 3)))
    return dict(tag=tag, grid=grid, n_row_blocks=nb, per_boundary=res)


def injection_limit(img, grid, xb=None, out=None):
    """在边界注入已知加性阶跃 A（真值已知），标定检测限 A*。"""
    nx = img.shape[1]
    if xb is None:
        xb = (nx // 2 // grid) * grid
    cal, _ = null_threshold(img, grid)
    thr = 5.0 * cal["sigma"]
    rows = []
    for A in INJ_AMPS:
        im2 = img.copy()
        if A:
            im2[:, xb:] += A
        st = SC.seam_steps(im2, boundaries=[xb])
        e = st[0]["excess"]
        rows.append(dict(A=float(A), excess=float(e) if np.isfinite(e) else None,
                         red=bool(np.isfinite(e) and abs(e - cal["mu"]) > thr)))
    det = next((r["A"] for r in rows if r["red"]), None)
    return dict(boundary=int(xb), null=cal, threshold=thr, rows=rows, detection_limit=det)


# ---------------------------------------------------------------- 加性合成
def corrected_index():
    co = M.read_json(M.P2 / "p2_corrected.json")
    idx = []
    for fr in co["frames"]:
        idx.append(dict(data_file=fr["data_file"],
                        tiles={int(t["tile_ipix"]): int(t["offset"]) for t in fr["tiles"]}))
    return co, idx


def recon_tiles(n_tiles=16):
    rj = M.read_json(M.P2 / "p2_rejection.json")
    it = M.read_json(M.P2 / "p2_integrated.json")
    co, cidx = corrected_index()
    allf = [("t2", f) for f in M.frame_ids("t2")] + [("t3", f) for f in M.frame_ids("t3")]
    rng = M.derive_rng("c3_tiles")
    picks = sorted(rng.choice(len(rj["tiles"]), size=min(n_tiles, len(rj["tiles"])),
                              replace=False).tolist())
    mask = np.memmap(rj["files"]["sample_mask"], dtype="<u1", mode="r")
    sig_prod = np.memmap(it["files"]["signal"], dtype="<f8", mode="r")
    rows = []
    for ti in picks:
        t = rj["tiles"][ti]
        tip = int(t["tile_ipix"])
        depth = int(t["depth"])
        slots = [int(s) for s in t["frame_slots"]]
        moff = int(t["sample_mask_offset"])
        mblk = np.asarray(mask[moff:moff + depth * TILE], dtype=np.uint8).reshape(depth, TILE)
        num = np.zeros(TILE)
        den = np.zeros(TILE)
        cnt = np.zeros(TILE, dtype=np.int32)
        vmax = np.full(TILE, -np.inf)
        vmin = np.full(TILE, np.inf)
        vs = np.zeros(TILE)
        nvalid = np.zeros(TILE, dtype=np.int32)
        meds = []
        for d, fslot in enumerate(slots):
            blk, fkey = allf[fslot]
            off = cidx[fslot]["tiles"].get(tip)
            if off is None:
                continue
            corr = np.fromfile(cidx[fslot]["data_file"], dtype="<f8", count=TILE, offset=off * 8)
            iv = M.read_tile_opt(blk, fkey, "ivar", tip)
            if iv is None:
                continue
            iv = iv.astype(np.float64).ravel()
            ok = np.isfinite(corr) & np.isfinite(iv) & (iv > 0) & (mblk[d] == 1)
            num[ok] += iv[ok] * corr[ok]
            den[ok] += iv[ok]
            vs[ok] += corr[ok]
            cnt[ok] += 1
            vmax = np.where(ok, np.maximum(vmax, corr), vmax)
            vmin = np.where(ok, np.minimum(vmin, corr), vmin)
            meds.append(float(np.median(corr[ok])) if ok.sum() else float("nan"))
        good = den > 0
        base = ti * TILE
        prod = np.asarray(sig_prod[base:base + TILE], dtype=np.float64)
        arms = dict(
            additive_ivar=np.where(good, num / np.where(good, den, 1.0), np.nan),
            arithmetic_mean=np.where(cnt > 0, vs / np.maximum(cnt, 1), np.nan),
            maximum=np.where(cnt > 0, vmax, np.nan),
            minimum=np.where(cnt > 0, vmin, np.nan))
        row = dict(tile_ipix=tip, depth=depth, n_covered=int(good.sum()))
        for nm, arr in arms.items():
            m = good & np.isfinite(prod) & np.isfinite(arr) & (prod != 0)
            if int(m.sum()) < 16:
                row[nm] = None
                continue
            rel = (arr[m] - prod[m]) / np.abs(prod[m])
            row[nm] = dict(n=int(m.sum()), med_rel=float(np.median(rel)),
                           max_abs_rel=float(np.max(np.abs(rel))),
                           rms_rel=float(np.sqrt(np.mean(rel ** 2))))
        # 天光保留：corrected 样本电平 vs 帧自身天光电平
        tab = M.read_json(M.p1_dir("t2") / "p1_snr.json")
        rows.append(row)
    return rows


def sky_retention():
    """corrected 样本电平 / 帧自身天光等效电平（stored 单位）。"""
    ph2 = M.read_json(M.P1T2 / "p1_phot.json")
    ph3 = M.read_json(M.P1T3 / "p1_phot.json")
    sn2 = {f["file"].replace("@", "_"): f["background"] for f in M.read_json(M.P1T2 / "p1_snr.json")["frames"]}
    sn3 = {f["file"].replace("@", "_"): f["background"] for f in M.read_json(M.P1T3 / "p1_snr.json")["frames"]}
    allf = [("t2", f["frame_key"], float(ph2["photscale_detail"][f["frame_key"]]["k_photo"]),
             sn2.get("calibrated_" + f["frame_key"] + ".fts")) for f in ph2["frames"]]
    allf += [("t3", f["frame_key"], float(ph3["photscale_detail"][f["frame_key"]]["k_photo"]),
              sn3.get("calibrated_" + f["frame_key"] + ".fts")) for f in ph3["frames"]]
    co, cidx = corrected_index()
    rj = M.read_json(M.P2 / "p2_rejection.json")
    rng = M.derive_rng("c3_sky_ret")
    picks = sorted(rng.choice(len(rj["tiles"]), size=6, replace=False).tolist())
    out = []
    for fslot, (blk, fkey, k, bg) in enumerate(allf):
        meds = []
        for ti in picks:
            tip = int(rj["tiles"][ti]["tile_ipix"])
            off = cidx[fslot]["tiles"].get(tip)
            if off is None:
                continue
            corr = np.fromfile(cidx[fslot]["data_file"], dtype="<f8", count=TILE, offset=off * 8)
            m = np.isfinite(corr) & (corr > 0)
            if m.sum():
                meds.append(float(np.median(corr[m])))
        if not meds or not bg:
            continue
        mc = float(np.median(meds))
        sky = k * float(bg) / A_PIXEL_SR
        out.append(dict(frame_key=fkey, med_corrected=mc, sky_equiv=sky, ratio=mc / sky))
    return out


def main():
    g = M.Gates()
    out = {"unit": "M42-REALDATA-01", "criterion": "C3 SCI-C additive sky / seamless"}

    st = selftest_seam()
    g.add("C3-SELFTEST-seam-copy", "本单元 seam_steps 与权威 sci_c_common.seam_steps 逐位一致",
          st.get("max_abs_diff"), bool(st.get("ok")),
          source="实验/additive-sky-seamless/code/sci_c_common.py:377-411",
          note=st.get("reason", ""))
    out["seam_selftest"] = st

    sig, hdunames, path = load_export()
    out["export"] = dict(path=str(path.relative_to(M.ROOT)), shape=list(sig.shape),
                         hdus=hdunames,
                         finite_fraction=float(np.isfinite(sig).mean()))
    d = run_seam_on(sig, BLOCK_GRID, "p3_export_block_grid", {})
    out["seam_p3"] = d
    g.add("C3-G1-seam-p3-export",
          "权威接缝判据在真实导出图 4096^2 的 512 块边界上：全部 |excess-mu| <= 5*sigma_null",
          d["max_abs_excess_dev"], bool(not d["verdict_red"]),
          source="实验/additive-sky-seamless/code/sci_c_common.py:377-411",
          note="边界数 %d；null sigma=%.3e；实测最大偏离 %.3e"
               % (d["n_boundaries"], d["null"]["sigma"], d["max_abs_excess_dev"]))

    bc = block_consistency(sig, BLOCK_GRID, nb=4, tag="p3_export_block_grid_cols")
    bcT = block_consistency(np.ascontiguousarray(sig.T), BLOCK_GRID, nb=4,
                            tag="p3_export_block_grid_rows")
    out["seam_block_consistency"] = dict(cols=bc, rows=bcT)
    cons = [r for r in bc["per_boundary"] if r.get("consistent")]
    consT = [r for r in bcT["per_boundary"] if r.get("consistent")]
    incon = [r for r in bc["per_boundary"] if not r.get("consistent")]
    inconT = [r for r in bcT["per_boundary"] if not r.get("consistent")]
    thr = 5.0 * d["null"]["sigma"]
    max_cons = max([abs(r["median"]) for r in cons + consT], default=0.0)
    g.add("C3-G1b-seam-block-consistency",
          "跨行块一致（=真接缝）的边界其 |excess| 必须 <= 5*sigma_null；不一致者归为天体结构",
          float(max_cons), bool(max_cons <= thr),
          source="实验/additive-sky-seamless/code/sci_c_common.py:356-374（同一判据本体）",
          level="data",
          note="列向一致 %d/7、行向一致 %d/7（最大 |excess|=%.3e）；不一致列向 %s；不一致行向 %s"
               % (len(cons), len(consT), max_cons,
                  ",".join("x=%d(%.1e)" % (r["x"], r.get("median", float('nan'))) for r in incon),
                  ",".join("y=%d(%.1e)" % (r["x"], r.get("median", float('nan'))) for r in inconT)))
    for r in d["steps_x"] + d["steps_y"]:
        pass
    # 注入测试放在**最干净的边界**（基线 |excess| 最小），避免与既有台阶混淆
    cleanest = min((r for r in bc["per_boundary"] if r.get("n_valid")),
                   key=lambda r: abs(r["median"]))["x"]
    inj = injection_limit(sig, BLOCK_GRID, xb=cleanest)
    out["seam_injection"] = inj
    g.add("C3-N1-injection-red",
          "负例：在边界注入加性阶跃 A >= 检测限 A* 时判据必须判红",
          inj["detection_limit"], bool(inj["detection_limit"] is not None),
          source="AGENTS.md §5", level="negative-control",
          note="注入边界 x=%d（基线 |excess| 最小的干净边界）；阈值 %.3e；A* = %s"
               % (inj["boundary"], inj["threshold"], inj["detection_limit"]))
    g.add("C3-N1b-injection-green-at-zero",
          "正例：A=0 时判据必须判绿（假阳性控制）",
          inj["rows"][0]["excess"], bool(not inj["rows"][0]["red"]),
          source="同上", level="positive-control")

    rows = recon_tiles(16)
    out["additive_reconstruction"] = rows
    def agg(name):
        vals = [r[name]["max_abs_rel"] for r in rows if r.get(name)]
        return dict(n_tiles=len(vals), max_abs_rel=float(np.max(vals)) if vals else None)
    a_prod = agg("additive_ivar")
    a_mean = agg("arithmetic_mean")
    a_max = agg("maximum")
    a_min = agg("minimum")
    out["additive_arms"] = dict(additive_ivar=a_prod, arithmetic_mean=a_mean,
                                maximum=a_max, minimum=a_min)
    g.add("C3-G2-additive-weighted-sum",
          "马赛克 == 逐样本 ivar 加权加性和（相对残差 <= 1e-9）",
          a_prod["max_abs_rel"], bool(a_prod["max_abs_rel"] is not None and a_prod["max_abs_rel"] <= 1e-9),
          source="docs/science/PHASE2_UPM.md:186-190 + lib/algorithms/coverage/src/integrate.cpp:75",
          note="抽样 %d 个 tile" % a_prod["n_tiles"])
    for nm, lab in (("arithmetic_mean", "算术平均"), ("maximum", "取最大"), ("minimum", "取最小")):
        v = out["additive_arms"][nm]["max_abs_rel"]
        g.add("C3-N2-%s-red" % nm,
              "负例：把加性加权和换成%s时同一判据必须判红（相对残差 > 1e-9）" % lab,
              v, bool(v is not None and v > 1e-9),
              source="AGENTS.md §5", level="negative-control")

    sr = sky_retention()
    out["sky_retention"] = sr
    ratios = np.array([r["ratio"] for r in sr])
    ok_sky = bool(ratios.size and np.all(ratios > 0.3))
    g.add("C3-G3-sky-retained",
          "corrected 样本电平 / 帧自身天光电平 > 0.3（保留公共天光平面，非全减背景）",
          float(np.min(ratios)) if ratios.size else None, ok_sky,
          source="ASTROCS_DESIGN.md:143（保留公共天光平面，多退少补）",
          note="中位比 %.4f，n=%d" % (float(np.median(ratios)) if ratios.size else float('nan'), ratios.size))
    g.add("C3-N3-full-subtraction-red",
          "负例：把公共天光平面整体减掉（全减背景）后同一判据必须判红",
          float(np.min(ratios) - 1.0) if ratios.size else None,
          bool(ratios.size and np.min(ratios) - 1.0 <= 0.3),
          source="实验/additive-sky-seamless/REPORT_paper.md:65-77", level="negative-control",
          note="注入方式：ratio -> ratio - 1（等价于把 corrected 电平减到 ~0）")

    vr = M.read_json(M.VISREPORT)
    out["vis_report"] = dict(seam_ratio=vr.get("seam_ratio"), max_seam_ratio=vr.get("max_seam_ratio"),
                             seam_detail=vr.get("seam_detail"), verdict=vr.get("verdict"),
                             findings=vr.get("findings"),
                             coverage_crosscheck=vr.get("coverage_crosscheck"),
                             note="渲染器的简化度量（seam_v/inner_v 等）；本单元用权威判据在**同一 512 块网格**上复算")
    out["gates"] = g.summary()
    p = M.json_dump(out, "c3_seam_additive.json")
    print(json.dumps(out["gates"], ensure_ascii=False, indent=1)[:7000])
    print("wrote", p)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
