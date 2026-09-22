#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""recon_exp04_parity.py — 重建算子实现与实验单元 EXP-04 的逐像素对拍 + 判据门。

存在理由（可复跑证据）：落地算子必须**就是**实验里被测量的那个算子。本脚本把
SparseSnrReconstructor（经 oracle/recon_dump 落盘）与实验单元
实验/absolute-snr/code/exp04/operators.py 的同名算子在同一控制网格上逐像素比较，
并在同一 (面, Δ) 上比较判据 E（权重效率损失），使"实现 = 被测算子"可机器复核。

判据门（每门都能被对应注入翻红，见 recon_negative_injections.py）:
  G1 默认算子 natural_bicubic_spline_clip_v1 == EXP-04 op_spline_natural_clip
  G2 高对比档 ..._mesh_median_v1 == EXP-04 op_sextractor_spline_clip
  G3 对照算子 bilinear_regular_grid_v1 == EXP-04 op_bilinear（保留不删的证明）
  G4 病态控制网格上 E 有界（值域钳制在 force；移除钳制 ⇒ E 爆到 1e4 量级 ⇒ 红）
  G5 重建场严格为正且落在有效控制值值域内（负 σ 被拦截）
  G6 默认目标域（解析可分辨）上默认算子显著优于 mesh 滤波档
     （滤波被改成全局默认 ⇒ 红）
  G7 角点锚定的控制网格被几何门拒绝（cell 中心门被移除 ⇒ 红）
  G8 定义域 = 层覆盖的 cell 并集（边界内外各测一点）

用法:
  python3 recon_exp04_parity.py --dump <recon_dump 路径> [--out <json>] [--crop 1024]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))
_EXP04 = os.path.join(_REPO, "实验", "absolute-snr", "code", "exp04")
sys.path.insert(0, os.path.dirname(_EXP04))
sys.path.insert(0, _EXP04)
import operators as O          # noqa: E402
import exp04_common as E       # noqa: E402

DEFAULT_OP = "natural_bicubic_spline_clip_v1"
MESH_OP = "natural_bicubic_spline_clip_mesh_median_v1"
BILINEAR_OP = "bilinear_regular_grid_v1"

PARITY_TOL = 1e-12


# ---------------------------------------------------------------------------
# recon_dump 调用
# ---------------------------------------------------------------------------
def _write_spec(path, op_token, nx, ny, dx, dy, origin, values, H, W, extra_points=()):
    x0 = origin + (dx - 1.0) / 2.0
    y0 = origin + (dy - 1.0) / 2.0
    lines = [
        "operator %s" % (op_token if op_token else "-"),
        "grid %d %d %.17g %.17g %.17g %.17g %.17g %.17g"
        % (nx, ny, x0, y0, dx, dy, origin, origin),
        "values " + " ".join("%.17g" % float(v) for v in np.asarray(values).ravel()),
        "field %d %d" % (H, W),
    ]
    for (px, py) in extra_points:
        lines.append("point %.17g %.17g" % (px, py))
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def run_dump(dump_bin, spec_path, out_path):
    """返回 (header dict, {(i,j): value 或 ('ERR', msg)}, [单点查询结果])。"""
    # 预处理失败时 dump 以 rc=1 退出并在 out 里写 PREPARE_ERR（判据的一部分），
    # 因此不把非零退出码当异常。
    subprocess.run([dump_bin, spec_path, out_path], check=False)
    header, cells, pts = {}, {}, []
    with open(out_path, encoding="utf-8") as fh:
        for line in fh:
            p = line.split()
            if not p:
                continue
            if p[0] in ("operator_id", "mesh_median", "clipped", "clip_low", "clip_high",
                        "n_filled", "cell_center_offset", "node_residual"):
                header[p[0]] = p[1]
                continue
            if p[0] == "PREPARE_ERR":
                header["prepare_err"] = " ".join(p[1:])
                return header, cells, pts
            if p[0] == "P" and len(p) >= 3:
                pts.append(float(p[2]) if p[2] != "ERR" else ("ERR", " ".join(p[3:])))
                continue
            if len(p) >= 3 and p[0].lstrip("-").isdigit() and p[1].lstrip("-").isdigit():
                if p[2] == "ERR":
                    cells[(int(p[0]), int(p[1]))] = ("ERR", " ".join(p[3:]))
                else:
                    cells[(int(p[0]), int(p[1]))] = float(p[2])
    return header, cells, pts


