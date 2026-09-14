
import csv, re, pathlib
repo = pathlib.Path(".")
pat = re.compile(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', re.M)
paths = []
for g in ("lib/*/include/**/*.h","lib/*/include/*.h","lib/*/cpp/include/**/*.h","lib/*/*/include/**/*.h","lib/plate_solve/cpp/ipv/include/*.h"):
    paths += list(repo.rglob(g))
paths = [p for p in set(paths) if "third_party" not in str(p) and "archive" not in str(p)]
syms=set()
for h in paths:
    try: t = h.read_text(encoding="utf-8", errors="ignore")
    except Exception: continue
    t = re.sub(r'/\*.*?\*/','',t,flags=re.S); t = re.sub(r'//.*','',t)
    for m in pat.finditer(t): syms.add(m.group(3))
print("headers:", len(paths), "extracted symbols:", len(syms))
rows = list(csv.DictReader(open("docs/contracts/API_CONTRACTS.csv", encoding="utf-8")))
csvsyms = {r["symbol"].strip() for r in rows}
print("CSV rows:", len(rows))
def present(s):
    return s in syms or s.split("::")[-1] in syms or any(k.split("::")[-1]==s for k in syms)
missing = sorted({r["symbol"].strip() for r in rows if r["symbol"].strip() and not present(r["symbol"].strip())})
print("API-MISSING-AST 候选:", len(missing))
print("其中本批相关:", [m for m in missing if 'mag_lim' in m or 'iterative' in m])
for t in ("estimate_mag_lim_by_density","estimate_mag_lim_iterative","gaia_query_mag_iterative"):
    print(f"  {t}: 在头文件AST={ t in syms } 在CSV清单={ t in csvsyms }")
print("missing 前 30:", missing[:30])

