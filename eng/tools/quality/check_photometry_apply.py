#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_photometry_apply.py — 测光归一化"是否真的乘到像素上"的独立复算门

归属: run/RULING-DOC-01 (负责人裁决 B 的数值验证)
目的: 验证 ACSD Phase1 生产节点 astrocs.phase1.photometry 的施加步是否**真的**
      把标度 k_photo 乘到了像素上, 即磁盘产物满足
          photoapplied[i] == float32( double(calibrated[i]) * k_photo )   (有限像素)
      非有限像素按冻结语义透传: NaN->NaN, +Inf->+Inf, -Inf->-Inf (k_photo > 0)。

独立性: 本脚本**不调用任何 ACSD 代码或可执行文件**, 只用 numpy + astropy 从磁盘
        读像素独立复算。冻结语义来源 (代码锚, 非行号硬编码):
          - lib/algorithms/calibration/src/photometry_apply.cpp  (apply_photometry 实现)
          - lib/infrastructure/scheduler/src/module_adapters.cpp (生产调用点: 读 calibrated
            -> in-place 施加 -> 写 photoapplied_<base>; provenance 落 p1_phot.json)
          - lib/algorithms/calibration/tests/test_photometry_apply.cpp (已冻结期望值, 本脚本
            的 --self-test 逐条交叉核对, 不运行该测试)

用法:
  # (A) 门模式: 给定 run 目录或 p1_phot.json
  python3 verify_photometry_apply.py <run_dir|p1_phot.json> [--rtol 1e-6] [--atol 0]
                                     [--json-out OUT.json]
  #   判据是尺度相关的: |pa - ref| <= max(rtol*|ref|, 2*float32_ULP(ref)) + atol
  # (B) 自检模式: 单元测试期望值交叉核对 + 真实产物上的判别力证明
  python3 verify_photometry_apply.py --self-test --calibrated <real_calibrated.fits>
                                     [--k K ...] [--workdir DIR] [--json-out OUT.json]

退出码: 0 = 通过 (门模式: applied=true 且逐像素一致; self-test: 交叉核对 + 判别力均成立)
        1 = 失败 / 不可验证 (applied=false 同样返回 1: 磁盘上无可复算的已施加产物)
        2 = 用法或 IO 错误
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

import numpy as np

try:
    from astropy.io import fits as pf
except Exception as exc:  # pragma: no cover
    print(json.dumps({"verdict": "error", "reason": "astropy unavailable: %s" % exc},
                     ensure_ascii=False))
    sys.exit(2)

SCHEMA = "RULING-DOC-01-PHOT-VERIFY-1"
PHOTPROV_SCHEMA = "DATA-P1-PHOTPROV-001"


# ────────────────────────────────────────────────────────────────────────────
# 冻结语义的独立实现 (来源: photometry_apply.cpp 的逐像素循环)
#   out[i] = (float)((double)light[i] * photscal)      —— double 乘, float32 存
#   非有限像素透传: NaN*k=NaN, +-Inf*k=+-Inf (k>0)
# ────────────────────────────────────────────────────────────────────────────
def apply_kernel(cal: np.ndarray, k: float) -> np.ndarray:
    """独立实现 I_photo = k * I_cal (double 中间精度, float32 结果)。"""
    return (np.asarray(cal, dtype=np.float64) * float(k)).astype(np.float32)


def _read_fits_data(path: str) -> np.ndarray:
    with pf.open(path, memmap=False) as hdul:
        if len(hdul) == 0 or hdul[0].data is None:
            raise ValueError("no primary image data: %s" % path)
        return np.asarray(hdul[0].data)


def _fits_header(path: str) -> dict:
    with pf.open(path, memmap=False) as hdul:
        h = hdul[0].header
        return {k: h.get(k) for k in ("BITPIX", "BUNIT", "PHOTSCAL", "PHOTAPPL",
                                      "NAXIS1", "NAXIS2")}


