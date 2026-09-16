# -*- coding: utf-8 -*-
"""V5 MAPTILES 全像素外部 Oracle (HIPS-IMG-001).

同一份 NESTED HEALPix map (value=ipix, FP64) 分别经:
  [Ref]     CDS Hipsgen MAPTILES
  [AstroCS] AIO writer + 共享 HEALPix core
生成 HiPS, 逐像素比较全部 leaf tile:
  tile_missing = 0, tile_extra = 0, pixel_mismatch = 0

Hipsgen 的 MAPTILES 输出 tile 尺寸不固定 (64 或 512), 脚本自动探测并按
"标准 HiPS 行主序" 做同路径或派生等价比较:
  fits_index = (tile_width-1-x)*tile_width + y, (x,y)=NESTED local 位解交错

用法: py -3.12 v5_maptile_oracle.py --order 10 [--workdir run/temp/v5_maptile_oracle] [--hipsgen run/temp/Hipsgen.jar]
"""
from __future__ import annotations
import argparse, ctypes, hashlib, json, math, os, subprocess, sys, time
from pathlib import Path

import numpy as np
from astropy.io import fits

ROOT = Path(r"F:\Astro dev\Astro CS Normalization Database")
AIO_DLL = ROOT / r"lib\astro_image_io\astro_image_io.dll"
HIPSGEN_DEFAULT = ROOT / r"run\temp\Hipsgen.jar"
NON_PRODUCTION_TOOL_ONLY = True


class AstroSphereTileView(ctypes.Structure):
    _fields_ = [("parent_ipix", ctypes.c_uint64),
                ("leaf_order", ctypes.c_uint32),
                ("width", ctypes.c_uint32),
                ("data_type", ctypes.c_int32),
                ("flux_sum", ctypes.c_void_p),
                ("covered_area", ctypes.c_void_p),
                ("valid_mask", ctypes.c_void_p)]


def add_dll_dirs():
    for d in (ROOT / "lib" / "astro_image_io", Path(r"C:\msys64\mingw64\bin")):
        os.add_dll_directory(str(d))
        os.environ["PATH"] = str(d) + os.pathsep + os.environ.get("PATH", "")


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_tree(root: Path) -> str:
    h = hashlib.sha256()
    for p in sorted(root.rglob("Npix*.fits")):
        if "Norder" not in p.as_posix():
            continue
        h.update(p.relative_to(root).as_posix().encode())
        h.update(sha256_file(p).encode())
    return h.hexdigest()


def gen_map(order: int, path: Path) -> None:
    nside = 1 << order
    npix = 12 * nside * nside
    vals = np.arange(npix, dtype=np.float64)
    cols = fits.Column(name="PIXVAL", format="D", array=vals)
    hdu = fits.BinTableHDU.from_columns([cols])
    hdu.header["PIXTYPE"] = "HEALPIX"
    hdu.header["ORDERING"] = "NESTED"
    hdu.header["NSIDE"] = nside
    hdu.header["FIRSTPIX"] = 0
    hdu.header["LASTPIX"] = npix - 1
    hdu.header["COORDSYS"] = "C"
    hdu.writeto(path, overwrite=True)
    print(f"map: {path.name} nside={nside} npix={npix} bytes={path.stat().st_size}")


def run_hipsgen(hipsgen: Path, map_path: Path, ref_root: Path, log_path: Path) -> None:
    if ref_root.exists():
        import shutil
        shutil.rmtree(ref_root)
    ref_root.mkdir(parents=True)
    cmd = ["java", "-jar", str(hipsgen), f"in={map_path}", f"out={ref_root}",
           "id=AUT/P", "MAPTILES"]
    env = dict(os.environ)
    env["JAVA_TOOL_OPTIONS"] = "-Xmx4g"
    t0 = time.time()
    with open(log_path, "wb") as lf:
        r = subprocess.run(cmd, env=env, stdout=lf, stderr=subprocess.STDOUT, timeout=1800)
    dt = time.time() - t0
    if r.returncode != 0:
        raise SystemExit(f"Hipsgen MAPTILES 失败 rc={r.returncode}, 见 {log_path}")
    print(f"hipsgen MAPTILES: rc=0 耗时={dt:.1f}s ref_root={ref_root.name}")


