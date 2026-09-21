#!/usr/bin/env python3
"""L4 视觉验收：整幅 PNG -> 固定网格分块（行列编号）+ 整幅缩略图。

依据 ACCEPTANCE_SPEC.md §5.1：按固定网格把整幅 PNG 切成便于目检的分块
（保留行列编号与整幅缩略图）；参数固定可复现。

用法:
  python3 eng/tools/quality/plane_chunks.py --input <full.png> --outdir <tiles> \
      [--grid 8] [--tile 1024] [--label] [--thumbnail 2048] [--overlap 0]

- 按 grid×grid 均匀切块（每块 tile×tile，含边框缩放）；行/列编号画在角标。
- 输出 tiles_index.json（每块行列号 + 源像素范围）与缩略图。
exit 0 = 成功；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations
import argparse, json, os, sys


class _Fail(Exception):
    """失败结论：由 main 统一转成非零退出码（EXIT-CONSISTENCY S2）。"""


def _die(msg: str) -> "NoReturn":
    print(f"PLANE_CHUNKS_FAIL: {msg}", file=sys.stderr)
    raise _Fail(msg)


def _label(img, text: str, scale: int = 3):
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    x0, y0 = 8, 8
    d.rectangle([x0 - 4, y0 - 4, x0 + 9 * len(text) * scale, y0 + 9 * scale], fill=0)
    try:
        d.text((x0, y0), text, fill=255)
    except Exception:  # noqa: BLE001
        pass
    return img


def main(argv=None) -> int:
    try:
        return _run(argv)
    except _Fail:
        return 2


def _run(argv=None) -> int:
    ap = argparse.ArgumentParser(description="PNG -> grid tiles + thumbnail")
    ap.add_argument("--input", required=True)
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--grid", type=int, default=8)
    ap.add_argument("--tile", type=int, default=1024)
    ap.add_argument("--overlap", type=int, default=0)
    ap.add_argument("--label", action="store_true")
    ap.add_argument("--thumbnail", type=int, default=2048)
    args = ap.parse_args(argv)

    if not os.path.isfile(args.input):
        _die(f"input not found: {args.input}")
    if args.grid < 1 or args.tile < 16:
        _die("--grid must be >=1 and --tile >=16")
    try:
        from PIL import Image
    except Exception as exc:  # noqa: BLE001
        _die(f"missing dependency: {exc}")

    im = Image.open(args.input)
    if im.mode not in ("L", "RGB"):
        im = im.convert("L")
    W, H = im.size
    os.makedirs(args.outdir, exist_ok=True)
    step_x = max(1, W // args.grid)
    step_y = max(1, H // args.grid)
    index = []
    for r in range(args.grid):
        for c in range(args.grid):
            cx0 = c * step_x
            cy0 = r * step_y
            cx1 = W if c == args.grid - 1 else min(W, cx0 + step_x)
            cy1 = H if r == args.grid - 1 else min(H, cy0 + step_y)
            if args.overlap:
                cx0 = max(0, cx0 - args.overlap); cy0 = max(0, cy0 - args.overlap)
                cx1 = min(W, cx1 + args.overlap); cy1 = min(H, cy1 + args.overlap)
            if cx1 <= cx0 or cy1 <= cy0:
                continue
            tile = im.crop((cx0, cy0, cx1, cy1))
            tw, th = tile.size
            scale = min(args.tile / tw, args.tile / th)
            if scale < 1.0:
                tile = tile.resize((max(1, int(tw * scale)), max(1, int(th * scale))),
                                   Image.LANCZOS)
            if args.label:
                tile = _label(tile.convert("L"), f"r{r:02d} c{c:02d}")
            name = f"tile_r{r:02d}_c{c:02d}.png"
            path = os.path.join(args.outdir, name)
            tile.save(path)
            index.append({"file": name, "row": r, "col": c,
                          "src_x0": cx0, "src_y0": cy0, "src_x1": cx1, "src_y1": cy1})
    if args.thumbnail:
        th = im.copy()
        th.thumbnail((args.thumbnail, args.thumbnail))
        th.save(os.path.join(args.outdir, "thumbnail.png"))
    with open(os.path.join(args.outdir, "tiles_index.json"), "w", encoding="utf-8") as f:
        json.dump({"source": os.path.abspath(args.input), "source_size": [W, H],
                   "grid": args.grid, "tile_px": args.tile, "overlap": args.overlap,
                   "n_tiles": len(index), "tiles": index}, f, ensure_ascii=False, indent=2)
    print("PLANE_CHUNKS_PASS: " + json.dumps(
        {"outdir": os.path.abspath(args.outdir), "n_tiles": len(index),
         "grid": args.grid, "source_size": [W, H]}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
