#!/usr/bin/env python3
"""并行确定性判据：逐字节相同优先，不同时按冻结容差做逐文件数值等价比对。

依据（权威链）:
  * ASTROCS_DESIGN.md §8.3 不变量 / §9（CPU 后端与资源）——「1 worker 与 N worker 的等价判据
    是事前冻结的浮点容差，不是逐位一致」；
  * docs/contracts/SCHEDULER_CONTRACT.md §2.1（并行确定性口径，冻结）；
  * docs/contracts/TEST_MATRIX.md §2（通用容差规则）：
      元数据/mask/计数/索引/端口/选择结果 = 精确一致；
      FP64 非归约 rtol=1e-12, atol=1e-13×scale；
      FP32 产品非归约 rtol=5e-6, atol=1e-6×scale；
      NaN/Inf/missing 位置与语义精确一致（禁止仅比较 finite 像素）；
      绝对容差可满足性下限 atol ≥ 1 ulp(scale)。

判据分三档（逐文件）:
  identical         文件字节完全相同；
  within_tolerance  字节不同，但非数值内容精确一致、数值差在冻结容差内；
  differs           结构/键/整数/mask/NaN 位置不一致，或数值差超容差 ⇒ 判红。

用法:
  python3 eng/tools/v6/v6_numeric_equiv.py --compare-root A B [--json-out p]
  python3 eng/tools/v6/v6_numeric_equiv.py --self-test [--json-out p]
exit 0 = 等价（identical 或 within_tolerance）；1 = 判红；2 = 环境/用法错误。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import pathlib
import sys

# TEST_MATRIX §2 冻结容差（禁止表外阈值）
TOLERANCES = {
    "float64": {"rtol": 1e-12, "atol_factor": 1e-13},
    "float32": {"rtol": 5e-6, "atol_factor": 1e-6},
    "float16": {"rtol": 5e-6, "atol_factor": 1e-6},
}

# FITS 头中随运行时刻/校验和而变的卡片：不参与等价比对（语义为「运行落点元数据」，
# 与 content_digest 的 @RUNDIR@ 归一化同口径）。
VOLATILE_CARDS = {"DATE", "CHECKSUM", "DATASUM", "CHECKVER"}

FITS_SUFFIXES = {".fits", ".fts", ".fit", ".fz"}
JSON_SUFFIXES = {".json"}


def _tolerance_for(dtype_name: str) -> dict:
    key = str(dtype_name).lower()
    if key in TOLERANCES:
        return TOLERANCES[key]
    # 未知/整数 dtype 一律按「精确一致」处理，绝不放宽
    return {"rtol": 0.0, "atol_factor": 0.0}


def _array_verdict(a, b, dtype_name: str) -> dict:
    """返回 {ok, n_diff, max_abs, max_rel, reason}。NaN/Inf 位置必须精确一致。"""
    import numpy as np

    a = np.asarray(a)
    b = np.asarray(b)
    if a.shape != b.shape:
        return {"ok": False, "reason": "shape mismatch %s vs %s" % (a.shape, b.shape)}
    if a.dtype != b.dtype:
        return {"ok": False, "reason": "dtype mismatch %s vs %s" % (a.dtype, b.dtype)}
    if a.size == 0:
        return {"ok": True, "n_diff": 0, "max_abs": 0.0, "max_rel": 0.0}
    if a.dtype.kind in ("i", "u", "b", "S", "U"):
        n = int(np.count_nonzero(a != b))
        return {"ok": n == 0, "n_diff": n, "max_abs": 0.0, "max_rel": 0.0,
                "reason": "" if n == 0 else "integer/bool payload differs at %d element(s)" % n}
    if a.dtype.kind != "f":
        n = int(np.count_nonzero(a != b))
        return {"ok": n == 0, "n_diff": n, "reason": "" if n == 0 else "non-float payload differs"}

    na, nb = np.isnan(a), np.isnan(b)
    ia, ib = np.isinf(a), np.isinf(b)
    if not np.array_equal(na, nb):
        return {"ok": False, "reason": "NaN positions differ (%d vs %d)" % (int(na.sum()), int(nb.sum()))}
    if not np.array_equal(ia, ib):
        return {"ok": False, "reason": "Inf positions differ (%d vs %d)" % (int(ia.sum()), int(ib.sum()))}
    if ia.any() and not np.array_equal(a[ia], b[ib]):
        return {"ok": False, "reason": "Inf signs differ"}

    m = ~(na | ia)
    if not m.any():
        return {"ok": True, "n_diff": 0, "max_abs": 0.0, "max_rel": 0.0}
    x, y = a[m].astype("float64"), b[m].astype("float64")
    absd = np.abs(x - y)
    scale = float(np.max(np.abs(x))) if x.size else 0.0
    tol = _tolerance_for(dtype_name)
    atol = tol["atol_factor"] * scale
    rtol = tol["rtol"]
    allowed = atol + rtol * np.abs(x)
    bad = absd > allowed
    n_bad = int(np.count_nonzero(bad))
    max_abs = float(np.max(absd)) if absd.size else 0.0
    nz = np.abs(x) > 0
    max_rel = float(np.max(absd[nz] / np.abs(x[nz]))) if nz.any() else 0.0
    return {"ok": n_bad == 0, "n_diff": int(np.count_nonzero(absd > 0)),
            "n_beyond_tolerance": n_bad, "max_abs": max_abs, "max_rel": max_rel,
            "scale": scale, "atol": atol, "rtol": rtol,
            "reason": "" if n_bad == 0 else
                      "%d element(s) beyond tolerance (max_abs=%.6g, max_rel=%.6g, atol=%.6g, rtol=%.6g)"
                      % (n_bad, max_abs, max_rel, atol, rtol)}


def _fits_verdict(pa: pathlib.Path, pb: pathlib.Path) -> dict:
    try:
        from astropy.io import fits
    except ImportError as exc:  # pragma: no cover - 环境缺失时 fail-closed
        return {"ok": False, "reason": "astropy unavailable (%s): cannot judge FITS payload -> fail-closed" % exc}
    with fits.open(pa, memmap=False) as ha, fits.open(pb, memmap=False) as hb:
        if len(ha) != len(hb):
            return {"ok": False, "reason": "HDU count %d vs %d" % (len(ha), len(hb))}
        worst = {"ok": True, "n_diff": 0, "max_abs": 0.0, "max_rel": 0.0}
        for i, (ua, ub) in enumerate(zip(ha, hb)):
            ca = [(c.keyword, repr(c.value)) for c in ua.header.cards if c.keyword not in VOLATILE_CARDS]
            cb = [(c.keyword, repr(c.value)) for c in ub.header.cards if c.keyword not in VOLATILE_CARDS]
            if ca != cb:
                da = [x for x in ca if x not in cb]
                db = [x for x in cb if x not in ca]
                return {"ok": False, "reason": "HDU[%d] header differs: only-A=%s only-B=%s" % (i, da[:4], db[:4])}
            if (ua.data is None) != (ub.data is None):
                return {"ok": False, "reason": "HDU[%d] data presence differs" % i}
            if ua.data is None:
                continue
            v = _array_verdict(ua.data, ub.data, ua.data.dtype.name)
            if not v["ok"]:
                v["reason"] = "HDU[%d]: %s" % (i, v.get("reason", ""))
                return v
            if v.get("max_abs", 0.0) > worst.get("max_abs", 0.0):
                worst = v
        return worst


def _json_verdict(pa: pathlib.Path, pb: pathlib.Path, tokens=None) -> dict:
    def load(p):
        data = p.read_bytes()
        if tokens:
            for t in tokens:
                data = data.replace(t, b"@RUNDIR@")
        return json.loads(data.decode("utf-8"))

    try:
        a, b = load(pa), load(pb)
    except (OSError, ValueError) as exc:
        return {"ok": False, "reason": "json parse failed: %s" % exc}

    worst = {"max_abs": 0.0, "max_rel": 0.0, "scale": 0.0}
    problem = []

    def walk(x, y, path):
        if isinstance(x, bool) or isinstance(y, bool):
            if x is not y:
                problem.append("%s: bool %r vs %r" % (path, x, y))
            return
        if isinstance(x, dict) and isinstance(y, dict):
            if set(x) != set(y):
                problem.append("%s: key set differs only-A=%s only-B=%s"
                               % (path, sorted(set(x) - set(y))[:4], sorted(set(y) - set(x))[:4]))
                return
            for k in x:
                walk(x[k], y[k], "%s.%s" % (path, k))
            return
        if isinstance(x, list) and isinstance(y, list):
            if len(x) != len(y):
                problem.append("%s: list length %d vs %d" % (path, len(x), len(y)))
                return
            for i, (xi, yi) in enumerate(zip(x, y)):
                walk(xi, yi, "%s[%d]" % (path, i))
            return
        if isinstance(x, int) and isinstance(y, int):
            if x != y:
                problem.append("%s: int %d vs %d" % (path, x, y))
            return
        if isinstance(x, (int, float)) and isinstance(y, (int, float)):
            xf, yf = float(x), float(y)
            if math.isnan(xf) or math.isnan(yf) or math.isinf(xf) or math.isinf(yf):
                if not (math.isnan(xf) and math.isnan(yf)) and xf != yf:
                    problem.append("%s: non-finite %r vs %r" % (path, x, y))
                return
            scale = max(abs(xf), abs(yf))
            tol = TOLERANCES["float64"]
            allowed = tol["atol_factor"] * scale + tol["rtol"] * abs(xf)
            d = abs(xf - yf)
            if d > worst["max_abs"]:
                worst.update({"max_abs": d, "max_rel": (d / abs(xf)) if xf else 0.0, "scale": scale})
            if d > allowed:
                problem.append("%s: float %r vs %r beyond tolerance (|d|=%.6g > %.6g)" % (path, x, y, d, allowed))
            return
        if x != y:
            problem.append("%s: %r vs %r" % (path, x, y))

    walk(a, b, "$")
    if problem:
        return {"ok": False, "reason": "; ".join(problem[:6])}
    return {"ok": True, "n_diff": 0, **worst}


def compare_file(pa: pathlib.Path, pb: pathlib.Path, tokens=None) -> dict:
    suffix = pa.suffix.lower()
    if pa.read_bytes() == pb.read_bytes():
        return {"status": "identical", "ok": True, "kind": suffix or "<none>"}
    if suffix in FITS_SUFFIXES:
        v = _fits_verdict(pa, pb)
    elif suffix in JSON_SUFFIXES:
        v = _json_verdict(pa, pb, tokens=tokens)
    else:
        return {"status": "differs", "ok": False, "kind": suffix or "<none>",
                "reason": "binary payload differs byte-wise"}
    v = dict(v)
    v["status"] = "within_tolerance" if v.get("ok") else "differs"
    v["kind"] = suffix
    return v


def compare_roots(roots, normalize_dirs=None) -> dict:
    """roots: 长度 >=2 的目录列表（同一次比对的各 worker 预算产物根）。"""
    roots = [pathlib.Path(r) for r in roots]
    tokens = []
    if normalize_dirs:
        for d in normalize_dirs:
            for cand in (str(d), str(pathlib.Path(d).resolve())):
                b = cand.encode("utf-8")
                if b not in tokens:
                    tokens.append(b)
        tokens.sort(key=len, reverse=True)
    tokens = tokens or None

    def relset(root):
        return {str(p.relative_to(root)).replace(os.sep, "/"): p
                for p in sorted(root.rglob("*")) if p.is_file() and p.suffix != ".log"}

    base = relset(roots[0])
    files = {}
    ok = True
    for r in roots[1:]:
        cur = relset(r)
        if set(cur) != set(base):
            ok = False
            files["<file-set>"] = {"status": "differs", "ok": False,
                                   "reason": "file set differs: only-A=%s only-B=%s"
                                             % (sorted(set(base) - set(cur))[:4], sorted(set(cur) - set(base))[:4])}
            continue
        for rel, pa in base.items():
            v = compare_file(pa, cur[rel], tokens=tokens)
            prev = files.get(rel)
            if prev is None or (prev["status"] == "identical" and v["status"] != "identical"):
                files[rel] = v
            if not v.get("ok"):
                ok = False
    n_ident = sum(1 for v in files.values() if v["status"] == "identical")
    n_tol = sum(1 for v in files.values() if v["status"] == "within_tolerance")
    n_diff = sum(1 for v in files.values() if v["status"] == "differs")
    worst = max((v.get("max_rel", 0.0) for v in files.values()), default=0.0)
    return {"ok": ok, "n_files": len(base), "identical": n_ident,
            "within_tolerance": n_tol, "differs": n_diff,
            "worst_max_rel": worst,
            "offenders": {k: v for k, v in files.items() if v["status"] == "differs"}}


def self_test(json_out: str = "") -> int:
    """正例/负例自检：判据必须能绿能红（AGENTS §5 非退化判据）。"""
    import shutil
    import tempfile

    import numpy as np
    from astropy.io import fits

    cases = []

    def rec(name, expect_ok, got):
        cases.append({"case": name, "expect_ok": expect_ok, "ok": bool(got.get("ok")),
                      "verdict": got.get("status") or ("ok" if got.get("ok") else "differs"),
                      "reason": got.get("reason", "")})

    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        a32 = (np.arange(64, dtype="float32").reshape(8, 8) * 1.5 + 100.0)
        hdr = fits.Header([("BUNIT", "ADU"), ("EXPTIME", 180.0)])

        def wf(p, arr, header=hdr):
            fits.PrimaryHDU(np.asarray(arr), header=header).writeto(p, overwrite=True)

        # 1) 完全相同 → 绿
        p1, p2 = td / "a1.fits", td / "a2.fits"
        wf(p1, a32); wf(p2, a32)
        rec("fits_identical", True, compare_file(p1, p2))

        # 2) 1 ulp 级扰动 → 容差内绿
        p3 = td / "a3.fits"
        wf(p3, np.nextafter(a32, np.float32(np.inf)))
        rec("fits_within_tolerance_1ulp", True, compare_file(p1, p3))

        # 3) 超容差扰动（1e-3 相对）→ 红
        p4 = td / "a4.fits"
        wf(p4, a32 * np.float32(1.001))
        rec("fits_beyond_tolerance", False, compare_file(p1, p4))

        # 4) 整数载荷 +1 → 红
        i1, i2 = td / "i1.fits", td / "i2.fits"
        arr_i = np.arange(64, dtype="int32").reshape(8, 8)
        wf(i1, arr_i); wf(i2, arr_i + 1)
        rec("fits_integer_differs", False, compare_file(i1, i2))

        # 5) NaN 位置移动 → 红
        n1 = a32.copy(); n2 = a32.copy()
        n1[0, 0] = np.nan; n2[1, 1] = np.nan
        q1, q2 = td / "n1.fits", td / "n2.fits"
        wf(q1, n1); wf(q2, n2)
        rec("fits_nan_position_differs", False, compare_file(q1, q2))

        # 6) 头卡片变化 → 红
        h2 = fits.Header([("BUNIT", "ADU"), ("EXPTIME", 181.0)])
        q3 = td / "n3.fits"
        wf(q3, a32, header=h2)
        rec("fits_header_differs", False, compare_file(p1, q3))

        # 7) JSON 相对 1e-14 扰动 → 绿；1e-3 → 红；键变化 → 红
        j1 = td / "j1.json"; j2 = td / "j2.json"; j3 = td / "j3.json"; j4 = td / "j4.json"
        j1.write_text(json.dumps({"k": [1.0, 2.0, 3.0], "s": "x", "i": 7}), encoding="utf-8")
        j2.write_text(json.dumps({"k": [1.0, 2.0, 3.0 * (1 + 1e-14)], "s": "x", "i": 7}), encoding="utf-8")
        j3.write_text(json.dumps({"k": [1.0, 2.0, 3.0 * (1 + 1e-3)], "s": "x", "i": 7}), encoding="utf-8")
        j4.write_text(json.dumps({"k": [1.0, 2.0, 3.0], "s": "y", "i": 7}), encoding="utf-8")
        rec("json_within_tolerance", True, compare_file(j1, j2))
        rec("json_beyond_tolerance", False, compare_file(j1, j3))
        rec("json_string_differs", False, compare_file(j1, j4))

        # 8) 纯二进制扰动 → 红
        b1, b2 = td / "b1.bin", td / "b2.bin"
        b1.write_bytes(bytes(range(256))); b2.write_bytes(bytes([1]) + bytes(range(1, 256)))
        rec("binary_differs", False, compare_file(b1, b2))

        # 9) 目录级：文件集缺失 → 红
        ra, rb = td / "ra", td / "rb"
        ra.mkdir(); rb.mkdir()
        wf(ra / "p.fits", a32); wf(rb / "p.fits", a32)
        rec("roots_identical", True, compare_roots([ra, rb]))
        (rb / "extra.fits").write_bytes(b"x")
        rec("roots_file_set_differs", False, compare_roots([ra, rb]))

        # 10) 目录级：容差内 → 绿
        shutil.rmtree(rb); rb.mkdir()
        wf(rb / "p.fits", np.nextafter(a32, np.float32(np.inf)))
        rec("roots_within_tolerance", True, compare_roots([ra, rb]))

    bad = [c for c in cases if c["ok"] != c["expect_ok"]]
    report = {"schema": "astrocs.v6.numeric-equiv-selftest/v1",
              "cases": cases, "n_cases": len(cases), "n_bad": len(bad),
              "verdict": "PASS" if not bad else "FAIL"}
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for c in cases:
        print("[%s] %s (expect_ok=%s) %s" % ("PASS" if c["ok"] == c["expect_ok"] else "FAIL",
                                             c["case"], c["expect_ok"], c["reason"][:90]))
    print("NUMERIC_EQUIV_SELFTEST_%s cases=%d bad=%d" % (report["verdict"], len(cases), len(bad)))
    return 0 if not bad else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--compare-root", nargs="+", default=[],
                    help="两个或更多产物根目录，逐个与第一个比对")
    ap.add_argument("--normalize-dir", action="append", default=[],
                    help="文本/JSON 中需归一化为 @RUNDIR@ 的运行沙盒绝对路径")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--json-out", default="")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test(args.json_out)
    if len(args.compare_root) < 2:
        print("USAGE_ERROR: --compare-root 需要 >=2 个目录（或用 --self-test）", file=sys.stderr)
        return 2
    roots = [pathlib.Path(r) for r in args.compare_root]
    for r in roots:
        if not r.is_dir():
            print("USAGE_ERROR: not a directory: %s" % r, file=sys.stderr)
            return 2
    res = compare_roots(roots, normalize_dirs=args.normalize_dir or None)
    if args.json_out:
        p = pathlib.Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(res, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in res.items() if k != "offenders"}, ensure_ascii=False))
    for rel, v in list(res["offenders"].items())[:8]:
        print("  OFFENDER %s: %s" % (rel, v.get("reason", "")[:160]))
    print("NUMERIC_EQUIV_%s" % ("PASS" if res["ok"] else "FAIL"))
    return 0 if res["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