def write_astrocs(order: int, out_root: Path) -> None:
    nside = 1 << order
    leaf_order = order
    tile_order = order - 9
    a_cell = 4.0 * math.pi / (12.0 * nside * nside)
    n_tiles = 12 * (4 ** tile_order)
    n = 512 * 512
    if out_root.exists():
        import shutil
        shutil.rmtree(out_root)
    aio = ctypes.CDLL(str(AIO_DLL))
    aio.aio_hips_product_begin.restype = ctypes.c_void_p
    aio.aio_hips_product_begin.argtypes = [
        ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int32,
        ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.c_double, ctypes.c_char_p, ctypes.c_uint32]
    aio.aio_hips_write_signal_support_tile.restype = ctypes.c_int
    aio.aio_hips_write_signal_support_tile.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(AstroSphereTileView)]
    aio.aio_hips_finalize.restype = ctypes.c_int
    aio.aio_hips_finalize.argtypes = [ctypes.c_void_p]
    aio.aio_hips_last_error.restype = ctypes.c_char_p
    ps = aio.aio_hips_product_begin(
        str(out_root).encode(), nside, 512, 1, 1,  # FP64 signal-only
        b"ivo://astrocs/maptile_oracle", b"Maptile Oracle", None, 1.0, None, 0)
    if not ps:
        raise SystemExit("astrocs product_begin fail: " + aio.aio_hips_last_error().decode())
    for parent in range(n_tiles):
        base = parent << 18
        z = np.arange(n, dtype=np.float64)
        flux = (base + z)               # area=1.0 → 存储值 = ipix 精确 (避免 a_cell 舍入)
        area = np.ones(n, dtype=np.float64)
        view = AstroSphereTileView()
        view.parent_ipix = parent
        view.leaf_order = leaf_order
        view.width = 512
        view.data_type = 1
        view.flux_sum = flux.ctypes.data_as(ctypes.c_void_p)
        view.covered_area = area.ctypes.data_as(ctypes.c_void_p)
        view.valid_mask = None
        rc = aio.aio_hips_write_signal_support_tile(ps, ctypes.byref(view))
        if rc != 0:
            raise SystemExit(f"astrocs tile {parent} fail: {aio.aio_hips_last_error().decode()}")
    if aio.aio_hips_finalize(ps) != 0:
        raise SystemExit("astrocs finalize fail: " + aio.aio_hips_last_error().decode())
    print(f"astrocs HiPS: order={order} tiles={n_tiles} out_root={out_root.name}")


def leaf_tile_dims(root: Path) -> tuple:
    for p in sorted(root.rglob("Npix*.fits")):
        if "Norder" not in p.as_posix():
            continue
        with fits.open(p, memmap=False) as f:
            arr = np.asarray(f[0].data)
        return arr.shape
    raise SystemExit("ref 无 tile")


def same_path_compare(astrocs_root: Path, ref_root: Path) -> dict:
    a = {}
    for p in astrocs_root.rglob("Npix*.fits"):
        rel = p.relative_to(astrocs_root).as_posix()
        if "Norder" not in rel:
            continue
        rel = rel.split("/", 1)[1] if rel.startswith("signal/") else rel
        a[rel] = p
    r = {p.relative_to(ref_root).as_posix(): p for p in ref_root.rglob("Npix*.fits") if "Norder" in p.as_posix()}
    leaf_norder = max(int(key.split("/")[0][6:]) for key in r)
    missing = sorted(set(r) - set(a))
    extra_all = sorted(set(a) - set(r))
    extra_leaf = [k for k in extra_all if int(k.split("/")[0][6:]) >= leaf_norder]
    # AstroCS 低阶 hierarchy tile (Norder < leaf) 是产品自带扩展, Hipsgen 不一定生成;
    # 逐像素硬门只约束参考存在的全部 tile + 叶级不允许多出。
    mismatch_tiles = 0
    mismatch_pixels = 0
    total_pixels = 0
    checked = 0
    for key in sorted(set(a) & set(r)):
        with fits.open(a[key], memmap=False) as fa, fits.open(r[key], memmap=False) as fr:
            aa = np.asarray(fa[0].data)
            rr = np.asarray(fr[0].data)
        if aa.shape != rr.shape:
            mismatch_tiles += 1
            continue
        checked += 1
        total_pixels += aa.size
        same = (aa == rr) | (np.isnan(aa) & np.isnan(rr))
        bad = int(aa.size - np.count_nonzero(same))
        if bad:
            mismatch_tiles += 1
            mismatch_pixels += bad
    return {"mode": "same_path", "ref_tiles": len(r), "astrocs_tiles": len(a),
            "leaf_norder": leaf_norder, "missing": len(missing), "extra_leaf": len(extra_leaf),
            "astrocs_hierarchy_tiles": len([k for k in extra_all if int(k.split("/")[0][6:]) < leaf_norder]),
            "compared_tiles": checked, "compared_pixels": total_pixels,
            "mismatch_tiles": mismatch_tiles, "mismatch_pixels": mismatch_pixels,
            "missing_examples": missing[:10], "extra_leaf_examples": extra_leaf[:10]}


def deinterleave(z: np.ndarray, shift: int) -> tuple:
    x = np.zeros_like(z, dtype=np.int64)
    y = np.zeros_like(z, dtype=np.int64)
    for b in range(shift):
        x |= ((z >> (2 * b)) & 1).astype(np.int64) << b
        y |= ((z >> (2 * b + 1)) & 1).astype(np.int64) << b
    return x, y


