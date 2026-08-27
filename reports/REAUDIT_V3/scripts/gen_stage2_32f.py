#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate stage2_fatduck32.json from stage2_fatduck26.json.
32R frame set: panel1 f01-f11 + panel2 f12-f22 + panel3 f23-f32 (32 frames).
Preserves all model/integration/output settings from the 26f template; only the
inputs.hips list (append f27-f32) and output mosaic path change.
This script does NOT touch any tracked repo file; it writes to C:/Users/fujia/ (Fatduck)
or the local scripts dir when invoked on Linux for dry-run validation.
"""
import json, os, sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stage2_fatduck26.json")
DST_NAME = "stage2_fatduck32.json"
DST = os.path.join(os.path.dirname(os.path.abspath(__file__)), DST_NAME)

def frame_paths():
    out = []
    for panel, start, count in ((1, 1, 11), (2, 12, 11), (3, 23, 10)):
        for i in range(start, start + count):
            out.append(
                "F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/"
                + "gc_R_panel%d_f%02d.hips" % (panel, i)
            )
    assert len(out) == 32, "expected 32 frames, got %d" % len(out)
    return out

def main():
    with open(SRC, "r", encoding="utf-8") as f:
        d = json.load(f)
    hips = frame_paths()
    d["inputs"]["hips"] = hips
    # 32R mosaic output path (distinct from the 5f/26f mosaics already on disk)
    out_mosaic = (
        "F:/Astro dev/Astro CS Normalization Database/run/temp/p2_v7/gc/"
        "audit_stage2_32f.mosaic.hips"
    )
    d["output"]["hips"] = out_mosaic
    with open(DST, "w", encoding="utf-8") as f:
        json.dump(d, f, indent=2, ensure_ascii=False)
    print("WROTE %s" % DST)
    print("n_hips=%d output=%s" % (len(hips), out_mosaic))
    print("first=%s" % hips[0])
    print("last=%s" % hips[-1])

if __name__ == "__main__":
    main()
