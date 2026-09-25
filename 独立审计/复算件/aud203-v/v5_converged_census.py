"""V5: census of convergence-state values in the TRACKED result corpus.

Method (read-only, no repo module import):
  1. `git -C <repo> ls-files`            -> the tracked set (authority for "exists")
  2. `git -C <repo> grep -n -I -o -E ...` -> every key:value occurrence, tracked files only
  3. classify each hit as RESULT (archived data: json/csv/log/txt under
     实验/**/results, artifacts/**, eng/tests/**/evidence*) vs SPEC/CODE/DOC.

Question answered: does the value 2 (stalled) ever appear as an *observed* value in a
tracked result file?
"""
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict

REPO = r"F:/Astro dev/Astro CS Normalization Database"
PAT = (r"converged[a-z_]*[\"' ]*[:=][ ]*[0-9]+"
       r"|stalled[\"' ]*[:=][ ]*[0-9]+")

RESULT_HINTS = ("/results/", "results/", "artifacts/evidence", "artifacts/acceptance",
                "/evidence/", "testdata/")
DATA_EXT = (".json", ".csv", ".log", ".txt", ".md", ".yaml", ".yml")


def tracked_files():
    out = subprocess.run(["git", "-C", REPO, "ls-files", "-z"], capture_output=True)
    return [p.decode("utf-8", "replace") for p in out.stdout.split(b"\0") if p]


def git_grep(pattern):
    out = subprocess.run(
        ["git", "-c", "core.quotePath=false", "-C", REPO,
         "grep", "-n", "-I", "-o", "-E", pattern],
        capture_output=True)
    return out.stdout.decode("utf-8", "replace")


def is_result(path):
    p = path.replace("\\", "/")
    code = p.endswith((".cpp", ".h", ".py"))
    if code:
        return False
    return any(h in p for h in RESULT_HINTS) or "/results/" in p


def main():
    files = tracked_files()
    print("tracked files total: %d" % len(files))
    res = [f for f in files if is_result(f)]
    print("tracked non-code result/evidence/doc files: %d" % len(res))

    txt = git_grep(PAT)
    cur = None
    rows = []
    for line in txt.splitlines():
        line = line.rstrip()
        if not line:
            continue
        if not re.match(r"^[^:]+:[0-9]+:", line):
            cur = line
            continue
        fn, ln, val = line.split(":", 2)
        rows.append((fn, int(ln), val.strip()))
    print("total key:value occurrences (tracked, all file kinds): %d" % len(rows))

    by_kind = defaultdict(list)
    for fn, ln, val in rows:
        kind = "RESULT-DATA" if is_result(fn) else ("CODE/SPEC" if fn.endswith((".cpp", ".h", ".py")) else "DOC")
        m = re.search(r"([0-9]+)$", val)
        num = int(m.group(1)) if m else None
        key = re.match(r"([a-zA-Z_]+)", val).group(1)
        by_kind[kind].append((fn, ln, key, num, val))

    for kind in ("RESULT-DATA", "CODE/SPEC", "DOC"):
        rows2 = by_kind.get(kind, [])
        c = Counter(n for _, _, _, n, _ in rows2)
        print("\n### %s : %d occurrences, value distribution %s"
              % (kind, len(rows2), dict(sorted(c.items()))))
        if kind == "RESULT-DATA":
            per_file = defaultdict(Counter)
            for fn, ln, key, n, val in rows2:
                per_file[fn][n] += 1
            for fn in sorted(per_file):
                print("    %-78s %s" % (fn, dict(sorted(per_file[fn].items()))))
        else:
            for fn, ln, key, n, val in rows2:
                if n in (2, 3):
                    print("    value %d here is spec/code text: %s:%d  %s" % (n, fn, ln, val))

    # explicit: any RESULT occurrence with value 2/3?
    bad = [r for r in by_kind.get("RESULT-DATA", []) if r[3] in (2, 3)]
    print("\n### RESULT-DATA occurrences with value 2 (stalled) or 3 (invalid): %d" % len(bad))
    for fn, ln, key, n, val in bad:
        print("    %s:%d  %s" % (fn, ln, val))

    # also: string-valued state names in tracked results (e.g. "stalled")
    txt2 = git_grep(r'"(stalled|max_iter|invalid|not_converged)"')
    cur = None
    srows = []
    for line in txt2.splitlines():
        line = line.rstrip()
        if not line:
            continue
        if not re.match(r"^[^:]+:[0-9]+:", line):
            cur = line
            continue
        fn, ln, val = line.split(":", 2)
        srows.append((fn, int(ln), val.strip()))
    print("\n### string state names in tracked corpus: %d occurrences" % len(srows))
    per = defaultdict(Counter)
    for fn, ln, val in srows:
        if is_result(fn):
            per[fn][val.strip('"')] += 1
    for fn in sorted(per):
        print("    %-78s %s" % (fn, dict(per[fn])))


if __name__ == "__main__":
    sys.setrecursionlimit(10000)
    main()
