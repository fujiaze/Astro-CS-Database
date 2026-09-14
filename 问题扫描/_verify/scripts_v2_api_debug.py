
import re, pathlib
repo = pathlib.Path(".")
pat = re.compile(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', re.M)
paths = []
for g in ("lib/*/include/**/*.h","lib/*/include/*.h","lib/*/cpp/include/**/*.h","lib/*/*/include/**/*.h","lib/plate_solve/cpp/ipv/include/*.h"):
    paths += list(repo.rglob(g))
paths = [p for p in set(paths) if "third_party" not in str(p) and "archive" not in str(p)]
hits = []
for h in paths:
    t = h.read_text(encoding="utf-8", errors="ignore")
    t2 = re.sub(r'/\*.*?\*/','',t,flags=re.S); t2 = re.sub(r'//.*','',t2)
    for m in pat.finditer(t2):
        if m.group(3) == "estimate_mag_lim_by_density":
            hits.append((str(h), m.group(0)[:160]))
print("命中文件:")
for f,s in hits: print("  ", f, "|", s.replace("\n","\\n"))
# 同时看 ipv_select.h 自身的解析结果
h = pathlib.Path("lib/plate_solve/cpp/ipv/include/ipv_select.h")
t = h.read_text(encoding="utf-8")
print("去注释前是否含该词:", "estimate_mag_lim_by_density" in t)
t2 = re.sub(r'/\*.*?\*/','',t,flags=re.S); t2 = re.sub(r'//.*','',t2)
print("去注释后是否含该词:", "estimate_mag_lim_by_density" in t2)
print("该头解析出的函数名:", sorted({m.group(3) for m in pat.finditer(t2)}))

