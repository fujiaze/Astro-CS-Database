#!/usr/bin/env python3
# NON_PRODUCTION_TOOL_ONLY
"""Validate every SNR Catalogue HiPS row is stored in the correct NESTED cell.

HiPS 标准瓦片命名: NorderK/DirD/NpixN.<ext> 中 N = ipix % 10000, D = ipix / 10000
(Hipsgen LINT/CHECK 已验证该约定), 因此完整 ipix = D*10000 + N。

Requires:
  astropy
  astropy-healpix
Use a pinned test venv. This script is an external oracle, not production code.
"""
from __future__ import annotations
import argparse, re, sys
from pathlib import Path

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("snr_root", type=Path)
    args = ap.parse_args()
    try:
        import astropy.units as u
        from astropy_healpix import HEALPix
    except Exception as exc:
        print("ERROR: astropy/astropy-healpix unavailable:", exc, file=sys.stderr)
        return 3

    props = {}
    for line in (args.snr_root / "properties").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            k,v=line.split("=",1); props[k.strip()]=v.strip()
    order=int(props["hips_order"])
    hp=HEALPix(nside=1<<order, order="nested", frame="icrs")

    rx=re.compile(r"Norder(\d+)[/\\]Dir(\d+)[/\\]Npix(\d+)\.tsv$")
    rows=wrong=dup=0
    ids=set()
    actual_cells=set()
    expected_cells=set()

    for p in args.snr_root.rglob("Npix*.tsv"):
        m=rx.search(str(p))
        if not m:
            print("BAD_PATH",p); wrong+=1; continue
        file_order=int(m.group(1))
        file_ipix=int(m.group(2))*10000 + int(m.group(3))  # Dir*10000 + Npix
        if file_order != order:
            print("BAD_ORDER", p); wrong+=1; continue
        actual_cells.add(file_ipix)
        with p.open("r",encoding="utf-8") as f:
            for line in f:
                if not line.strip() or line.startswith("#"):
                    continue
                parts=line.split()
                if len(parts)<6:
                    print("BAD_ROW",p,line.strip()); wrong+=1; continue
                sid=int(parts[0]); ra=float(parts[1]); dec=float(parts[2])
                expected=int(hp.lonlat_to_healpix(ra*u.deg, dec*u.deg))
                expected_cells.add(expected)
                rows+=1
                if sid in ids: dup+=1
                ids.add(sid)
                if expected != file_ipix:
                    wrong+=1
                    if wrong <= 20:
                        print("WRONG_TILE", sid, ra, dec, "file",file_ipix,"expected",expected)
    print("rows=",rows)
    print("wrong_tile_rows=",wrong)
    print("duplicate_ids=",dup)
    print("actual_tile_count=",len(actual_cells))
    print("expected_tile_count=",len(expected_cells))
    return 0 if rows>0 and wrong==0 and dup==0 and actual_cells==expected_cells else 1

if __name__ == "__main__":
    raise SystemExit(main())
