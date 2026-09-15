
import os, re, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
def rd(p): return open(p, encoding="utf-8", errors="replace").read()

print("=== 1) lib/snr_estimator/tests/p1noise/CMakeLists.txt 是否定义 p1noise_under_test / astrocs_p1_noise ===")
t = rd("lib/snr_estimator/tests/p1noise/CMakeLists.txt")
for pat in [r"add_library\s*\(\s*(\w+)", r"add_executable\s*\(\s*(\w+)", r"add_test\s*\(\s*NAME\s+(\S+)"]:
    print("   ", pat, "->", sorted(set(re.findall(pat, t))))
print()
print("=== 2) AstroSphereTileView / AioHipsSnrPoint 全副本字段数 ===")
files = [l.split(":",1)[0] for l in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-l","AstroSphereTileView"], capture_output=True, text=True).stdout.splitlines()]
for f in files:
    if not f.endswith(".py"): continue
    txt = rd(f)
    for cls in ("AstroSphereTileView","AioHipsSnrPoint"):
        m = re.search(r"class\s+" + cls + r"\s*\(\s*(?:ctypes\.)?Structure\s*\)\s*:\s*(?:\n|\s)*_fields_\s*=\s*\[", txt)
        if not m: continue
        i = txt.find("[", m.end()-1); d=0; j=i
        while j < len(txt):
            if txt[j]=="[": d+=1
            elif txt[j]=="]":
                d-=1
                if d==0: break
            j+=1
        body = txt[i:j]
        names = re.findall(chr(34)+r"([A-Za-z_]\w*)"+chr(34), body)
        ln = txt[:m.start()].count(chr(10))+1
        print("   %-56s:%-4d %-20s n=%d %s" % (f, ln, cls, len(names), names))
print()
print("=== 3) GaiaSpectrumStar 全副本字段数 ===")
files = [l.split(":",1)[0] for l in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-l","GaiaSpectrumStar"], capture_output=True, text=True).stdout.splitlines()]
for f in files:
    if not f.endswith(".py"): continue
    txt = rd(f)
    m = re.search(r"class\s+GaiaSpectrumStar\s*\(\s*(?:ctypes\.)?Structure\s*\)\s*:\s*(?:\n|\s)*_fields_\s*=\s*\[", txt)
    if not m: 
        print("   (无镜像类)", f); continue
    i = txt.find("[", m.end()-1); d=0; j=i
    while j < len(txt):
        if txt[j]=="[": d+=1
        elif txt[j]=="]":
            d-=1
            if d==0: break
        j+=1
    names = re.findall(chr(34)+r"([A-Za-z_]\w*)"+chr(34), txt[i:j])
    print("   %-56s:%-4d n=%d %s" % (f, txt[:m.start()].count(chr(10))+1, len(names), names))
print()
print("=== 4) dpsf_fit_batch_f32 两处 Python 调用实参个数 ===")
for f, rng in [("lib/plate_solve/tools/diag_gaia_psf_projection.py",(205,225)), ("lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py",(155,172))]:
    txt = rd(f).splitlines()
    print("   ---", f, rng)
    for k in range(rng[0]-1, min(rng[1], len(txt))):
        print("     %4d %s" % (k+1, txt[k][:118]))
print()
print("=== 5) 这些镜像脚本的门采集面 ===")
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
for f in ["hips_direct_smoke.py","v5_maptile_oracle.py","hips_mapping_oracle.py","v5_snr_precision_roundtrip.py","diag_gaia_psf_projection.py","gate1_psf_final_test.py","gate2_psf_oracle.py","ipv_abi_mirror.py"]:
    gids = [c["id"] for c in checks if f in json.dumps(c)]
    o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-l",f], capture_output=True, text=True).stdout.splitlines()
    o = [x for x in o if not x.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/"))]
    print("   %-34s checks.json 引用: %s | 仓内引用文件: %s" % (f, gids or "无", o[:6]))
