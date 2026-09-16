import os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
roots = ["include", "lib/astro_image_io/include", "lib/calibration/include", "lib/cosmetic/include", "lib/drizzle/include",
         "lib/hips/include", "lib/dynamic_psf/include", "lib/star_detector/include", "lib/snr_estimator/cpp/include",
         "lib/photometric_calib/cpp/include", "lib/plate_solve/cpp/ipv/include", "providers/cpu/common/include",
         "providers/cpu/baseline/include", "providers/cpu/avx2/include", "providers/cpu/avx512/include",
         "lib/common/include", "modules/services/io/include", "runtime/module_loader", "runtime/registry", "lib/gaia_xpsd_client/src"]
QUOTED = re.compile(r"^[ \t]*#include[ \t]*\"([^\"]+)\"", re.M)
SYSINC = re.compile(r"^[ \t]*#include[ \t]*<([^>]+)>", re.M)
def resolve(inc, from_dir):
    for base in (from_dir,) + tuple(roots):
        c = os.path.normpath(os.path.join(base, inc))
        if os.path.exists(c): return c
    return None
def closure(h, seen):
    if h in seen: return set()
    seen.add(h)
    t = open(h, encoding="utf-8", errors="replace").read()
    s = set(SYSINC.findall(t))
    for m in QUOTED.findall(t):
        r = resolve(m, os.path.dirname(h))
        if r: s |= closure(r, seen)
    return s
for h in ["include/astrocs/abi/artifact_api_v1.h", "lib/snr_estimator/cpp/include/snr_estimator.h",
          "modules/services/io/include/astrocs/io/hips_input_v1.h", "runtime/registry/module_registry.h"]:
    t = open(h, encoding="utf-8", errors="replace").read()
    print("="*90)
    print("FILE", h)
    print("  quoted:", QUOTED.findall(t))
    print("  closure(repo-relative):", sorted(x for x in closure(h, set()) if "/" in x))
print("="*90)
print("HioSnrControlPoint 声明处:", subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","HioSnrControlPoint"], capture_output=True, text=True).stdout.split())
print("acs_fio_trace_hooks_v1 声明处:", subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","acs_fio_trace_hooks_v1"], capture_output=True, text=True).stdout.split())