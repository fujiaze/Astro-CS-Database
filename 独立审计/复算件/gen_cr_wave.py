"""Generate the next CR wave inventories: tracked lib sources + packaging config,
excluding files already assigned to CR-01..CR-34, prioritised by science-bearing modules."""
import subprocess, os, sys, collections
sys.stdout.reconfigure(encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))          # 独立审计/复算件
ROOT = os.path.dirname(HERE)                                # 产出
OUT = os.path.join(ROOT, "inventory")
REPO = r"F:\Astro dev\Astro CS Normalization Database"

def git(*a):
    return subprocess.run(["git", "-C", REPO, "-c", "core.quotePath=false", *a],
                          capture_output=True, text=True, encoding="utf-8").stdout

tracked = [f for f in git("ls-files").splitlines() if f.strip()]
SRC_EXT = (".cpp", ".h", ".hpp", ".c", ".cc")
universe = [f for f in tracked if (f.startswith("lib/") and f.endswith(SRC_EXT))
            or f.startswith("eng/packaging/config")]

assigned = set()
for name in os.listdir(OUT):
    if name.startswith("CR-") and name.endswith(".txt"):
        for line in open(os.path.join(OUT, name), encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#"):
                assigned.add(line.split(":")[0])

remaining = [f for f in universe if f not in assigned]

lc = {}
for f in remaining:
    r = subprocess.run(["git", "-C", REPO, "grep", "-c", "", "--", f],
                       capture_output=True, text=True)
    s = r.stdout.strip()
    lc[f] = int(s.split(":")[-1]) if s and s.split(":")[-1].isdigit() else 0

PRIORITY = [
    "lib/algorithms/drizzle", "lib/algorithms/platesolve", "lib/algorithms/photometry",
    "lib/algorithms/coverage", "lib/algorithms/star_detection", "lib/algorithms/calibration",
    "lib/algorithms/noise_snr", "lib/algorithms/psf", "lib/algorithms/projection",
    "lib/algorithms/resample", "lib/algorithms/integration", "lib/algorithms/shared",
    "lib/algorithms/cosmetic", "lib/algorithms/phase1_session", "lib/algorithms/phase2_session",
    "lib/algorithms/phase3_session", "lib/algorithms/fits_output", "lib/include",
    "lib/infrastructure/scheduler", "lib/infrastructure/cli", "lib/infrastructure/pipeline",
    "lib/infrastructure/gaia_xpsd_client", "lib/infrastructure/observability",
    "eng/packaging/config", "lib/infrastructure/benchmark", "lib/infrastructure/acr",
    "lib/infrastructure/aio", "lib/infrastructure/hips_browser", "lib/third_party",
]

def prio(f):
    d = "/".join(f.split("/")[:3]) if f.startswith("lib") else "eng/packaging/config"
    for i, p in enumerate(PRIORITY):
        if d == p or f.startswith(p + "/"):
            return i
    return len(PRIORITY)

remaining.sort(key=lambda f: (prio(f), f))

MAXL, CHUNK, LINE_CAP = 2100, 6, 3200
start = 35
batches, cur, cur_lines = [], [], 0
for f in remaining:
    n = lc.get(f, 0)
    if n > MAXL:
        if cur:
            batches.append(cur); cur, cur_lines = [], 0
        batches.append([f"{f}:1-{n}"])
        continue
    if cur and (len(cur) >= CHUNK or cur_lines + n > LINE_CAP):
        batches.append(cur); cur, cur_lines = [], 0
    cur.append(f); cur_lines += n
if cur:
    batches.append(cur)

made = 0
for i, b in enumerate(batches):
    p = os.path.join(OUT, f"CR-{start+i}.txt")
    if os.path.exists(p):
        continue
    label = " / ".join(sorted({("/".join(x.split(":")[0].split("/")[:3]) if x.startswith("lib")
                                else "eng/packaging/config") for x in b}))
    total = sum(lc.get(x.split(":")[0], 0) for x in b)
    with open(p, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(f"# 批次 CR-{start+i}｜{label}｜{len(b)} 项（整读＋逐数值处置表；合计约 {total} 行）\n")
        for x in b:
            fh.write(x + "\n")
    made += 1
print("new batch files:", made, "| total planned:", len(batches))
print("unassigned before this wave:", len(remaining), "| universe:", len(universe))
print("first 12:", ", ".join(f"CR-{start+j}" for j in range(min(12, len(batches)))))
