# -*- coding: utf-8 -*-
"""V5 SNR-PREC-001: SNR Catalogue TSV FP32/FP64 文本精度 round-trip.

生成 10000+ 随机 + 边界 SNR 值 (FP32/FP64), 经 AIO writer 写出 SNR HiPS,
逐行解析 TSV 并与期望值做 bitwise (struct.pack) 比较:
  FP32: %.9g  (float32 round-trip)
  FP64: %.17g (float64 round-trip)
并核对 snr/metadata.xml datatype = float/double。

用法: py -3.12 v5_snr_precision_roundtrip.py [--workdir run/temp/v5_snr_precision]
"""
from __future__ import annotations
import argparse, ctypes, json, math, os, random, struct, sys
from pathlib import Path

import numpy as np

ROOT = Path(r"F:\Astro dev\Astro CS Normalization Database")
AIO_DLL = ROOT / r"lib\astro_image_io\astro_image_io.dll"
NON_PRODUCTION_TOOL_ONLY = True


class AstroSphereTileView(ctypes.Structure):
    _fields_ = [("parent_ipix", ctypes.c_uint64),
                ("leaf_order", ctypes.c_uint32),
                ("width", ctypes.c_uint32),
                ("data_type", ctypes.c_int32),
                ("flux_sum", ctypes.c_void_p),
                ("covered_area", ctypes.c_void_p),
                ("valid_mask", ctypes.c_void_p)]


class AioHipsSnrPoint(ctypes.Structure):
    _fields_ = [("ra", ctypes.c_double), ("dec", ctypes.c_double),
                ("snr", ctypes.c_double),
                ("star_id", ctypes.c_int64),
                ("quality_flags", ctypes.c_uint32),
                ("photometric_status", ctypes.c_uint32)]


