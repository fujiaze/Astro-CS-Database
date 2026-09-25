import io, os, re, glob

BASE = r"独立审计/证据"
out = io.open(r"独立审计/复算件/d4merge/outlines.txt", "w", encoding="utf-8")
files = sorted(glob.glob(os.path.join(BASE, "AUD-20*.md")) + glob.glob(os.path.join(BASE, "复核-AUD20*.md"))
               + glob.glob(os.path.join(BASE, "复核-*.md")))
out.write("FILE LIST:\n")
for f in files:
    out.write("  %s\n" % os.path.basename(f))
for f in files:
    t = io.open(f, encoding="utf-8", errors="replace").read()
    lines = t.split("\n")
    out.write("\n########## %s  (%d lines, %d chars)\n" % (os.path.basename(f), len(lines), len(t)))
    for i, l in enumerate(lines, 1):
        if re.match(r"^#{1,4} ", l):
            out.write("%5d: %s\n" % (i, l[:160]))
out.close()
print("ok", len(files))
