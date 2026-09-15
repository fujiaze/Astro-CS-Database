
import os, re, json, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
def rd(p):
    return open(p, encoding="utf-8", errors="replace").read() if os.path.exists(p) else None

print("=== N-01 复验：hips_direct_smoke.py 是否存在/字段 ===")
p = "lib/astro_image_io/tests/hips_direct_smoke.py"
t = rd(p)
print("exists:", t is not None)
if t:
    for m in re.finditer(r"class\s+(\w+)\s*\((?:ctypes\.)?Structure\)\s*:\s*_fields_\s*=\s*\[(.*?)\]\s*", t, re.S):
        names = re.findall(r"\(" + chr(34) + r"([A-Za-z_]\w*)" + chr(34), m.group(2))
        print("   ", m.group(1), names)

print()
print("=== N-02 复验：AstroSphereTileView 全部镜像副本字段 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","AstroSphereTileView"], capture_output=True, text=True)
for f in [x for x in out.stdout.split(chr(10)) if x]:
    txt = rd(f) or ""
    for m in re.finditer(r"class\s+AstroSphereTileView\s*\((?:ctypes\.)?Structure\)\s*:\s*_fields_\s*=\s*\[(.*?)\n\s*\]", txt, re.S):
        names = re.findall(chr(34) + r"([A-Za-z_]\w*)" + chr(34), m.group(1))
        print("   %-58s fields=%d %s" % (f, len(names), names))

print()
print("=== N-03 复验：GaiaSpectrumStar 全部镜像副本字段 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","GaiaSpectrumStar"], capture_output=True, text=True)
for f in [x for x in out.stdout.split(chr(10)) if x]:
    txt = rd(f) or ""
    for m in re.finditer(r"class\s+GaiaSpectrumStar\s*\((?:ctypes\.)?Structure\)\s*:\s*_fields_\s*=\s*\[(.*?)\n\s*\]", txt, re.S):
        names = re.findall(chr(34) + r"([A-Za-z_]\w*)" + chr(34), m.group(1))
        print("   %-58s fields=%d %s" % (f, len(names), names))

print()
print("=== N-04 复验：dpsf_fit_batch_f32 的 python argtypes 参数个数 vs C 原型 ===")
h = rd("lib/dynamic_psf/include/dynamic_psf.h")
m = re.search(r"dpsf_fit_batch_f32\s*\(([^;]*?)\)\s*;", h, re.S)
cargs = [a.strip() for a in re.sub(r"\s+", " ", m.group(1)).split(",")]
print("   C 原型参数数:", len(cargs))
print("   C 参数:", cargs)
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","-e","dpsf_fit_batch_f32","--","*.py"], capture_output=True, text=True)
print(out.stdout[:1500])
