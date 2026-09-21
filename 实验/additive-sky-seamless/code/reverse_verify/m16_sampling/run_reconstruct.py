#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""M16-SAMPLING 重建驱动：仿真采样帧 -> AstroCS normalize -> mosaic -> export -> 与真值比对。

用法::

    export TMPDIR=/var/tmp/astrocs
    python3 实验/SCI-C/code/reverse_verify/m16_sampling/run_reconstruct.py \
        --dataset run/reverse_verify/m16_sampling/mosaic_diff_pointing \
        --out     run/reverse_verify/m16_sampling/recon/mosaic_diff_pointing \
        [--weight-mode 1] [--skip-generate]

产物：out/{configs,logs,norm,mosaic,export}/ + out/reconstruct.json（含全部判据与真值比对）。

判据（与真值 = 生成时用的信号面比较）：
  * flux_scale      —— 重建/真值的全局标度比（面亮度口径；应为一个常数）
  * position_px     —— 互相关亚像素峰位（应为 0）
  * structure_rms   —— 扣标度后的相对结构残差（应 << 1）
  * corr            —— 相关系数（应 -> 1）
  * phot_dmag       —— 亮星孔径测光的星等差（中位/散布）
  * 负例（同指向共模）—— 帧间空间形状差异度量必须归零（由 gen 侧 selftest 覆盖）
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[5]
sys.path.insert(0, str(ROOT / "reverse_verify" / "synthetic"))
import m16_sampling as MS  # noqa: E402

ASTROCS = ROOT / "build" / "astrocs"
TMPDIR = "/var/tmp/astrocs"


def run_cli(args: List[str], log: Path, timeout_s: int = 3600) -> Dict[str, Any]:
    log.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TMPDIR=TMPDIR)
    Path(TMPDIR).mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    with open(log, "w", encoding="utf-8") as fh:
        p = subprocess.run([str(ASTROCS)] + args, stdout=fh, stderr=subprocess.STDOUT,
                           env=env, cwd=str(ROOT), timeout=timeout_s)
    return {"cmd": " ".join([str(ASTROCS)] + args), "rc": int(p.returncode),
            "log": str(log), "elapsed_s": time.time() - t0}


# ---------------------------------------------------------------------------
# 真值 -> 导出网格（按 WCS 重采样）
# ---------------------------------------------------------------------------
def _wcs_from_header(hdr) -> Dict[str, Any]:
    cd = [[float(hdr["CD1_1"]), float(hdr["CD1_2"])], [float(hdr["CD2_1"]), float(hdr["CD2_2"])]]
    return {"CRVAL1": float(hdr["CRVAL1"]), "CRVAL2": float(hdr["CRVAL2"]),
            "CRPIX1": float(hdr["CRPIX1"]), "CRPIX2": float(hdr["CRPIX2"]),
            "CD1_1": cd[0][0], "CD1_2": cd[0][1], "CD2_1": cd[1][0], "CD2_2": cd[1][1],
            "CTYPE1": str(hdr.get("CTYPE1", "RA---TAN")),
            "CTYPE2": str(hdr.get("CTYPE2", "DEC--TAN"))}


def resample_to_grid(src: np.ndarray, src_hdr: Dict[str, Any], dst_hdr: Dict[str, Any],
                     dst_shape: Tuple[int, int], *, order: int = 1) -> np.ndarray:
    """把 src（带 src_hdr 的 TAN WCS）按 WCS 重采样到 dst 网格（解析 TAN 映射）。"""
    from scipy.ndimage import map_coordinates
    ny, nx = dst_shape
    yy, xx = np.mgrid[0:ny, 0:nx]
    s1, s2, spx, spy, scd, _ = MS._wcs_parts(src_hdr, from_meta=False)
    d1, d2, dpx, dpy, dcd, _ = MS._wcs_parts(dst_hdr, from_meta=False)
    xi = dcd[0, 0] * (xx - (dpx - 1.0)) + dcd[0, 1] * (yy - (dpy - 1.0))
    eta = dcd[1, 0] * (xx - (dpx - 1.0)) + dcd[1, 1] * (yy - (dpy - 1.0))
    ra, dec = MS.tan_deproject(xi, eta, d1, d2)
    xi2, eta2 = MS.tan_project(ra, dec, s1, s2)
    inv = np.linalg.inv(scd)
    sx = inv[0, 0] * xi2 + inv[0, 1] * eta2 + (spx - 1.0)
    sy = inv[1, 0] * xi2 + inv[1, 1] * eta2 + (spy - 1.0)
    out = map_coordinates(np.asarray(src, dtype=np.float64), [sy, sx], order=order,
                          mode="constant", cval=0.0)
    inside = (sx >= 0) & (sx <= src.shape[1] - 1) & (sy >= 0) & (sy <= src.shape[0] - 1)
    return out, inside


