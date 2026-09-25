import pathlib
import sys

sys.stdout.reconfigure(encoding="utf-8")

p = pathlib.Path("独立审计/证据/通读-CR-60.md")
t = p.read_text(encoding="utf-8")

# phase 1: old id -> marker (§N§), longest patterns first
pairs = [
    ("CR-60-05 主位点", "§6§ 主位点"),
    ("CR-60-05 主位点", "§6§ 主位点"),
    ("CR-60-05(甲)", "§6§"),
    ("CR-60-05(乙)", "§7§"),
    ("CR-60-05(丙)", "§8§"),
    ("CR-60-05", "§11§"),
    ("CR-60-07", "§9§"),
    ("CR-60-08", "§10§"),
    ("CR-60-09", "§12§"),
    ("CR-60-1 ", "§1§ "),
    ("CR-60-1）", "§1§）"),
    ("CR-60-1，", "§1§，"),
    ("CR-60-1;", "§1§;"),
    ("CR-60-2", "§5§"),
    ("CR-60-3", "§3§"),
    ("CR-60-4", "§2§"),
    ("CR-60-6", "§4§"),
]
for a, b in pairs:
    t = t.replace(a, b)

# phase 2: markers -> final ids
for n in range(1, 13):
    t = t.replace("§%d§" % n, "CR-60-%d" % n)

p.write_text(t, encoding="utf-8")
import re
print("leftover markers:", re.findall(r"§\d+§|CR-60-0\d", t))
for line_no, line in enumerate(t.splitlines(), 1):
    if "CR-60-" in line and (line.lstrip().startswith("###") or "定级" in line):
        print(line_no, line[:70])
