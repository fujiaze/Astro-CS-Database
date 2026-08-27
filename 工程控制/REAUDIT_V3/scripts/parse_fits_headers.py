#!/usr/bin/env python3
"""Parse FITS primary headers of the 32 R light frames (pure stdlib) and update testdata_manifest.csv with real parsed values."""
import csv, os, struct

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
PKG = os.path.join(ROOT, "package", "03_testdata")
manifest_path = os.path.join(PKG, "testdata_manifest.csv")

def read_fits_header(path, nrecords=200):
    """Return dict of FITS header keywords from first 80-byte records (primary HDU)."""
    kw = {}
    with open(path, "rb") as f:
        raw = f.read(80 * nrecords)
    for i in range(0, len(raw) - 79, 80):
        rec = raw[i:i+80].decode("ascii", errors="replace")
        if rec.startswith("END"):
            break
        key = rec[:8].strip()
        if not key or key == "" or key.startswith("COMMENT") or key.startswith("HISTORY"):
            continue
        val = rec[8:].strip()
        if val.startswith("="):
            val = val[1:].strip()
        kw[key] = val
    return kw

def get_val(kw, key, default=None):
    v = kw.get(key)
    if v is None: return default
    v = v.split("/")[0].strip()
    v = v.strip("\'\" ")
    try: return int(v)
    except Exception: pass
    try: return float(v)
    except Exception: pass
    return v

rows = list(csv.DictReader(open(manifest_path, encoding="utf-8")))
updated = 0
for r in rows:
    lp = os.path.join(ROOT, r["relative_path"]) if r.get("relative_path") else None
    if not lp or not os.path.isfile(lp):
        continue
    ext = os.path.splitext(lp)[1].lower()
    if ext == ".fts":
        try:
            kw = read_fits_header(lp)
            w = get_val(kw, "NAXIS1"); h = get_val(kw, "NAXIS2")
            bp = get_val(kw, "BITPIX")
            r["width"] = str(w); r["height"] = str(h)
            r["bitpix_or_dtype"] = "BITPIX=" + str(bp)
            r["exposure"] = str(get_val(kw, "EXPTIME", ""))
            r["gain"] = str(get_val(kw, "GAIN", get_val(kw, "EGAIN", "")))
            r["temperature"] = str(get_val(kw, "SET-TEMP", get_val(kw, "CCD-TEMP", "")))
            r["date_obs"] = str(get_val(kw, "DATE-OBS", ""))
            c1 = get_val(kw, "CTYPE1", ""); c2 = get_val(kw, "CTYPE2", "")
            r["wcs_present"] = "1" if (c1 and "RA" in str(c1).upper() and c2) else "0"
            r["wcs_ctype"] = str(c1) + "," + str(c2)
            r["wcs_crval"] = str(get_val(kw, "CRVAL1", "")) + "," + str(get_val(kw, "CRVAL2", ""))
            cd = [get_val(kw, "CD1_1", ""), get_val(kw, "CD1_2", ""),
                  get_val(kw, "CD2_1", ""), get_val(kw, "CD2_2", "")]
            r["wcs_cd_or_pc"] = ",".join(str(x) for x in cd)
            updated += 1
        except Exception as e:
            r["transfer_status"] = "FITS_PARSE_ERROR:" + str(e)[:60]
    elif ext == ".xisf":
        # XISF is an XML container; record presence + size (full XML parse out of scope here)
        with open(lp, "rb") as f: head = f.read(512)
        r["wcs_present"] = "n/a(xisf xml)"
        r["bitpix_or_dtype"] = "xisf(xml container; full parse deferred)"
        r["date_obs"] = ""; r["exposure"] = ""

with open(manifest_path, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
print("manifest updated with parsed FITS headers:", updated, "of", len(rows), "rows")

# quick summary of WCS presence
red = [r for r in rows if r["filter"] == "Red"]
wcs_yes = sum(1 for r in red if r.get("wcs_present") == "1")
print("Red frames with parsed WCS:", wcs_yes, "of", len(red))
for r in red[:3]:
    print("sample:", r["filename"], "size=" + r["width"] + "x" + r["height"], "wcs=" + r.get("wcs_present"), "ctype=" + r.get("wcs_ctype"))
