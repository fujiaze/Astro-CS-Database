
import csv, re, pathlib
repo = pathlib.Path(".")
pat = re.compile(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', re.M)
globs = ("lib/*/include/**/*.h","lib/*/include/*.h","lib/*/cpp/include/**/*.h","lib/*/*/include/**/*.h","lib/plate_solve/cpp/ipv/include/*.h")
paths=[]
for g in globs:
    for p in repo.rglob(g):
        s=str(p)
        if s.startswith("lib/") or s.startswith("include/"):   # 干净检出等价口径: 排除 run/ 影子树
            paths.append(p)
paths=sorted(set(p for p in paths if "third_party" not in str(p) and "archive" not in str(p)))
syms=set()
for h in paths:
    t=h.read_text(encoding="utf-8",errors="ignore")
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//.*','',t)
    for m in pat.finditer(t): syms.add(m.group(3))
print("干净检出口径 headers:", len(paths), " symbols:", len(syms))
rows=list(csv.DictReader(open("docs/contracts/API_CONTRACTS.csv",encoding="utf-8")))
def present(s): return s in syms or s.split("::")[-1] in syms or any(k.split("::")[-1]==s for k in syms)
missing=sorted({r["symbol"].strip() for r in rows if r["symbol"].strip() and not present(r["symbol"].strip())})
print("API-MISSING-AST 候选行数:", len(missing)); print("清单:", missing[:20])
inv=[l.split(',')[0] for l in open("docs/architecture/api_inventory.csv",encoding="utf-8").read().splitlines()[1:] if l]
print("api_inventory 条目:", len(inv))