def field_from_cells(cells, H, W):
    """(array, n_err)；ERR 单元置 NaN。"""
    a = np.full((H, W), np.nan)
    n_err = 0
    for (i, j), v in cells.items():
        if i < 0 or j < 0 or i >= W or j >= H:
            continue
        if isinstance(v, tuple):
            n_err += 1
            continue
        a[j, i] = v
    return a, n_err


def cpp_field(dump_bin, tmpdir, tag, op_token, ctrl, D, H, W, origin=0.0, nx=None, ny=None):
    nx = ctrl.shape[1] if nx is None else nx
    ny = ctrl.shape[0] if ny is None else ny
    spec = os.path.join(tmpdir, "spec_%s.txt" % tag)
    out = os.path.join(tmpdir, "out_%s.txt" % tag)
    _write_spec(spec, op_token, nx, ny, float(D), float(D), origin, ctrl, H, W)
    header, cells, _pts = run_dump(dump_bin, spec, out)
    arr, n_err = field_from_cells(cells, H, W)
    return header, arr, n_err


# ---------------------------------------------------------------------------
# 判据（与实验同一实现）
# ---------------------------------------------------------------------------
def eff_loss(est, truth, mask):
    return E.eval_field(est, truth, mask)["eff_loss"]


def analytic_face(ell, s, crop=None):
    """复现实验单元 e1_analytic.build_faces 的同一构造（含同一 seed 偏移），
    使本脚本的 (面, Δ) 与 EXP-04 报告里的数字可直接对照。

    索引算术与 e1 逐字一致：i 按 s 外层、ell 内层累加（ell 列表 =
    E.ELL_SYNTH），sigma 用 seed_off=100+i，数据面用 seed_off=200+i。"""
    crop = E.CROP if crop is None else crop
    i = 0
    for ss in E.S_FIELD_SYNTH:
        for ells in E.ELL_SYNTH:
            i += 1
            if abs(ss - s) < 1e-12 and abs(ells - ell) < 1e-12:
                sigma = E.synth_sigma_face(ell, s, crop, seed_off=100 + i)
                img, truth = E.synth_data_face(sigma, seed_off=200 + i)
                return "grf_ell%.1f_s%.3f" % (ell, s), img, truth
    raise ValueError("face (ell=%r, s=%r) not in EXP-04 synthetic grid" % (ell, s))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", required=True, help="recon_dump 可执行文件路径")
    ap.add_argument("--out", default=os.path.join(_HERE, "recon_exp04_parity.json"))
    ap.add_argument("--tmpdir", default=None)
    ap.add_argument("--crop", type=int, default=1024, help="E 对照面的边长（默认 = EXP-04 CROP）")
    ap.add_argument("--oracle-json", default=None,
                    help="weight_chain_oracle.py 的 reference_values JSON（跨语言交叉核对）")
    args = ap.parse_args()

    tmpdir = args.tmpdir or os.path.join(os.path.dirname(os.path.abspath(args.out)), "parity_tmp")
    os.makedirs(tmpdir, exist_ok=True)

    gates = []

    def gate(name, ok, detail):
        gates.append({"gate": name, "verdict": "PASS" if ok else "FAIL", "detail": detail})
        print("  [%s] %s — %s" % ("PASS" if ok else "FAIL", name, detail), flush=True)
        return ok

    def skip(name, why):
        """显式登记「本配置下该门不适用」——不得当作通过，也不计入失败。"""
        gates.append({"gate": name, "verdict": "SKIP", "detail": why})
        print("  [SKIP] %s — %s" % (name, why), flush=True)

    # ------------------------------------------------------------------
    # G1-G3 逐像素对拍（同控制网格、同查询点）
    # ------------------------------------------------------------------
    rng = np.random.default_rng(E.SEED_BASE)
    nx = ny = 8
    D = 16
    H = W = 128
    ctrl = np.exp(rng.normal(0.0, 0.25, size=(ny, nx)))
    parity = {}
    for tag, op_token, py_fn in (
            ("G1_default_spline_clip", DEFAULT_OP, O.op_spline_natural_clip),
            ("G2_mesh_median_clip", MESH_OP, O.op_sextractor_spline_clip),
            ("G3_bilinear_legacy", BILINEAR_OP, O.op_bilinear)):
        header, arr, n_err = cpp_field(args.dump, tmpdir, tag, op_token, ctrl, D, H, W)
        pyv, _ = py_fn(ctrl, D, (H, W))
        d = np.abs(arr - pyv)
        rel = float(np.nanmax(d / np.maximum(np.abs(pyv), 1e-300)))
        parity[tag] = {"operator_id_declared": op_token,
                       "operator_id_effective": header.get("operator_id"),
                       "n_err": n_err,
                       "max_abs_diff": float(np.nanmax(d)),
                       "max_rel_diff": rel,
                       "mesh_median": header.get("mesh_median"),
                       "clipped": header.get("clipped"),
                       "node_residual": header.get("node_residual")}
        gate(tag, n_err == 0 and rel <= PARITY_TOL,
             "max_rel=%.3e (tol %.0e), n_err=%d, effective_op=%s"
             % (rel, PARITY_TOL, n_err, header.get("operator_id")))

    # ------------------------------------------------------------------
    # G4/G5 病态控制网格：E 有界 + 正值/值域
    # ------------------------------------------------------------------
    patho = []
    for ell, s, Dp in ((8.0, 0.30, 16), (16.0, 0.30, 16)):
        tag_p, img, truth = analytic_face(ell, s, crop=args.crop)
        ctrl_p = E.ctrl_estimated(img, Dp)
        mask = np.isfinite(truth)
        header, arr, n_err = cpp_field(
            args.dump, tmpdir, "patho_%d_%d" % (int(ell), Dp), DEFAULT_OP, ctrl_p, Dp,
            truth.shape[0], truth.shape[1])
        py_clip, _ = O.op_spline_natural_clip(ctrl_p, Dp, truth.shape)
        py_raw, _ = O.op_spline_natural(ctrl_p, Dp, truth.shape)
        e_cpp = eff_loss(arr, truth, mask & np.isfinite(arr))
        e_py_clip = eff_loss(py_clip, truth, mask)
        e_py_raw = eff_loss(py_raw, truth, mask)
        row = {"face": tag_p, "delta": Dp, "crop": args.crop,
               "n_ctrl": [int(ctrl_p.shape[1]), int(ctrl_p.shape[0])],
               "E_cpp_default": e_cpp, "E_py_clipped": e_py_clip, "E_py_unclipped": e_py_raw,
               "cpp_min": float(np.nanmin(arr)), "cpp_max": float(np.nanmax(arr)),
               "py_unclipped_min": float(np.nanmin(py_raw)),
               "ctrl_min": float(np.nanmin(ctrl_p)), "ctrl_max": float(np.nanmax(ctrl_p)),
               "n_err": n_err,
               "clip_low": header.get("clip_low"), "clip_high": header.get("clip_high"),
               "n_filled": header.get("n_filled")}
        patho.append(row)
        ok_e = (np.isfinite(e_cpp) and e_cpp <= 2.0
                and abs(e_cpp - e_py_clip) <= 1e-9 * max(1.0, e_py_clip))
        gate("G4_E_bounded_%s_D%d" % (row["face"], Dp), ok_e,
             "E_cpp=%.4g, E_py_clip=%.4g, E_py_unclipped=%.4g" % (e_cpp, e_py_clip, e_py_raw))
        ok_pos = (n_err == 0 and np.nanmin(arr) > 0.0
                  and np.nanmin(arr) >= float(header["clip_low"]) - 1e-12
                  and np.nanmax(arr) <= float(header["clip_high"]) + 1e-12)
        gate("G5_positive_in_range_%s_D%d" % (row["face"], Dp), ok_pos,
             "min=%.6g max=%.6g ctrl_range=[%s, %s] n_err=%d"
             % (np.nanmin(arr), np.nanmax(arr), header.get("clip_low"),
                header.get("clip_high"), n_err))

    # ------------------------------------------------------------------
    # G6 默认目标域（解析可分辨，ell >> Delta）：默认算子必须显著优于 mesh 滤波档
    # ------------------------------------------------------------------
    tag6, img6, truth6 = analytic_face(64.0, 0.10, crop=args.crop)
    D6 = 64
    ctrl6 = E.ctrl_estimated(img6, D6)
    mask6 = np.isfinite(truth6)
    H6, W6 = truth6.shape
    # 默认臂**不声明算子**（走默认路径）——否则测的是显式声明，而不是"默认是什么"。
    _, arr_def, err_def = cpp_field(args.dump, tmpdir, "g6_default", None, ctrl6, D6, H6, W6)
    _, arr_mesh, err_mesh = cpp_field(args.dump, tmpdir, "g6_mesh", MESH_OP, ctrl6, D6, H6, W6)
    _, arr_bil, err_bil = cpp_field(args.dump, tmpdir, "g6_bilinear", BILINEAR_OP, ctrl6, D6, H6, W6)
    e_def = eff_loss(arr_def, truth6, mask6 & np.isfinite(arr_def))
    e_mesh = eff_loss(arr_mesh, truth6, mask6 & np.isfinite(arr_mesh))
    e_bil = eff_loss(arr_bil, truth6, mask6 & np.isfinite(arr_bil))
    g6 = {"face": tag6, "delta": D6, "crop": int(H6),
          "E_default": e_def, "E_mesh_median": e_mesh, "E_bilinear_legacy": e_bil,
          "ratio_mesh_over_default": float(e_mesh / e_def) if e_def > 0 else float("inf"),
          "ratio_bilinear_over_default": float(e_bil / e_def) if e_def > 0 else float("inf")}
    gate("G6_default_beats_mesh_on_default_domain", e_def > 0 and e_mesh > 3.0 * e_def,
         "默认路径（不声明算子）E=%.4g, mesh 档 E=%.4g, ratio=%.2fx"
         % (e_def, e_mesh, g6["ratio_mesh_over_default"]))

    # ------------------------------------------------------------------
    # G7 几何门：角点锚定的控制网格必须被拒绝
    # ------------------------------------------------------------------
    nx7 = ny7 = 4
    D7 = 64
    ctrl7 = np.full((ny7, nx7), 2.5)
    spec7 = os.path.join(tmpdir, "spec_g7.txt")
    with open(spec7, "w", encoding="utf-8") as fh:
        fh.write("operator %s\n" % DEFAULT_OP)
        # 角点锚定：x0 = origin（=0）而不是 origin+(dx-1)/2 = 31.5
        fh.write("grid %d %d 0 0 %d %d 0 0\n" % (nx7, ny7, D7, D7))
        fh.write("values " + " ".join("%.17g" % v for v in ctrl7.ravel()) + "\n")
        fh.write("field 8 8\n")
    header7, _c7, _p7 = run_dump(args.dump, spec7, os.path.join(tmpdir, "out_g7.txt"))
    rejected = "prepare_err" in header7
    gate("G7_corner_anchored_grid_rejected", rejected,
         "prepare_err=%s" % header7.get("prepare_err", "<none>"))

    # 正例：同一网格改成 cell 中心锚定必须通过（证明判红来自几何相位而不是网格本身）
    header7b, arr7b, err7b = cpp_field(args.dump, tmpdir, "g7_ok", DEFAULT_OP, ctrl7, D7, 8, 8)
    gate("G7b_cell_centered_grid_accepted",
         ("prepare_err" not in header7b) and err7b == 0
         and bool(np.allclose(arr7b, 2.5, rtol=0, atol=1e-12)),
         "effective_op=%s, mean=%.17g" % (header7b.get("operator_id"), float(np.nanmean(arr7b))))

    # ------------------------------------------------------------------
    # G8 定义域 = cell 并集
    # ------------------------------------------------------------------
    nx8 = ny8 = 2
    D8 = 8
    ctrl8 = np.array([[1.0, 3.0], [2.0, 4.0]])
    verdicts = []
    for (px, py) in ((-0.4, 3.5), (15.4, 3.5), (-0.6, 3.5), (15.6, 3.5)):
        sp = os.path.join(tmpdir, "spec_g8_%s.txt" % str(px).replace("-", "m").replace(".", "p"))
        _write_spec(sp, DEFAULT_OP, nx8, ny8, D8, D8, 0.0, ctrl8, 1, 1, extra_points=[(px, py)])
        hh, cc, pp = run_dump(args.dump, sp, sp + ".out")
        got = pp[0] if pp else ("ERR", "no result")
        verdicts.append({"x": px, "y": py,
                         "value": got if not isinstance(got, tuple) else "ERR"})
    ok8 = (all(isinstance(v["value"], float) for v in verdicts[:2])
           and all(isinstance(v["value"], str) for v in verdicts[2:]))
    gate("G8_domain_is_cell_union", ok8, json.dumps(verdicts, ensure_ascii=False))

    # ------------------------------------------------------------------
    # G9 与 EXP-04 报告公开数值的直接对照（同面、同 Δ、同判据、同 seed）
    # ------------------------------------------------------------------
    report_ref = [
        # (面, Δ, 量, 报告值, 本脚本实测)
        ("grf_ell8.0_s0.300", 16, "E_py_unclipped", 1.44e4,
         patho[0]["E_py_unclipped"]),
        ("grf_ell8.0_s0.300", 16, "E_default(clipped)", 1.007, patho[0]["E_cpp_default"]),
        ("grf_ell16.0_s0.300", 16, "E_py_unclipped", 2.48e4,
         patho[1]["E_py_unclipped"]),
        ("grf_ell16.0_s0.300", 16, "E_default(clipped)", 0.2938, patho[1]["E_cpp_default"]),
        ("grf_ell64.0_s0.100", 64, "E_default", 0.0091, g6["E_default"]),
        ("grf_ell64.0_s0.100", 64, "E_mesh_median", 0.0847, g6["E_mesh_median"]),
        ("grf_ell64.0_s0.100", 64, "E_bilinear_legacy", 0.0204, g6["E_bilinear_legacy"]),
    ]
    cmp_rows = []
    for face, dpx, what, published, measured in report_ref:
        rel = abs(measured - published) / abs(published) if published else float("inf")
        cmp_rows.append({"face": face, "delta": dpx, "quantity": what,
                         "exp04_report_value": published, "measured": measured,
                         "rel_dev": rel,
                         # 报告 §2.7 的 1.007 / 0.2938 与 §2.1 的 0.0091/0.0847/0.0204
                         # 均只给 3-4 位有效数字 ⇒ 对照容差按有效位数取 5e-3。
                         "within_published_precision": bool(rel <= 5e-3)})
    n_bad = sum(1 for r in cmp_rows if not r["within_published_precision"])
    if args.crop == E.CROP:
        gate("G9_exp04_published_values_reproduced", n_bad == 0,
             "%d/%d 项在报告有效位数内一致；最大相对偏差 %.3e"
             % (len(cmp_rows) - n_bad, len(cmp_rows),
                max(r["rel_dev"] for r in cmp_rows)))
    else:
        skip("G9_exp04_published_values_reproduced",
             "报告公开值只在 EXP-04 CROP=%d 下可比；本次 crop=%d" % (E.CROP, args.crop))

    # ------------------------------------------------------------------
    # G10 与 Python 侧独立 Oracle 的参考值交叉核对（4x4 固定 fixture）
    # ------------------------------------------------------------------
    cross = None
    if args.oracle_json and os.path.isfile(args.oracle_json):
        with open(args.oracle_json, encoding="utf-8") as fh:
            ref = json.load(fh)["reference_values"]
        vals = ref["grid4x4_values"]
        ctrl4 = np.array(vals, dtype=np.float64).reshape(4, 4)
        cross = {"oracle_json": os.path.basename(args.oracle_json), "rows": []}
        ok10 = True
        for op_token, key in ((DEFAULT_OP, "grid4x4_default_queries"),
                              (MESH_OP, "grid4x4_mesh_median_queries")):
            want = ref[key]
            tag10 = "g10_%s" % ("default" if op_token == DEFAULT_OP else "mesh")
            qs = [tuple(float(t) for t in s.split(",")) for s in want]
            sp10 = os.path.join(tmpdir, "spec_%s.txt" % tag10)
            _write_spec(sp10, op_token, 4, 4, 8.0, 8.0, 0.0, ctrl4, 1, 1, extra_points=qs)
            _h10, _c10, p10 = run_dump(args.dump, sp10, sp10 + ".out")
            worst = 0.0
            n_err10 = 0
            for k, (qx, qy) in enumerate(qs):
                got = p10[k] if k < len(p10) else ("ERR", "missing")
                if isinstance(got, tuple):
                    n_err10 += 1
                    continue
                w = float(want["%.1f,%.1f" % (qx, qy)])
                worst = max(worst, abs(got - w) / max(1.0, abs(w)))
            cross["rows"].append({"operator": op_token, "n_queries": len(want),
                                  "max_rel_dev_vs_python_oracle": worst, "n_err": n_err10})
            ok10 = ok10 and n_err10 == 0 and worst <= 1e-11
        gate("G10_cross_language_python_oracle_agrees", ok10,
             json.dumps(cross["rows"], ensure_ascii=False))
    else:
        gate("G10_cross_language_python_oracle_agrees", False,
             "--oracle-json 未提供或文件不存在 ⇒ 跨语言核对未执行（不得当作通过）")

    result = {
        "oracle": "recon_exp04_parity.py",
        "cross_language": cross,
        "exp04_report_reference": cmp_rows,
        "exp04_operators": os.path.relpath(os.path.join(_EXP04, "operators.py"), _REPO),
        "parity_tol": PARITY_TOL,
        "parity": parity,
        "pathological": patho,
        "default_vs_mesh": g6,
        "gates": gates,
        "n_pass": sum(1 for g in gates if g["verdict"] == "PASS"),
        "n_fail": sum(1 for g in gates if g["verdict"] == "FAIL"),
        "n_skip": sum(1 for g in gates if g["verdict"] == "SKIP"),
    }
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, ensure_ascii=False)
    print("\n== recon/EXP-04 parity: %d passed, %d failed, %d skipped =="
          % (result["n_pass"], result["n_fail"], result["n_skip"]))
    print("evidence -> " + args.out)
    return 0 if result["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())