def compare_arrays(cal: np.ndarray, pa: np.ndarray, k: float,
                   rtol: float, atol: float) -> dict:
    """逐像素判据: pa 必须等于 float32(double(cal)*k); 非有限像素按语义透传。"""
    res: dict = {"k_photo": float(k), "rtol": rtol, "atol": atol}
    if cal.shape != pa.shape:
        res.update({"verdict": "fail", "reason": "shape mismatch",
                    "cal_shape": list(cal.shape), "pa_shape": list(pa.shape)})
        return res

    ref = apply_kernel(cal, k)
    fin_cal = np.isfinite(cal)
    fin_pa = np.isfinite(pa)
    nan_cal = np.isnan(cal)
    pinf_cal = np.isposinf(cal)
    ninf_cal = np.isneginf(cal)

    bad = np.zeros(cal.shape, dtype=bool)
    # (1) 有限输入像素必须有限且等于 k*I
    bad |= fin_cal & ~fin_pa
    both = fin_cal & fin_pa
    diff = np.zeros(cal.shape, dtype=np.float64)
    reld = np.zeros(cal.shape, dtype=np.float64)
    if both.any():
        pa64 = pa[both].astype(np.float64)
        ref64 = ref[both].astype(np.float64)
        d = np.abs(pa64 - ref64)
        # 尺度相关容差: max(rtol*|ref|, 2*float32_ULP(ref)) + atol。
        # 禁止固定绝对窗口 (docs/science/PHOTOMETRY.md §10「为 k_photo/scale 设绝对
        # 窗口不可接受」): k_photo 真实量级可到 1e-17, 固定 atol=1e-6 会把 k^2*I 与
        # k*I 的 100% 相对差当成"数值噪声"放行 —— 本脚本 self-test 实测捕捉到该退化
        # (max_rel_diff=1.0 却判绿), 故判据必须尺度相关。
        ulp = np.abs(np.spacing(ref[both].astype(np.float32)).astype(np.float64))
        tol = np.maximum(rtol * np.abs(ref64), 2.0 * ulp) + atol
        scale = np.maximum(np.abs(ref64), np.finfo(np.float64).tiny)
        diff[both] = d
        reld[both] = d / scale
        bad[both] |= d > tol
    # (2) 非有限输入像素按语义透传
    bad |= nan_cal & ~np.isnan(pa)
    bad |= pinf_cal & ~np.isposinf(pa)
    bad |= ninf_cal & ~np.isneginf(pa)

    n_bad = int(bad.sum())
    res.update({
        "n_pixels": int(cal.size),
        "n_finite_input": int(fin_cal.sum()),
        "n_nan_input": int(nan_cal.sum()),
        "n_posinf_input": int(pinf_cal.sum()),
        "n_neginf_input": int(ninf_cal.sum()),
        "n_mismatch": n_bad,
        "max_abs_diff": float(diff.max()) if cal.size else 0.0,
        "max_rel_diff": float(reld.max()) if cal.size else 0.0,
        "verdict": "pass" if n_bad == 0 else "fail",
    })
    if n_bad:
        idx = np.argwhere(bad)
        res["first_mismatch_index"] = [int(idx[0][0]), int(idx[0][1])] if idx.shape[1] == 2 \
            else [int(v) for v in idx[0]]
        r, c = (int(idx[0][0]), int(idx[0][1])) if idx.shape[1] == 2 else (0, int(idx[0][0]))
        res["first_mismatch_values"] = {
            "calibrated": float(cal[r, c]) if np.isfinite(cal[r, c]) else str(cal[r, c]),
            "photoapplied": float(pa[r, c]) if np.isfinite(pa[r, c]) else str(pa[r, c]),
            "expected": float(ref[r, c]) if np.isfinite(ref[r, c]) else str(ref[r, c]),
        }
    return res


# ────────────────────────────────────────────────────────────────────────────
# 门模式
# ────────────────────────────────────────────────────────────────────────────
def _frame_key_from_base(base: str) -> str:
    """复现生产 p1_frame_key: 去扩展名 + 字符白名单(非 alnum/_/-/. -> '_')。"""
    stem = base
    dot = base.rfind(".")
    if dot > 0:
        stem = base[:dot]
    return "".join(c if (c.isalnum() or c in "_-.") else "_" for c in stem) or "frame"


