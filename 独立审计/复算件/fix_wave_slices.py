"""Fix the generated wave: vendor code must not be line-audited, and oversized first-party
files must be sliced to lane-sized chunks. Rewrites/deletes only files I generated."""
import sys, os, re, glob, subprocess, collections
sys.stdout.reconfigure(encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))
INV = os.path.join(os.path.dirname(HERE), "inventory")
REPO = r"F:\Astro dev\Astro CS Normalization Database"


def lines_of(f):
    r = subprocess.run(["git", "-C", REPO, "grep", "-c", "", "--", f],
                       capture_output=True, text=True)
    s = r.stdout.strip()
    return int(s.split(":")[-1]) if s and s.split(":")[-1].isdigit() else 0


VENDOR = re.compile(r'(lib/third_party/|lib/infrastructure/aio/third_party/'
                    r'|/legacy/healpix_stack/|/archive/legacy/)')
SLICE_TARGET = 1500

assign = {}
for p in glob.glob(os.path.join(INV, "CR-*.txt")):
    b = os.path.basename(p)[:-4]
    for line in open(p, encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#"):
            assign.setdefault(line.split(":")[0], []).append(b)

vendor_files = sorted({f for f in assign if VENDOR.search(f)})
huge = sorted({f for f in assign if not VENDOR.search(f) and lines_of(f) > 3000},
              key=lambda f: -lines_of(f))
print("vendor files:", len(vendor_files), "| first-party >3000 lines:", len(huge))

used = {int(os.path.basename(p)[3:-4]) for p in glob.glob(os.path.join(INV, "CR-*.txt"))}


def nxt(start):
    n = start
    while n in used:
        n += 1
    used.add(n)
    return n


def drop_batches(files):
    """Remove entries from existing batch files; delete the batch if it becomes empty."""
    gone = []
    for f in files:
        for b in assign.get(f, []):
            p = os.path.join(INV, f"CR-{b}.txt")
            if not os.path.exists(p):
                continue
            keep = [l.strip() for l in open(p, encoding="utf-8")
                    if l.strip() and not l.strip().startswith("#")
                    and l.strip().split(":")[0] != f]
            if keep:
                hdr = [l.strip() for l in open(p, encoding="utf-8") if l.strip().startswith("#")]
                with open(p, "w", encoding="utf-8", newline="\n") as fh:
                    fh.write(hdr[0].replace("整读", "整读") + "\n")
                    fh.writelines(k + "\n" for k in keep)
            else:
                os.remove(p)
                gone.append(b)
    return gone


# 1. vendor -> triage batches (no per-value cards)
drop_batches(vendor_files)
for i in range(0, len(vendor_files), 10):
    grp = vendor_files[i:i + 10]
    b = nxt(260 + i // 10)
    with open(os.path.join(INV, f"CR-{b}.txt"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write(f"# 批次 CR-{b}｜第三方引入件**溯源登记**批（不逐值立卡）｜{len(grp)} 个文件\n")
        fh.writelines(g + "\n" for g in grp)
print("vendor triage batches written:", (len(vendor_files) + 9) // 10)

# 2. oversized first-party -> slices of ~1500 lines
for f in huge:
    drop_batches([f])
    n = lines_of(f)
    k = (n + SLICE_TARGET - 1) // SLICE_TARGET
    ids = [nxt(240) for _ in range(k)]
    for j, b in enumerate(ids):
        lo = j * SLICE_TARGET + 1
        hi = min(n, (j + 1) * SLICE_TARGET)
        with open(os.path.join(INV, f"CR-{b}.txt"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(f"# 批次 CR-{b}｜超大第一方文件按行段分片｜{f} 的 {lo}-{hi} 行"
                     f"（全文件 {n} 行，拆成 {k} 段，跨段结论须读完全部段才下）\n")
            fh.write(f"{f}:{lo}-{hi}\n")
    print(f"  {f} ({n} 行) -> {k} 段: CR-{ids[0]}..CR-{ids[-1]}")

print("inventory now:", len(glob.glob(os.path.join(INV, 'CR-*.txt'))), "batches")
