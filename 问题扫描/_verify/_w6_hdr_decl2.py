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
def resolve(inc, from_dir):
    for base in (from_dir,) + tuple(roots):
        c = os.path.normpath(os.path.join(base, inc))
        if os.path.isfile(c): return c
    return None
def closure(h, seen):
    if h in seen: return set()
    seen.add(h)
    t = open(h, encoding="utf-8", errors="replace").read()
    s = set("<"+x+">" for x in SYSINC.findall(t))
    for m in QUOTED.findall(t):
        r = resolve(m, os.path.dirname(h))
        if r:
            s.add(r)
            s |= closure(r, seen)
        else:
            s.add("UNRESOLVED:" + m)
    return s
DECL = re.compile(r"\btypedef\s+(?:struct|union|enum)?\s*([A-Za-z_]\w*)?\s*\{[^}]*\}\s*([A-Za-z_]\w*)\s*;|\btypedef\s+(?:struct|union|enum)\s+([A-Za-z_]\w*)\s*;|\benum\s+([A-Za-z_]\w*)\s*\{|#define\s+([A-Z_][A-Z0-9_]{3,})", re.M)
pubfiles = []
for r in roots:
    for dp, ds, fs in os.walk(r):
        if "third_party" in dp: continue
        for f in fs:
            if f.endswith((".h",".hpp")): pubfiles.append(os.path.join(dp,f))
pubfiles = sorted(set(pubfiles))
code = {}
declmap = collections.defaultdict(set)
for h in pubfiles:
    t = open(h, encoding="utf-8", errors="replace").read()
    b = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    b = re.sub(r"//[^\n]*", " ", b)
    code[h] = b
    for m in DECL.finditer(b):
        for g in (m.group(2), m.group(3), m.group(4), m.group(5)):
            if g: declmap[g].add(h)
print("口径 W6-Q2 公共头面分母:", len(pubfiles), "| 符号:", len(declmap))
print()
print("=== A) 引号 include 悬空（在任何候选 -I 根下都找不到） ===")
n = 0
for h in pubfiles:
    for m in QUOTED.findall(code[h]):
        if resolve(m, os.path.dirname(h)) is None:
            n += 1
            print("   %-58s:%-4d -> %s" % (h, h and code[h][:code[h].find(chr(34)+m+chr(34))].count(chr(10))+1, m))
print("   小计:", n)
print()
print("=== B) 使用了他处声明的符号但自身 include 闭包拿不到（= 不能独立编译） ===")
viol = []
for h in pubfiles:
    own = closure(h, set())
    need = collections.defaultdict(set)
    for tok in set(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", code[h])):
        ds = declmap.get(tok)
        if not ds or (h in ds): continue
        if not any(e in own for e in ds):
            need[tok] = set(ds)
    if need:
        viol.append((h, need))
print("   违规头数:", len(viol), "/", len(pubfiles))
for h, need in sorted(viol):
    srcs = sorted({s for v in need.values() for s in v})
    print("   %-60s 缺 %s" % (h, srcs[:2]))
    print("        符号:", sorted(need)[:8])
print()
print("=== C) 系统整型头闭合（stdint/stddef 等价类） ===")
EQ = {"stdint.h": {"stdint.h","cstdint"}, "stddef.h": {"stddef.h","cstddef"}}
TYPES = ["uint8_t","uint16_t","uint32_t","uint64_t","int8_t","int16_t","int32_t","int64_t","size_t","ptrdiff_t","intptr_t","uintptr_t"]
cnt = 0
for h in pubfiles:
    b = code[h]
    used = [t for t in TYPES if re.search(r"\b"+t+r"\b", b)]
    if not used: continue
    own = closure(h, set())
    miss = []
    for need, alts in EQ.items():
        if need == "stdint.h" and not any(t.endswith("_t") and t not in ("size_t","ptrdiff_t") for t in used): continue
        if need == "stddef.h" and not any(t in ("size_t","ptrdiff_t") for t in used): continue
        if not any(("<"+a+">") in own or a in own for a in alts): miss.append(need)
    if miss:
        cnt += 1
        if cnt <= 20: print("   %-58s 用 %s 缺 %s" % (h, used[:3], miss))
print("   小计:", cnt)
json.dump({"B": [[h, sorted(need)] for h, need in viol], "C_count": cnt}, open(os.path.join(HERE,"_w6_hdr_result.json"),"w",encoding="utf-8"), ensure_ascii=False, indent=1)