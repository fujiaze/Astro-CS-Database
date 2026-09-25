import re
import sys

sys.stdout.reconfigure(encoding="utf-8")

p = r"独立审计/证据/通读-CR-37.md"
txt = open(p, encoding="utf-8").read()

# 1) finding IDs defined vs referenced
defined = set(re.findall(r"^### (CR-37-\d+)", txt, re.M))
refd = set(re.findall(r"CR-37-\d+", txt))
print("defined findings:", len(defined), sorted(defined))
print("referenced but not defined:", sorted(refd - defined))

# 2) per-section disposition table row counts
sections = re.split(r"^# \d · |^# 本批|^# 我读不动|^# 派单|^# 收工", txt, flags=re.M)
heads = re.findall(r"^# (\d) · `([^`]+)`", txt, re.M)
print("\nsections:", heads)
parts = re.split(r"(?m)^# ", txt)
for part in parts[1:]:
    first = part.splitlines()[0]
    if not first.startswith(tuple("123456")):
        continue
    body = part
    # count table rows in the 2.5 section only (before first 'findings' heading)
    m = re.search(r"§2\.5 数值处置表（(\d+) 行）", body)
    declared = m.group(1) if m else "?"
    seg = body.split("## ")
    tbl = ""
    for s in seg:
        if s.startswith(("1.1", "2.1", "3.1", "4.1", "5.1", "6.1")):
            tbl = s
    rows = [l for l in tbl.splitlines() if l.startswith("|")]
    n = max(len(rows) - 2, 0)  # header + separator
    print(f"{first[:12]:14s} declared={declared:3s} counted_rows={n}")

# 3) PROGRESS marker + checklist
print("\nPROGRESS:", re.findall(r"PROGRESS: (\d/\d)", txt))
print("checklist 已读：否 剩余:", txt.count("已读：否"))
print("checklist 已读：是:", txt.count("已读：是"))
print("\ntotal chars:", len(txt), "lines:", txt.count(chr(10)) + 1)
