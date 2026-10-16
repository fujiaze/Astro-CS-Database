#!/usr/bin/env python3
"""从真实 p1_sources.json 枚举帧 PSF 顶点（空间 PSF 验证的输入身份）。

按 AGENTS.md §13.1: 输入从运行清单/产品枚举, 不从目录名猜。本脚本只做
"产品字段 → 纯文本顶点表"的无损搬运, 不做任何筛选（质量过滤在 spatial_psf
模块的 canonicalize_vertex 内）; 输出列与 verify 程序一致:
  x y sx sy theta star_id

用法:
  python3 extract_psf_vertices.py <p1_sources.json> <frame_key_substr> <out.txt>
"""
from __future__ import annotations

import json
import sys


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    src, frame_sub, out = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(src, "r", encoding="utf-8") as f:
        doc = json.load(f)
    frames = doc.get("frames", [])
    hit = None
    for fr in frames:
        if frame_sub in fr.get("file", ""):
            hit = fr
            break
    if hit is None:
        print("frame not found for substring %r; frames=%s"
              % (frame_sub, [f.get("file") for f in frames]))
        return 1
    rows = [p for p in hit.get("psf_params", [])
            if all(k in p for k in ("cx", "cy", "sx", "sy", "theta"))]
    with open(out, "w", encoding="utf-8") as f:
        for i, p in enumerate(rows):
            f.write("%.17g %.17g %.17g %.17g %.17g %d\n"
                    % (p["cx"], p["cy"], p["sx"], p["sy"], p["theta"], i))
    print("frame=%s n_psf_params=%d -> %s"
          % (hit.get("file"), len(rows), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
