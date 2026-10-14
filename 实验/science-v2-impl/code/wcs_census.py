#!/usr/bin/env python3
"""S00b：全部真实帧的 WCS/几何可得性普查（只读 FITS 头，不算哈希）。

目的：AGENTS §13.1「缺几何的帧单独登记为待查，不得猜测补齐」。本脚本给出
每个数据集里「可解几何帧」与「缺 CRVAL/CRPIX/CD 帧」的**真实计数**，供
E2E 选帧与后续 normalize 输入面使用。不推断、不修补。
"""
import json
import os
import sys

from astropy.io import fits

TESTDATA = os.path.join(os.path.dirname(__file__), "..", "..", "..", "testdata")
OUT = os.path.join(os.path.dirname(__file__), "..", "run", "S00", "wcs_census.json")
SUFFIX = (".fts", ".fit", ".fits")


def geom(h):
    def num(k):
        v = h.get(k, None)
        return v if isinstance(v, (int, float)) else None
    return {
        "ctype": (h.get("CTYPE1"), h.get("CTYPE2")),
        "crval": (num("CRVAL1"), num("CRVAL2")),
        "crpix": (num("CRPIX1"), num("CRPIX2")),
        "cd": (num("CD1_1"), num("CD1_2"), num("CD2_1"), num("CD2_2")),
    }


def solvable(g):
    return (g["ctype"][0] and g["ctype"][1] and
            all(v is not None for v in g["crval"]) and
            all(v is not None for v in g["crpix"]) and
            all(v is not None for v in g["cd"]))


def cd_det(g):
    a, b, c, d = g["cd"]
    return abs(a * d - b * c)


def main():
    report = {"schema": "acsd.science-v2.wcs-census/v1",
              "testdata_root": os.path.relpath(TESTDATA),
              "note": "只读 FITS 头; 缺几何的帧登记为待查, 不猜测补齐",
              "datasets": {}}
    for name in sorted(os.listdir(TESTDATA)):
        d = os.path.join(TESTDATA, name)
        if not os.path.isdir(d) or name.endswith("calibration files"):
            continue
        frames = []
        for root, _dirs, files in os.walk(d):
            for n in sorted(files):
                if n.lower().endswith(SUFFIX):
                    frames.append(os.path.join(root, n))
        if not frames:
            continue
        ok, bad, degenerate = [], [], []
        for p in frames:
            try:
                with fits.open(p, memmap=True) as hl:
                    g = geom(hl[0].header)
            except Exception as e:
                bad.append({"path": os.path.relpath(p, TESTDATA),
                            "reason": "unreadable: %s" % type(e).__name__})
                continue
            if not solvable(g):
                bad.append({"path": os.path.relpath(p, TESTDATA),
                            "reason": "missing CTYPE/CRVAL/CRPIX/CD keys"})
            elif cd_det(g) <= 0.0:
                degenerate.append({"path": os.path.relpath(p, TESTDATA),
                                   "reason": "degenerate CD matrix (det==0)"})
            else:
                ok.append(os.path.relpath(p, TESTDATA))
        report["datasets"][name] = {
            "n_total": len(frames),
            "n_geometry_solvable": len(ok),
            "n_missing_geometry": len(bad),
            "n_degenerate_cd": len(degenerate),
            "missing_examples": bad[:5],
            "degenerate_examples": degenerate[:5],
        }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1, ensure_ascii=False)
    for k, v in report["datasets"].items():
        print("%-44s total=%-4d solvable=%-4d missing=%-3d degenerate=%d" % (
            k, v["n_total"], v["n_geometry_solvable"],
            v["n_missing_geometry"], v["n_degenerate_cd"]))
    print("output:", OUT)


if __name__ == "__main__":
    sys.exit(main())