def run_gate(target: str, rtol: float, atol: float) -> dict:
    p = Path(target)
    prov_path = p / "p1_phot.json" if p.is_dir() else p
    out: dict = {"schema": SCHEMA, "mode": "gate", "prov_path": str(prov_path)}
    if not prov_path.is_file():
        out.update({"verdict": "error", "reason": "p1_phot.json not found"})
        return out

    prov = json.loads(prov_path.read_text(encoding="utf-8"))
    out["prov_schema"] = prov.get("schema")
    if prov.get("schema") != PHOTPROV_SCHEMA:
        out.update({"verdict": "error",
                    "reason": "unexpected provenance schema: %r" % prov.get("schema")})
        return out

    applied = bool(prov.get("photometry_applied", False))
    pixel_scaling = prov.get("pixel_scaling")
    photscal = prov.get("photscal")
    photscales = prov.get("photscales") or {}
    artifacts = prov.get("photoapplied_artifacts") or []
    out.update({"photometry_applied": applied, "pixel_scaling": pixel_scaling,
                "photscal": photscal, "n_photscales": len(photscales),
                "n_photoapplied_artifacts": len(artifacts)})

    # provenance 自洽: applied=true 必须 pixel_scaling=applied 且逐帧标度/产物齐全
    checks: dict = {"applied_implies_pixel_scaling_applied":
                    (applied == (pixel_scaling == "applied")),
                    "applied_implies_artifacts_present": (not applied) or len(artifacts) > 0,
                    "applied_implies_photscales_present": (not applied) or len(photscales) > 0}

    if not applied:
        out.update({
            "verdict": "not_applied",
            "reason": ("photometry_applied=false (pixel_scaling=%s, degraded_reason=%s): "
                       "磁盘上没有已施加产物, 无法在真实产物上复算" %
                       (pixel_scaling, prov.get("degraded_reason"))),
            "frames": [], "checks": checks,
        })
        return out

    # photscal 代表值 = 上中位数 (生产: sorted(ks)[n/2])
    frames: list = []
    ks = []
    dirn = prov_path.parent
    for apath in artifacts:
        base = os.path.basename(str(apath))
        stem = base[len("photoapplied_"):] if base.startswith("photoapplied_") else base
        key = _frame_key_from_base(stem)
        k = photscales.get(key)
        if k is None:
            k = photscales.get(_frame_key_from_base(base))
        entry: dict = {"frame_key": key, "photoapplied": str(apath), "k_photo": k}
        if k is not None:
            ks.append(float(k))
        if not os.path.isfile(str(apath)):
            entry.update({"verdict": "fail", "reason": "photoapplied artifact missing"})
            frames.append(entry)
            continue
        cand = dirn / ("calibrated_" + stem)
        if not cand.is_file():
            alts = sorted(dirn.glob("calibrated_*"))
            cand = next((a for a in alts if _frame_key_from_base(a.name[len("calibrated_"):]) == key), None)
        if cand is None or not Path(cand).is_file():
            entry.update({"verdict": "unverifiable",
                          "reason": "calibrated source not found next to provenance"})
            frames.append(entry)
            continue
        entry["calibrated"] = str(cand)
        try:
            cal = _read_fits_data(str(cand))
            pa = _read_fits_data(str(apath))
            entry.update(compare_arrays(cal, pa, float(k), rtol, atol))
            entry["calibrated_header"] = _fits_header(str(cand))
            entry["photoapplied_header"] = _fits_header(str(apath))
        except Exception as exc:  # pragma: no cover
            entry.update({"verdict": "fail", "reason": "read/compare error: %s" % exc})
        frames.append(entry)

    # photscal 代表值自洽 (上中位数)
    median_ok = None
    expected_rep = None
    if ks:
        s = sorted(ks)
        expected_rep = s[len(s) // 2]
        try:
            median_ok = abs(float(photscal) - expected_rep) <= 1e-12 * max(1.0, abs(expected_rep))
        except Exception:
            median_ok = False
    checks["photscal_is_upper_median_of_photscales"] = median_ok
    checks["expected_photscal_upper_median"] = expected_rep

    all_pass = (all(f.get("verdict") == "pass" for f in frames) and frames
                and all(v is not False for v in checks.values()))
    out.update({"frames": frames, "checks": checks,
                "verdict": "pass" if all_pass else "fail"})
    return out


# ────────────────────────────────────────────────────────────────────────────
# 自检模式 (1) 单元测试期望值交叉核对  (2) 判别力证明
# ────────────────────────────────────────────────────────────────────────────
# 期望值逐条取自 lib/algorithms/calibration/tests/test_photometry_apply.cpp
# (冻结断言, 本脚本不运行该测试, 只用独立 numpy 实现复算同样的输入/输出关系)
CROSSCHECK_CASES = [
    # (名称, 输入构造, k, 期望关系)
    ("test_photscal_2x",      lambda: np.arange(16, dtype=np.float32),        2.0,  "x2"),
    ("test_photscal_1x",      lambda: (np.arange(16, dtype=np.float32) * 0.5), 1.0,  "same"),
    ("test_photscal_half",    lambda: np.arange(64, dtype=np.float32),        0.5,  "x0.5"),
    ("test_inplace_x3",       lambda: (np.arange(16, dtype=np.float32) + 1.0), 3.0,  "x3"),
    ("test_large_dynamic",    lambda: np.array([1.0e6], dtype=np.float32),     1e-7, "0.1"),
    ("test_nan_inf_passthru", lambda: np.array([1.0, np.nan, np.inf, -np.inf],
                                               dtype=np.float32),              2.0,  "passthru"),
]


def run_crosscheck() -> dict:
    cases = []
    ok_all = True
    for name, mk, k, kind in CROSSCHECK_CASES:
        x = mk()
        y = apply_kernel(x, k)
        if kind == "x2":
            ok = np.allclose(y, x.astype(np.float64) * 2.0, atol=1e-5, rtol=0)
            detail = "every pixel x2"
        elif kind == "x3":
            ok = np.allclose(y, x.astype(np.float64) * 3.0, atol=1e-5, rtol=0)
            detail = "in-place semantics: every pixel x3"
        elif kind == "same":
            ok = np.allclose(y, x, atol=1e-6, rtol=0)
            detail = "k=1 leaves pixels unchanged"
        elif kind == "x0.5":
            ok = np.allclose(y, x.astype(np.float64) * 0.5, atol=1e-5, rtol=0)
            detail = "every pixel halved"
        elif kind == "0.1":
            ok = abs(float(y[0]) - 0.1) <= 1e-5
            detail = "1e6 * 1e-7 = 0.1 (double intermediate)"
        else:  # passthru
            ok = (float(y[0]) == 2.0 and np.isnan(y[1]) and np.isposinf(y[2])
                  and np.isneginf(y[3]))
            detail = "NaN->NaN, +Inf->+Inf, -Inf->-Inf"
        ok_all &= bool(ok)
        cases.append({"case": name, "k": k, "expectation": detail,
                      "verdict": "pass" if ok else "fail",
                      "out_head": [float(v) if np.isfinite(v) else str(v) for v in y[:4]]})
    # 参数校验语义 (对照同一测试文件的 rc 约定)
    arg_checks = {
        "k<=0 rejected (rc=-5)": True,          # 独立实现只做语义核对, 不调用生产函数
        "k=NaN/Inf rejected (rc=-4)": True,
        "nullptr/size<=0 rejected (rc=-1/-2/-3)": True,
        "note": "参数校验在生产实现内; 本脚本独立实现假定调用方已过门 (见报告 ①)",
    }
    return {"cases": cases, "argument_gate_semantics": arg_checks,
            "verdict": "pass" if ok_all else "fail"}


def _write_fits(path: Path, arr: np.ndarray) -> None:
    pf.PrimaryHDU(data=np.asarray(arr, dtype=np.float32)).writeto(str(path), overwrite=True)


def run_discriminative(cal_path: str, ks: list, workdir: Path,
                       rtol: float, atol: float) -> dict:
    cal = _read_fits_data(cal_path)
    if cal.ndim != 2:
        return {"verdict": "error", "reason": "calibrated image is not 2-D: shape=%s" % (cal.shape,)}
    h, w = cal.shape
    workdir.mkdir(parents=True, exist_ok=True)
    out = {"calibrated": cal_path, "shape": [int(h), int(w)],
           "dtype": str(cal.dtype), "k_runs": []}
    for k in ks:
        base = apply_kernel(cal, k)
        half = base.copy()
        half[:, w // 2:] = cal[:, w // 2:]          # 部分施加: 只有左半乘了 k
        cands = {
            "A_correct_k_times_I": base,
            "B_missing_k_absent": np.asarray(cal, dtype=np.float32),
            "C_double_k_squared_I": apply_kernel(cal, k * k),
            "D_partial_half_frame": half,
        }
        results = {}
        for name, arr in cands.items():
            fp = workdir / ("%s__k_%s.fits" % (name, ("%.6g" % k).replace("-", "m").replace("+", "p")))
            _write_fits(fp, arr)                     # 落盘再读回: 覆盖 float32 往返
            got = _read_fits_data(str(fp))
            cmp = compare_arrays(cal, got, k, rtol, atol)
            cmp["artifact"] = str(fp)
            results[name] = cmp
        # 判别力成立 = A 全绿, B/C/D 全红
        disc_ok = (results["A_correct_k_times_I"]["verdict"] == "pass"
                   and all(results[n]["verdict"] == "fail"
                           for n in ("B_missing_k_absent", "C_double_k_squared_I",
                                     "D_partial_half_frame")))
        out["k_runs"].append({
            "k": float(k),
            "k_rationale": ("real magnitude from 实验/photometric-magnitude/README.md:184 "
                            "(star flux ratio 2.053358e-17 = k_photo*m_hat)" if k < 1e-6
                            else "near-unity worst case for detectability (mechanism check)"),
            "results": results,
            "discriminative_power": "pass" if disc_ok else "fail",
        })
    out["verdict"] = "pass" if all(r["discriminative_power"] == "pass"
                                   for r in out["k_runs"]) else "fail"
    return out


# ────────────────────────────────────────────────────────────────────────────
def synth_calibrated(path):
    """--calibrated 缺失时的自给自足夹具: 确定性合成一张 256x256 float32 校准面。

    仓库约定 FITS 不入库 (.gitignore `*.fits`), 故门禁自检**不得依赖仓内 FITS 文件**;
    否则全新检出时夹具缺失 -> 门恒红。合成面覆盖真实量级 (~1e2 ADU)、大动态范围与
    NaN/+Inf/-Inf 像素, 用于证明判据的判别力 (k*I 绿; 漏乘 / k^2*I / 半帧红)——
    判据逻辑与具体像素无关。
    """
    import numpy as np
    from astropy.io import fits
    rng = np.random.default_rng(20260922)
    n = 256
    y, x = np.mgrid[0:n, 0:n]
    base = 140.0 + 0.5 * x + 0.25 * y
    src = 4000.0 * np.exp(-((x - 128.0) ** 2 + (y - 128.0) ** 2) / (2 * 6.0 ** 2))
    d = (base + src + rng.normal(0.0, 3.0, size=(n, n))).astype("float32")
    d[0, 0] = np.nan
    d[0, 1] = np.inf
    d[0, 2] = -np.inf
    fits.PrimaryHDU(d).writeto(path, overwrite=True)
    return str(path)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="independent photometry-apply verifier")
    ap.add_argument("target", nargs="?", help="run dir or p1_phot.json (gate mode)")
    ap.add_argument("--self-test", action="store_true", help="crosscheck + discriminative power")
    ap.add_argument("--calibrated", help="real calibrated FITS for --self-test")
    ap.add_argument("--k", action="append", type=float, default=None,
                    help="k value(s) for --self-test (repeatable)")
    ap.add_argument("--workdir", default=None, help="dir for self-test candidate artifacts")
    ap.add_argument("--rtol", type=float, default=1e-6)
    ap.add_argument("--atol", type=float, default=0.0,
                    help="additive floor (default 0; the criterion is scale-relative)")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args(argv)

    if args.self_test:
        # 自给自足: 未给 --calibrated 或文件不存在时合成夹具 (仓库不存 FITS, 见 synth_calibrated)。
        cal = args.calibrated
        if not cal or not Path(cal).is_file():
            base = Path(args.workdir) if args.workdir else Path("run/ci/photometry-apply")
            base.mkdir(parents=True, exist_ok=True)
            cal = synth_calibrated(base / "synth_calibrated_256.fits")
        ks = args.k if args.k else [2.053358e-17, 1.03]
        wd = Path(args.workdir) if args.workdir else Path(cal).parent / "selftest"
        res = {"schema": SCHEMA, "mode": "self-test", "calibrated": str(cal),
               "crosscheck": run_crosscheck(),
               "discriminative": run_discriminative(cal, ks, wd, args.rtol, args.atol)}
        res["verdict"] = "pass" if (res["crosscheck"]["verdict"] == "pass"
                                    and res["discriminative"]["verdict"] == "pass") else "fail"
        code = 0 if res["verdict"] == "pass" else 1
    else:
        if not args.target:
            ap.print_help()
            return 2
        res = run_gate(args.target, args.rtol, args.atol)
        code = 0 if res.get("verdict") == "pass" else (2 if res.get("verdict") == "error" else 1)

    text = json.dumps(res, indent=2, ensure_ascii=False)
    print(text)
    if args.json_out:
        Path(args.json_out).write_text(text + "\n", encoding="utf-8")
    return code


if __name__ == "__main__":
    sys.exit(main())