# ---------------------------------------------------------------------------
# 指标
# ---------------------------------------------------------------------------
def subpixel_peak_offset(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> Tuple[float, float, float]:
    """互相关亚像素峰位（a 相对 b 的平移），返回 (dy, dx, peak_corr)。"""
    aa = np.where(mask, a, 0.0)
    bb = np.where(mask, b, 0.0)
    aa = aa - aa[mask].mean()
    bb = bb - bb[mask].mean()
    fa = np.fft.rfft2(aa)
    fb = np.fft.rfft2(bb)
    cc = np.fft.irfft2(fa * np.conj(fb), s=aa.shape)
    iy, ix = np.unravel_index(int(np.argmax(cc)), cc.shape)
    ny, nx = cc.shape

    def _par(v_m, v_0, v_p):
        den = (v_m - 2.0 * v_0 + v_p)
        return 0.0 if abs(den) < 1e-30 else float(0.5 * (v_m - v_p) / den)

    ym, y0, yp = cc[(iy - 1) % ny, ix], cc[iy, ix], cc[(iy + 1) % ny, ix]
    xm, x0, xp = cc[iy, (ix - 1) % nx], cc[iy, ix], cc[iy, (ix + 1) % nx]
    dy = iy + _par(ym, y0, yp)
    dx = ix + _par(xm, x0, xp)
    if dy > ny / 2:
        dy -= ny
    if dx > nx / 2:
        dx -= nx
    peak = float(cc[iy, ix] / math.sqrt(float((aa[mask] ** 2).sum() * (bb[mask] ** 2).sum())))
    return float(dy), float(dx), peak


def star_photometry(truth: np.ndarray, rec: np.ndarray, mask: np.ndarray, *, n_stars: int = 120,
                    box: int = 9, ap_r: float = 6.0, ann: Tuple[float, float] = (10.0, 16.0)
                    ) -> Dict[str, Any]:
    """在真值上找亮且孤立的峰 -> 真值/重建双孔径测光 -> 星等差分布。"""
    from scipy.ndimage import maximum_filter
    ny, nx = truth.shape
    work = np.where(mask, truth, -np.inf)
    thr = float(np.percentile(work[mask], 99.5))
    mx = maximum_filter(work, size=box)
    peaks = (work >= mx) & mask & (work > thr)
    ys, xs = np.nonzero(peaks)
    if len(ys) == 0:
        return {"n": 0}
    order = np.argsort(work[ys, xs])[::-1]
    yy, xx = np.mgrid[0:ny, 0:nx]
    dmags: List[float] = []
    used: List[Tuple[int, int]] = []
    for k in order:
        y0, x0 = int(ys[k]), int(xs[k])
        if any((y0 - a) ** 2 + (x0 - b) ** 2 < (4 * ap_r) ** 2 for a, b in used):
            continue
        if not (20 <= y0 < ny - 20 and 20 <= x0 < nx - 20):
            continue
        used.append((y0, x0))
        rr2 = (yy - y0) ** 2 + (xx - x0) ** 2
        apm = rr2 <= ap_r ** 2
        anm = (rr2 >= ann[0] ** 2) & (rr2 <= ann[1] ** 2)
        if not apm.any() or not anm.any():
            continue
        ft = float((truth[apm] - np.median(truth[anm])).sum())
        fr = float((rec[apm] - np.median(rec[anm])).sum())
        if ft > 0 and fr > 0:
            dmags.append(-2.5 * math.log10(fr / ft))
        if len(dmags) >= n_stars:
            break
    if not dmags:
        return {"n": 0}
    d = np.array(dmags)
    return {"n": int(d.size), "dmag_median": float(np.median(d)),
            "dmag_sd": float(1.4826 * np.median(np.abs(d - np.median(d)))),
            "dmag_p16": float(np.percentile(d, 16)), "dmag_p84": float(np.percentile(d, 84)),
            "note": "dmag = -2.5log10(F_recon/F_truth)（同孔径同环天光；0 = 光度守恒）"}


def validate(dataset: Path, export_fits: Path, out: Dict[str, Any]) -> Dict[str, Any]:
    from astropy.io import fits
    with fits.open(export_fits, memmap=True) as f:
        sci = np.asarray(f[0].data, dtype=np.float64)
        ehdr = _wcs_from_header(f[0].header)
        hdus = [h.name for h in f]
    with fits.open(dataset / "truth_canvas.fits", memmap=True) as f:
        truth_src = np.asarray(f[0].data, dtype=np.float64)
        thdr = _wcs_from_header(f[0].header)
        vmask_src = np.asarray(f[1].data).astype(bool) if len(f) > 1 else np.ones_like(truth_src, bool)
    truth, inside = resample_to_grid(truth_src, thdr, ehdr, sci.shape, order=1)
    vmask, _ = resample_to_grid(vmask_src.astype(np.float64), thdr, ehdr, sci.shape, order=1)
    mask = inside & (vmask > 0.999) & np.isfinite(sci) & (sci != 0)
    if mask.sum() < 1000:
        return {"error": "too few valid overlapping pixels", "n_overlap": int(mask.sum())}
    t = truth[mask]
    s = sci[mask]
    good = t > float(np.percentile(t, 60.0))
    scale = float(np.median(s[good] / t[good])) if good.any() else float("nan")
    # 位置：互相关（在双方都减中位后）
    dy, dx, peak = subpixel_peak_offset(sci, truth, mask)
    resid = s / scale - t
    rms_rel = float(np.std(resid) / np.std(t))
    corr = float(np.corrcoef(s, t)[0, 1])
    phot = star_photometry(np.where(mask, truth, 0.0), np.where(mask, sci / scale, 0.0), mask)
    return {
        "export_hdus": hdus, "export_shape": list(sci.shape), "n_overlap_px": int(mask.sum()),
        "export_stats": {"min": float(np.nanmin(sci)), "median": float(np.median(sci)),
                         "max": float(np.nanmax(sci))},
        "truth_stats_resampled": {"median": float(np.median(t)), "max": float(np.max(t))},
        "flux_scale_recon_over_truth": scale,
        "position_offset_px": [float(dy), float(dx)], "position_offset_norm_px": float(
            math.hypot(dy, dx)),
        "xcorr_peak": peak,
        "structure_rms_rel": rms_rel, "corr": corr,
        "photometry": phot,
    }


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description="M16-SAMPLING 重建 + 真值比对")
    ap.add_argument("--dataset", required=True, help="生成器输出目录（含 frames/ masters/ truth_canvas.fits）")
    ap.add_argument("--out", required=True)
    ap.add_argument("--weight-mode", type=int, default=1)
    ap.add_argument("--timeout", type=int, default=3600)
    a = ap.parse_args()

    ds = Path(a.dataset)
    if not ds.is_absolute():
        ds = ROOT / ds
    out = Path(a.out)
    if not out.is_absolute():
        out = ROOT / out
    for sub in ("configs", "logs", "norm", "mosaic", "export"):
        (out / sub).mkdir(parents=True, exist_ok=True)
    man = json.loads((ds / "dataset.json").read_text(encoding="utf-8"))
    res: Dict[str, Any] = {"dataset": str(ds), "out": str(out), "scene_id": man["scene_id"],
                           "kind": man["kind"], "band": man["band"],
                           "weight_mode": a.weight_mode, "stages": {}}

    # ---- normalize ----
    lights = [str((ds / "frames" / ("%s.fits" % f["frame_id"])).resolve()) for f in man["frames"]]
    ncfg = {
        "schema_version": "1",
        "blocks": [{
            "name": man["scene_id"],
            "input_lights": lights,
            "master_bias": str((ds / "masters" / "master_bias.fits").resolve()),
            "master_dark": str((ds / "masters" / "master_dark.fits").resolve()),
            "master_flat": str((ds / "masters" / "master_flat.fits").resolve()),
            "output_dir": str((out / "norm").resolve()),
            "filter_passband": "",
            "dark_optimization": False,
            "master_units": {"light": "ADU", "bias": "ADU", "dark": "ADU", "flat": "normalized"},
            # flat 的 ADU 换算因子必须显式声明（U4：文件不携带单位）；本模块的 flat 就是
            # 归一化乘性响应（median~1.0），故换算因子 = 1.0
            "master_scale": {"flat": 1.0},
            "master_flat_normalize": "median",
            "drizzle": {"nested": 1, "pixfrac": 1.0, "precision_mode": 1},
            # header_pointing：逐帧从帧头 OBJCTRA/OBJCTDEC + FOCALLEN/XPIXSZ 取初始指向与
            # 板尺度，再走真实 IPV 解算链（必须给 gaia_data_dir，无 silent default）
            "wcs": {"init_source": "header_pointing",
                    "gaia_data_dir": str((ROOT / "GaiaDR3").resolve())},
        }],
    }
    (out / "configs" / "normalize.json").write_text(
        json.dumps(ncfg, indent=1, ensure_ascii=False), encoding="utf-8")
    res["stages"]["normalize"] = run_cli(["normalize", "--json",
                                          str(out / "configs" / "normalize.json"), "-y"],
                                         out / "logs" / "normalize.log", a.timeout)
    # 产物布局：norm/<frame_key>/{signal,support[,variance]}/；HiPS 根 = 含 signal/properties 的目录
    hips = sorted(str(p) for p in (out / "norm").glob("*") if p.is_dir()
                  and ((p / "signal" / "properties").exists() or (p / "properties").exists()))
    res["n_hips_products"] = len(hips)
    if not hips:
        res["error"] = "normalize produced no HiPS products"
        (out / "reconstruct.json").write_text(json.dumps(res, indent=1, ensure_ascii=False),
                                              encoding="utf-8")
        return 1

    # ---- mosaic ----
    mcfg = {"schema_version": "1", "hips_paths": hips,
            "output_dir": str((out / "mosaic").resolve()),
            "weight_mode": int(a.weight_mode), "algorithm_rejection_method": ""}
    (out / "configs" / "mosaic.json").write_text(json.dumps(mcfg, indent=1, ensure_ascii=False),
                                                 encoding="utf-8")
    res["stages"]["mosaic"] = run_cli(["mosaic", "--json", str(out / "configs" / "mosaic.json"),
                                       "-y"], out / "logs" / "mosaic.log", a.timeout)

    # ---- export（几何由真值画布的 WCS 推出：保证覆盖，且尺度 = 画布尺度）----
    from astropy.io import fits as _fits
    with _fits.open(ds / "truth_canvas.fits", memmap=True) as f:
        th = f[0].header
        tshape = (int(f[0].data.shape[0]), int(f[0].data.shape[1]))
    thdr = _wcs_from_header(th)
    cw = MS.canvas_wcs({"wcs": {"crval1": thdr["CRVAL1"], "crval2": thdr["CRVAL2"],
                                "crpix1": thdr["CRPIX1"], "crpix2": thdr["CRPIX2"],
                                "cd": [[thdr["CD1_1"], thdr["CD1_2"]],
                                       [thdr["CD2_1"], thdr["CD2_2"]]],
                                "ctype1": thdr["CTYPE1"], "ctype2": thdr["CTYPE2"],
                                "scale_arcsec_per_px": 0.04}})
    corners = np.array([[0, 0], [tshape[1] - 1, 0], [0, tshape[0] - 1],
                        [tshape[1] - 1, tshape[0] - 1]], dtype=float)
    sky = cw.all_pix2world(corners, 0)
    ra_c = float(np.mean(sky[:, 0]))
    dec_c = float(np.mean(sky[:, 1]))
    scale_deg = 0.04 / 3600.0
    cosd = math.cos(math.radians(dec_c))
    wpx = int(math.ceil((sky[:, 0].max() - sky[:, 0].min()) * cosd / scale_deg)) + 8
    hpx = int(math.ceil((sky[:, 1].max() - sky[:, 1].min()) / scale_deg)) + 8
    ecfg = {"schema_version": "1", "source": {"hips_dir": str((out / "mosaic").resolve())},
            "output_dir": str((out / "export").resolve()),
            "center": {"ra_deg": ra_c, "dec_deg": dec_c},
            "width_px": wpx, "height_px": hpx, "scale_deg_per_px": scale_deg,
            "projection": "TAN", "sampler": "bilinear", "coverage_output": "mask",
            "output_mode": "surface_brightness"}
    (out / "configs" / "export.json").write_text(json.dumps(ecfg, indent=1, ensure_ascii=False),
                                                 encoding="utf-8")
    res["export_geometry"] = {"center": [ra_c, dec_c], "width_px": wpx, "height_px": hpx,
                              "scale_deg_per_px": scale_deg,
                              "truth_canvas_shape": list(tshape)}
    res["stages"]["export"] = run_cli(["export", "--json", str(out / "configs" / "export.json"),
                                       "-y"], out / "logs" / "export.log", a.timeout)
    fits_out = sorted((out / "export").rglob("*.fits"))
    res["export_fits"] = [str(p) for p in fits_out]
    if not fits_out:
        res["error"] = "export produced no FITS"
    else:
        prod = [p for p in fits_out if "coverage" not in p.name.lower()]
        res["validation"] = validate(ds, prod[0] if prod else fits_out[0], res)
    (out / "reconstruct.json").write_text(json.dumps(res, indent=1, ensure_ascii=False, default=float),
                                          encoding="utf-8")
    print(json.dumps({k: v for k, v in res.items() if k != "validation"}, indent=1,
                     ensure_ascii=False, default=float))
    if "validation" in res:
        print(json.dumps(res["validation"], indent=1, ensure_ascii=False, default=float))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
