#!/usr/bin/env python3
"""Parse FITS headers of the synced master frames (Control §5) - pure python, no numpy."""
import os, struct, json

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
TD = os.path.join(ROOT, "testdata")
masters = {
    "masterBias": None, "masterDark-180s": None, "masterFlat-Red": None,
}
for dp, _, fns in os.walk(TD):
    for fn in fns:
        for k in masters:
            if k in fn and fn.endswith((".fts", ".fits", ".fit")):
                masters[k] = os.path.join(dp, fn)
for k, p in sorted(masters.items()):
    if not p:
        continue
    with open(p, "rb") as f:
        hdr = f.read(2880)
    cards = []
    # parse 80-char cards until END
    for i in range(0, 2880, 80):
        c = hdr[i:i+80].decode("latin1", "ignore")
        key = c[:8].strip()
        val = c[10:30].strip() if len(c) > 30 else ""
        cards.append((key, val))
        if key == "END":
            break
    d = dict(cards)
    print("=== %s (%s) ===" % (k, os.path.basename(p)))
    for key in ["BITPIX","NAXIS","NAXIS1","NAXIS2","BZERO","BSCALE","IMAGETYP","EXPTIME","FILTER","CCDNAME","DETECTOR","GAIN","RDNOISE","SATURATE","CRVAL1","CRVAL2","CTYPE1","CTYPE2","INSTRUME","OBSERVAT","DATE-OBS"]:
        if key in d and d[key]:
            print("  %-10s %s" % (key, d[key][:40]))
    print()
