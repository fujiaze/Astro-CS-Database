import subprocess
import sys

# Which tracked sources are NOT valid UTF-8 (comments unreadable by a UTF-8 scanner)?
REPO = r"F:\Astro dev\Astro CS Normalization Database"
r = subprocess.run(["git", "-C", REPO, "ls-files", "-z"], capture_output=True)
files = [p.decode("utf-8") for p in r.stdout.split(b"\0") if p]
bad = []
import os
n_read = 0
for f in files:
    if not f.endswith((".cpp", ".h", ".hpp", ".cc", ".cxx", ".py", ".json", ".md", ".yaml")):
        continue
    p = os.path.join(REPO, f.replace("/", os.sep))
    try:
        b = open(p, "rb").read()
    except OSError:
        continue
    n_read += 1
    try:
        b.decode("utf-8")
    except UnicodeDecodeError:
        bad.append((f, len(b)))
print("files_checked:", n_read, "not_utf8:", len(bad))
for f, n in bad[:40]:
    print("  ", f, n)
with open(r"独立审计/证据/scan402\not_utf8.tsv", "w",
          encoding="utf-8", newline="\n") as fh:
    for f, n in bad:
        fh.write(f + "\t" + str(n) + "\n")
