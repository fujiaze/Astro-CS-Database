
import os, re, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)

def c_struct(path, name):
    txt = open(path, encoding="utf-8", errors="replace").read()
    for mm in re.finditer(r"typedef\s+struct(?:\s+\w+)?\s*\{", txt):
        i = txt.find("{", mm.start()); d = 0; j = i
        while j < len(txt):
            if txt[j] == "{": d += 1
            elif txt[j] == "}":
                d -= 1
                if d == 0: break
            j += 1
        tail = txt[j+1:j+140]
        nm = re.match(r"\s*(\w+)\s*;", tail)
        if nm and nm.group(1) == name:
            return (txt[:mm.start()].count(chr(10)) + 1, txt[i+1:j])
    return None

def py_fields(path, cls):
    txt = open(path, encoding="utf-8", errors="replace").read()
    m = re.search(r"class\s+" + re.escape(cls) + r"\s*\(\s*(?:ctypes\.)?Structure\s*\)\s*:", txt)
    if not m: return None
    i = txt.find("{", m.end()-1); d = 0; j = i
    while j < len(txt):
        if txt[j] == "{": d += 1
        elif txt[j] == "}":
            d -= 1
            if d == 0: break
        j += 1
    body = txt[i+1:j]
    out = []
    for grp in re.findall(r"\((.*?)\)", body, re.S):
        g = re.sub(r"\s+", " ", grp).strip()
        parts = g.split(",", 1)
        if len(parts) != 2: continue
        nm = parts[0].strip().strip(chr(34) + chr(39))
        ty = parts[1].strip()
        if re.fullmatch(r"[A-Za-z_]\w*", nm): out.append((nm, ty))
    return (txt[:m.start()].count(chr(10)) + 1, out)

PAIRS = [
  ("lib/plate_solve/cpp/ipv/include/ipv_api.h", "IpvParams", "lib/plate_solve/tools/ipv_abi_mirror.py", "IpvParams"),
  ("lib/plate_solve/cpp/ipv/include/ipv_api.h", "IpvWcsResult", "lib/plate_solve/tools/ipv_abi_mirror.py", "IpvWcsResult"),
  ("lib/astro_image_io/include/aio_hips.h", "AstroSphereTileView", "lib/astro_image_io/tests/hips_mapping_oracle.py", "AstroSphereTileView"),
  ("lib/astro_image_io/include/aio_hips.h", "AioHipsSnrPoint", "lib/astro_image_io/tests/v5_snr_precision_roundtrip.py", "AioHipsSnrPoint"),
  ("lib/gaia_xpsd_client/src/gaia_client.h", "GaiaSpectrumStar", "lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py", "GaiaSpectrumStar"),
  ("lib/star_detector/include/star_detector.h", "SDetParams", "lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py", "SDetParams"),
  ("lib/dynamic_psf/include/dynamic_psf.h", "DPSFFitParams", "lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py", "DPSFFitParams"),
  ("lib/dynamic_psf/include/dynamic_psf.h", "DPSFFitResult", "lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate1_psf_final_test.py", "DPSFFitResult"),
]
for cf, cn, pf, pn in PAIRS:
    cs = c_struct(cf, cn); pyf = py_fields(pf, pn)
    print("=" * 96)
    print("C:", cf + ":" + str(cs[0] if cs else "?"), cn, "  PY:", pf + ":" + str(pyf[0] if pyf else "?"), pn)
    if not cs or not pyf:
        print("   PARSE-FAIL", "C" if not cs else "", "PY" if not pyf else "")
        continue
    decls = []
    for raw in cs[1].split(chr(10)):
        s = raw.split("//")[0].strip()
        if not s or s.startswith("#") or s.startswith("/*") or s.startswith("*"): continue
        decls.append(re.sub(r"\s+", " ", s))
    decls = [d for d in decls if not d.endswith("{")]
    print("   C decls:", len(decls), " PY fields:", len(pyf[1]))
    for k in range(max(len(decls), len(pyf[1]))):
        d = decls[k] if k < len(decls) else "--"
        p = pyf[1][k] if k < len(pyf[1]) else ("--", "--")
        mark = "" if (d != "--" and p[0] != "--" and p[0] in d) else "   <<< 不对位"
        print("     [%02d] C: %-46s | PY: %-32s%s" % (k, d[:46], str(p), mark))
