#!/usr/bin/env python3
"""L4 视觉验收：平面 FITS -> 整幅拉伸 PNG（+ 缩略图）。

依据 ACCEPTANCE_SPEC.md §5.1：对平面 FITS 自动做适合深空目标的非线性拉伸
（带遮罩的 asinh/自动拉伸），参数固定可复现，输出整幅 PNG 与缩略图。

用法:
  python3 tools/quality/plane_stretch.py --input <plane.fits> --out <full.png> \
      [--thumbnail <thumb.png>] [--thumbnail-size 2048] \
      [--stretch asinh|auto|zscale|linear] [--percentiles 0.1,99.9]

- 只读 PRIMARY HDU（或 --hdu N）；NaN/Inf/非有限像元按掩膜处理（输出 0）。
- 拉伸参数写入 PNG 文本块（pnginfo）与可选 sidecar JSON，保证可复现。
exit 0 = 成功；2 = 输入/依赖不可用（fail-closed）。
"""
from __future__ import annotations
import argparse, json, math, os, sys


class _Fail(Exception):
    """失败结论：由 main 统一转成非零退出码（EXIT-CONSISTENCY S2）。"""


def _die(msg: str) -> "NoReturn":
    print(f"PLANE_STRETCH_FAIL: {msg}", file=sys.stderr)
    raise _Fail(msg)


def stretch_to_uint8(arr, mode: str, lo_pct: float, hi_pct: float):
    import numpy as np
    finite = np.isfinite(arr)
    if not finite.any():
        _die("image has no finite pixels")
    vals = arr[finite].astype(np.float64)
    lo = float(np.percentile(vals, lo_pct))
    hi = float(np.percentile(vals, hi_pct))
    if not (hi > lo):
        hi = lo + 1.0
    x = (arr.astype(np.float64) - lo) / (hi - lo)
    if mode == "asinh":
        # 深空常用：带线性核的反正弦压缩，保留星云层次同时压暗背景
        x = np.arcsinh(x * 10.0) / math.asinh(10.0)
    elif mode == "linear":
        x = np.clip(x, 0.0, 1.0)
    elif mode in ("auto", "zscale"):
        # 自动：中位数背景 + MAD 缩放，近似 zscale
        med = float(np.median(vals))
        mad = float(np.median(np.abs(vals - med))) or (hi - lo) / 6.0
        z1, z2 = med - 2.5 * 1.4826 * mad, med + 8.0 * 1.4826 * mad
        x = (arr.astype(np.float64) - z1) / (z2 - z1) if z2 > z1 else x
        x = np.arcsinh(np.clip(x, 0.0, None) * 10.0) / math.asinh(10.0)
    else:
        _die(f"unknown stretch '{mode}'")
    x = np.clip(x, 0.0, 1.0)
    out = np.zeros(arr.shape, dtype=np.uint8)
    out[finite] = (x[finite] * 255.0 + 0.5).astype(np.uint8)
    return out, {"lo": lo, "hi": hi, "mode": mode, "lo_pct": lo_pct, "hi_pct": hi_pct}


def main(argv=None) -> int:
    try:
        return _run(argv)
    except _Fail:
        return 2


def _run(argv=None) -> int:
    ap = argparse.ArgumentParser(description="plane FITS -> stretched PNG")
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--thumbnail", default=None)
    ap.add_argument("--thumbnail-size", type=int, default=2048)
    ap.add_argument("--stretch", default="asinh", choices=("asinh", "auto", "zscale", "linear"))
    ap.add_argument("--percentiles", default="0.1,99.9")
    ap.add_argument("--hdu", type=int, default=0)
    ap.add_argument("--sidecar", default=None, help="write stretch params JSON here")
    args = ap.parse_args(argv)

    if not os.path.isfile(args.input):
        _die(f"input not found: {args.input}")
    try:
        from astropy.io import fits
        import numpy as np
        from PIL import Image
    except Exception as exc:  # noqa: BLE001
        _die(f"missing dependency: {exc}")
    try:
        lo_pct, hi_pct = (float(v) for v in args.percentiles.split(","))
    except Exception:  # noqa: BLE001
        _die("--percentiles must be 'lo,hi'")

    with fits.open(args.input, memmap=False) as hdul:
        if args.hdu >= len(hdul):
            _die(f"hdu {args.hdu} out of range ({len(hdul)} hdus)")
        data = hdul[args.hdu].data
    if data is None:
        _die("selected HDU has no image data")
    arr = np.asarray(data)
    if arr.ndim != 2:
        arr = np.squeeze(arr)
        if arr.ndim != 2:
            _die(f"expected 2-D image, got shape {arr.shape}")

    img8, params = stretch_to_uint8(arr, args.stretch, lo_pct, hi_pct)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    im = Image.fromarray(img8, mode="L")
    meta = {"source": os.path.abspath(args.input), "hdu": args.hdu,
            "shape": list(img8.shape), **params}
    im.save(args.out, pnginfo=_pnginfo(meta))
    result = {"out": os.path.abspath(args.out), **meta}
    if args.thumbnail:
        th = im.copy()
        th.thumbnail((args.thumbnail_size, args.thumbnail_size))
        os.makedirs(os.path.dirname(os.path.abspath(args.thumbnail)), exist_ok=True)
        th.save(args.thumbnail, pnginfo=_pnginfo(meta))
        result["thumbnail"] = os.path.abspath(args.thumbnail)
    if args.sidecar:
        os.makedirs(os.path.dirname(os.path.abspath(args.sidecar)), exist_ok=True)
        with open(args.sidecar, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=2)
    print("PLANE_STRETCH_PASS: " + json.dumps(result, ensure_ascii=False))
    return 0


def _pnginfo(meta: dict):
    from PIL import PngImagePlugin
    info = PngImagePlugin.PngInfo()
    for k, v in meta.items():
        info.add_text(f"astrocs_{k}", str(v))
    return info


if __name__ == "__main__":
    raise SystemExit(main())