def make_values(dtype: str, n_rand: int = 10000) -> list:
    if dtype == "fp32":
        f = np.float32
        finfo = np.finfo(np.float32)
    else:
        f = np.float64
        finfo = np.finfo(np.float64)
    rng = np.random.default_rng(20260809)
    vals = [f(0.0), f(-0.0), f(1.0), f(-1.0),
            f(finfo.max), f(-finfo.max),
            f(finfo.tiny), f(-finfo.tiny),          # 最小正规数
            f(np.nextafter(f(0.0), f(1.0))),         # 最小次正规
            f(np.nextafter(f(finfo.tiny), f(0.0))),
            f(0.1), f(-0.1), f(12345.6789), f(3.14159265358979)]
    mags = 10.0 ** rng.uniform(-40, 40, n_rand)
    signs = rng.choice([-1.0, 1.0], n_rand)
    vals += [f(s * m) for s, m in zip(signs, mags)]
    vals += [f(x) for x in rng.uniform(-1e6, 1e6, n_rand)]
    return vals


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", type=Path, default=ROOT / "run" / "temp" / "v5_snr_precision")
    args = ap.parse_args()
    wd = args.workdir
    wd.mkdir(parents=True, exist_ok=True)
    for d in (ROOT / "lib" / "astro_image_io", Path(r"C:\msys64\mingw64\bin")):
        os.add_dll_directory(str(d))
        os.environ["PATH"] = str(d) + os.pathsep + os.environ.get("PATH", "")
    aio = ctypes.CDLL(str(AIO_DLL))
    aio.aio_hips_product_begin.restype = ctypes.c_void_p
    aio.aio_hips_product_begin.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
        ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.c_double, ctypes.c_char_p, ctypes.c_uint32]
    aio.aio_hips_write_signal_support_tile.restype = ctypes.c_int
    aio.aio_hips_write_signal_support_tile.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(AstroSphereTileView)]
    aio.aio_hips_write_snr_points.restype = ctypes.c_int
    aio.aio_hips_write_snr_points.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(AioHipsSnrPoint), ctypes.c_int]
    aio.aio_hips_finalize.restype = ctypes.c_int
    aio.aio_hips_finalize.argtypes = [ctypes.c_void_p]
    aio.aio_hips_last_error.restype = ctypes.c_char_p

    results = {}
    total_ok = True
    for dtype in ("fp32", "fp64"):
        out = wd / dtype
        import shutil
        if out.exists():
            shutil.rmtree(out)
        dt = 0 if dtype == "fp32" else 1
        ps = aio.aio_hips_product_begin(
            str(out).encode(), 512, 512, dt, 7,  # SIGNAL|SUPPORT|SNR
            b"ivo://astrocs/snr_precision", b"SNR Precision", None, 1.0, None, 0)
        if not ps:
            raise SystemExit("begin fail: " + aio.aio_hips_last_error().decode())
        n = 512 * 512
        flux = np.ones(n, dtype=np.float64)
        area = np.ones(n, dtype=np.float64)
        view = AstroSphereTileView()
        view.parent_ipix = 0
        view.leaf_order = 9
        view.width = 512
        view.data_type = dt
        view.flux_sum = flux.ctypes.data_as(ctypes.c_void_p)
        view.covered_area = area.ctypes.data_as(ctypes.c_void_p)
        view.valid_mask = None
        if aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view)) != 0:
            raise SystemExit("tile fail: " + aio.aio_hips_last_error().decode())
        vals = make_values(dtype)
        pts = (AioHipsSnrPoint * len(vals))()
        expected = {}
        for i, v in enumerate(vals):
            ra = (i * 37.0) % 360.0
            dec = -80.0 + (i * 53.0) % 160.0
            sid = 1000000 + i
            pts[i].ra = ra
            pts[i].dec = dec
            pts[i].snr = float(v)
            pts[i].star_id = sid
            pts[i].quality_flags = (i % 7)
            pts[i].photometric_status = (i % 3)
            expected[sid] = float(v)
        if aio.aio_hips_write_snr_points(ps, pts, len(vals)) != 0:
            raise SystemExit("snr fail: " + aio.aio_hips_last_error().decode())
        if aio.aio_hips_finalize(ps) != 0:
            raise SystemExit("finalize fail: " + aio.aio_hips_last_error().decode())
        exp_path = wd / f"expected_{dtype}.jsonl"
        with open(exp_path, "w", encoding="utf-8") as f:
            for sid, v in expected.items():
                f.write(json.dumps({"star_id": sid, "snr": v}) + "\n")
        # 解析 TSV
        actual = {}
        dup = 0
        for p in (out / "snr").rglob("Npix*.tsv"):
            for line in p.read_text(encoding="utf-8").splitlines():
                if not line.strip() or line.startswith("#"):
                    continue
                ff = line.split()
                sid = int(ff[0])
                val = float(ff[3])
                if sid in actual:
                    dup += 1
                actual[sid] = val
        missing = set(expected) - set(actual)
        extra = set(actual) - set(expected)
        pack = struct.pack if False else None
        bad = 0
        fmt = "<f" if dtype == "fp32" else "<d"
        for sid in set(expected) & set(actual):
            if struct.pack(fmt, expected[sid]) != struct.pack(fmt, actual[sid]):
                bad += 1
        meta = (out / "snr" / "properties").read_text(encoding="utf-8") if (out / "snr" / "properties").exists() else ""
        meta_xml = ""
        for mp in (out / "snr").rglob("metadata.xml"):
            meta_xml = mp.read_text(encoding="utf-8")
            break
        datatype_ok = ("datatype=\"float\"" in meta_xml) if dtype == "fp32" else ("datatype=\"double\"" in meta_xml)
        ok = not missing and not extra and dup == 0 and bad == 0 and datatype_ok
        total_ok = total_ok and ok
        results[dtype] = {"points": len(vals), "parsed": len(actual), "missing": len(missing),
                          "extra": len(extra), "duplicates": dup, "bitwise_mismatch": bad,
                          "datatype_ok": datatype_ok, "PASS": ok}
        print(json.dumps({"dtype": dtype, **results[dtype]}, ensure_ascii=False))
    (wd / "snr_precision_result.json").write_text(
        json.dumps(results, indent=2, ensure_ascii=False), encoding="utf-8")
    return 0 if total_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