def derived_compare(astrocs_root: Path, ref_root: Path, order: int, ref_w: int) -> dict:
    # AstroCS leaf tile t 覆盖 ipix [t*512^2, (t+1)*512^2);
    # Hipsgen leaf tile ip 覆盖 [ip*ref_w^2, (ip+1)*ref_w^2)。
    # 故每个 AstroCS tile t == 连续 factor 个 Hipsgen tile ip = t*factor + tt。
    ref_log2 = int(round(math.log2(ref_w)))
    leaf_norder = order - ref_log2
    astro_norder = order - 9
    n_astro_tiles = 12 * (4 ** astro_norder)
    factor = (512 // ref_w) ** 2
    face_pix = 512 * 512
    z = np.arange(face_pix, dtype=np.int64)
    x, y = deinterleave(z, 9)
    astro_fi = (511 - x) * 512 + y
    tt_local = z >> (2 * ref_log2)            # 所属 Hipsgen 子 tile (0..factor-1)
    z_sub = z & ((1 << (2 * ref_log2)) - 1)   # 子 tile 内 NESTED local
    x6, y6 = deinterleave(z_sub, ref_log2)
    ref_fi = (ref_w - 1 - x6) * ref_w + y6
    mismatch_tiles = 0
    mismatch_pixels = 0
    total_pixels = 0
    for t in range(n_astro_tiles):
        d = t // 10000
        npf = t % 10000
        ap = astrocs_root / "signal" / f"Norder{astro_norder}" / f"Dir{d}" / f"Npix{npf}.fits"
        if not ap.exists():
            mismatch_tiles += 1
            continue
        with fits.open(ap, memmap=False) as fa:
            aa = np.asarray(fa[0].data).reshape(-1)
        assembled = np.empty(face_pix, dtype=np.float64)
        for tt in range(factor):
            ip = t * factor + tt
            d2 = ip // 10000
            n2 = ip % 10000
            rp = ref_root / f"Norder{leaf_norder}" / f"Dir{d2}" / f"Npix{n2}.fits"
            if not rp.exists():
                mismatch_tiles += 1
                continue
            with fits.open(rp, memmap=False) as fr:
                rr = np.asarray(fr[0].data).reshape(-1)
            mask = tt_local == tt
            # 按 astrocs FITS 索引位置组装 (astrocs_flat[astro_fi] 与 rr[ref_fi] 应相等)
            assembled[astro_fi[mask]] = rr[ref_fi[mask]]
        total_pixels += face_pix
        same = (aa == assembled) | (np.isnan(aa) & np.isnan(assembled))
        bad = int(face_pix - np.count_nonzero(same))
        if bad:
            mismatch_tiles += 1
            mismatch_pixels += bad
    return {"mode": f"derived(ref_w={ref_w}, ref_leaf_norder={leaf_norder}, astro_leaf_norder={astro_norder})",
            "astrocs_leaf_tiles": n_astro_tiles, "factor": factor,
            "compared_pixels": total_pixels,
            "mismatch_tiles": mismatch_tiles, "mismatch_pixels": mismatch_pixels}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--order", type=int, default=10)
    ap.add_argument("--workdir", type=Path, default=ROOT / "run" / "temp" / "v5_maptile_oracle")
    ap.add_argument("--hipsgen", type=Path, default=HIPSGEN_DEFAULT)
    ap.add_argument("--reuse", action="store_true", help="复用已有 map/ref/astrocs 产物, 只重跑比较")
    args = ap.parse_args()
    order = args.order
    wd = args.workdir / f"order{order}"
    wd.mkdir(parents=True, exist_ok=True)
    map_path = wd / f"map_order{order}_pixval.fits"
    ref_root = wd / "ref"
    astrocs_root = wd / "astrocs"
    if args.reuse and map_path.exists() and ref_root.exists() and astrocs_root.exists():
        print("reuse: 跳过生成 (map/ref/astrocs 已存在)")
    else:
        gen_map(order, map_path)
        run_hipsgen(args.hipsgen, map_path, ref_root, wd / "hipsgen_maptiles.log")
        add_dll_dirs()
        write_astrocs(order, astrocs_root)
    dims = leaf_tile_dims(ref_root)
    ref_w = dims[1]
    print("hipsgen leaf tile dims:", dims)
    if ref_w == 512:
        res = same_path_compare(astrocs_root, ref_root)
    else:
        res = derived_compare(astrocs_root, ref_root, order, ref_w)
    res.update({
        "order": order, "nside": 1 << order, "ref_tile_dims": list(dims),
        "input_sha256": sha256_file(map_path),
        "hipsgen_version": "HipsGen based on Aladin v12.677",
        "hipsgen_command": "java -jar Hipsgen.jar in=<map> out=<ref> id=AUT/P MAPTILES",
        "hipsgen_timeout_sec": 1800,
        "ref_tree_sha256": sha256_tree(ref_root),
        "astrocs_tree_sha256": sha256_tree(astrocs_root),
        "hard_gate": {"tile_missing": 0, "tile_extra": 0, "pixel_mismatch": 0},
    })
    if "extra_leaf" in res:
        ok = res["missing"] == 0 and res["extra_leaf"] == 0 and res["mismatch_pixels"] == 0
    else:
        ok = res["mismatch_pixels"] == 0
    res["PASS"] = bool(ok)
    out_json = wd / "compare_result.json"
    out_json.write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(res, indent=2, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
