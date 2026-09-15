import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
SYSNEED = {
  "uint8_t":"stdint.h","uint16_t":"stdint.h","uint32_t":"stdint.h","uint64_t":"stdint.h",
  "int8_t":"stdint.h","int16_t":"stdint.h","int32_t":"stdint.h","int64_t":"stdint.h",
  "size_t":"stddef.h","ptrdiff_t":"stddef.h","NULL":"stddef.h",
  "FILE:":"stdio.h"
}
TYPES = ["uint8_t","uint16_t","uint32_t","uint64_t","int8_t","int16_t","int32_t","int64_t","size_t","ptrdiff_t"]
roots = ["include", "lib/astro_image_io/include", "lib/calibration/include", "lib/cosmetic/include", "lib/drizzle/include",
         "lib/hips/include", "lib/dynamic_psf/include", "lib/star_detector/include", "lib/gaia_xpsd_client/include",
         "lib/gaia_xpsd_client/src", "lib/plate_solve/cpp/ipv/include", "lib/snr_estimator/cpp/include",
         "lib/photometric_calib/cpp/include", "providers/cpu/common/include", "providers/cpu/baseline/include",
         "providers/cpu/avx2/include", "providers/cpu/avx512/include", "lib/common/include", "modules/services/io/include"]
def resolve(inc, from_dir):
    cand = os.path.normpath(os.path.join(from_dir, inc))
    if os.path.exists(cand): return cand
    for r in roots:
        cand = os.path.normpath(os.path.join(r, inc))
        if os.path.exists(cand): return cand
    if os.path.exists(inc): return inc
    return None

QUOTED = re.compile(r"^[ \t]*#include[ \t]*\"([^\"]+)\"", re.M)
SYSINC = re.compile(r"^[ \t]*#include[ \t]*<([^>]+)>", re.M)

def closure(h, seen):
    if h in seen: return set()
    seen.add(h)
    txt = open(h, encoding="utf-8", errors="replace").read()
    s = set(x for x in SYSINC.findall(txt))
    for m in QUOTED.findall(txt):
        r = resolve(m, os.path.dirname(h))
        if r: s |= closure(r, seen)
    return s

pub = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","include/**"], capture_output=True, text=True).stdout.split(chr(0)) if p]
print("=== include/astrocs 公共头独立编译所需系统头缺口（口径 W6-P：只判 stdint/stddef 型） ===")
n = 0
for h in pub:
    txt = open(h, encoding="utf-8", errors="replace").read()
    used = [t for t in TYPES if re.search(r"\b" + t + r"\b", txt)]
    if not used: continue
    need = set()
    if any(t.startswith("u") or t.startswith("i") for t in used): need.add("stdint.h")
    if "size_t" in used or "ptrdiff_t" in used: need.add("stddef.h")
    own = set(SYSINC.findall(txt))
    trans = closure(h, set())
    missing = sorted(x for x in need if x not in own and x not in trans)
    if missing:
        n += 1
        print("   %-52s 用 %s 但缺 %s" % (h, used[:4], missing))
print("   小计:", n, "/", len(pub))
print()
print("=== 模块公共头同样判（lib/*/include, providers/*/include, modules/*/*/include） ===")
mod = [p for p in subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","-z","lib/**/include/**","providers/**/include/**","modules/**/include/**","runtime/**/include/**"], capture_output=True, text=True).stdout.split(chr(0)) if p and p.endswith((".h",".hpp"))]
mod = [f for f in mod if "third_party" not in f and "healpix_browser_qt" not in f and "/acr/" not in f]
print("   分母:", len(mod))
n2 = 0
for h in sorted(mod):
    txt = open(h, encoding="utf-8", errors="replace").read()
    used = [t for t in TYPES if re.search(r"\b" + t + r"\b", txt)]
    if not used: continue
    need = set(["stdint.h", "stddef.h"])
    own = set(SYSINC.findall(txt))
    trans = closure(h, set())
    missing = sorted(x for x in need if x not in own and x not in trans)
    if missing:
        n2 += 1
        if n2 <= 25:
            print("   %-64s 缺 %s" % (h, missing))
print("   小计:", n2)