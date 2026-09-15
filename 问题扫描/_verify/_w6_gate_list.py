import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
rows = []
for c in checks:
    cmd = [x for x in c["command"] if isinstance(x, str)]
    entry = " ".join(cmd[:5])
    rows.append((c["id"], ",".join(c["profiles"]), c["platform"], c["waivable"], entry))
for r in rows:
    print("%-40s | %-32s | %-8s | waiver=%-5s | %s" % (r[0], r[1], r[2], r[3], r[4][:104]))