#!/usr/bin/env python3
"""Parse XISF XML headers of synced master frames (Control §5) - pure python."""
import os, re, struct, json, hashlib

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
TD = os.path.join(ROOT, "testdata", "T4 calibration files")
out = {}
for fn in sorted(os.listdir(TD)):
    p = os.path.join(TD, fn)
    with open(p, "rb") as f:
        magic = f.read(8)
        head = f.read(60000)  # enough for header XML
    txt = head.decode("utf-8", "ignore")
    geom = re.search(r'geometry="([^"]+)"', txt)
    fmt = re.search(r'sampleFormat="([^"]+)"', txt)
    cs = re.search(r'colorSpace="([^"]+)"', txt)
    bounds = re.search(r'bounds="([^"]+)"', txt)
    loc = re.search(r'location="([^"]+)"', txt)
    def kw(name):
        m = re.search(r'name="%s" value="([^"]*)"' % name, txt)
        return m.group(1) if m else None
    info = {
        "magic": magic.decode("latin1"),
        "geometry": geom.group(1) if geom else None,
        "sampleFormat": fmt.group(1) if fmt else None,
        "colorSpace": cs.group(1) if cs else None,
        "bounds": bounds.group(1) if bounds else None,
        "attachment": loc.group(1) if loc else None,
        "IMAGETYP": kw("IMAGETYP"), "XBINNING": kw("XBINNING"), "EXPTIME": kw("EXPTIME"),
        "FILTER": kw("FILTER"), "INSTRUME": kw("INSTRUME"), "XPIXSZ": kw("XPIXSZ"),
        "size_bytes": os.path.getsize(p),
        "sha256": hashlib.sha256(open(p,"rb").read()).hexdigest(),
    }
    out[fn] = info
    print("=== %s ===" % fn[:45])
    for k, v in info.items():
        print("  %-14s %s" % (k, v))
    print()
open(os.path.join(ROOT, "package", "03_testdata", "masters_xisf_manifest.json"), "w").write(
    json.dumps(out, indent=2, ensure_ascii=False))
print("masters_xisf_manifest.json written")
