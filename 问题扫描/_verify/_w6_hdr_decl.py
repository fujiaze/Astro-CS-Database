import os, re, json, subprocess, collections
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
INT_EQ = ({"stdint.h","cstdint"}, {"stddef.h","cstddef"})

def resolve(inc, from_dir):
    for base in (from_dir,) + tuple(roots):
        cand = os.path.normpath(os.path.join(base, inc))
        if os.path.exists(cand): return cand
    return os.path.exists(inc) and inc or None

def closure(h, seen):
    if h in seen: return set()
    seen.add(h)
    txt = open(h, encoding="utf-8", errors="replace").read()
    s = set(SYSINC.findall(txt))
    for m in QUOTED.findall(txt):
        r = resolve(m, os.path.dirname(h))
        if r: s |= closure(r, seen)
    return s

DECL = re.compile(r"\btypedef\s+(?:struct|union|enum)?\s*([A-Za-z_]\w*)?\s*(?:\{[^}]*\}\s*)?([A-Za-z_]\w*)\s*;|\benum\s+([A-Za-z_]\w*)\s*\{|\b#define\s+([A-Z_][A-Z0-9_]*)", re.M)
declmap = collections.defaultdict(set)
pubfiles = []
for r in roots:
    for dirpath, dirs, fs in os.walk(r):
        for f in fs:
            if f.endswith((".h", ".hpp")):
                pubfiles.append(os.path.join(dirpath, f))
pubfiles = sorted(set(pubfiles))
code_of = {}
for h in pubfiles:
    txt = open(h, encoding="utf-8", errors="replace").read()
    body = re.sub(r"/\*.*?\*/", " ", txt, flags=re.S)
    body = re.sub(r"//[^\n]*", " ", body)
    code_of[h] = body
    for m in DECL.finditer(body):
        for g in (m.group(2), m.group(3), m.group(4)):
            if g and g not in ("struct","union","enum"):
                declmap[g].add(h)

print("口径 W6-Q：公共头面分母 =", len(pubfiles), "| 可解析符号声明数 =", len(declmap))
viol = []
for h in pubfiles:
    body = code_of[h]
    own = closure(h, set())
    needed = collections.defaultdict(set)
    for tok in set(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", body)):
        ds = declmap.get(tok)
        if not ds: continue
        ext = [d for d in ds if d != h]
        if not ext: continue
        covered = any(e in own for e in ext) or (h in ds)
        if not covered:
            needed[tok] = set(ext)
    if needed:
        viol.append((h, needed))
print("含「用了别处声明的符号但自己的 include 闭包里拿不到」的公共头 =", len(viol))
for h, needed in sorted(viol):
    top = sorted(needed.items(), key=lambda kv: -len(kv[0]))[:6]
    srcs = sorted({s for v in needed.values() for s in v})
    print("   %-58s 缺声明源 %s" % (h, srcs[:3]))
    print("        符号样例:", [k for k, _ in top][:8])