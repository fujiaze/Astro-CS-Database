import json, re, glob, os
base = "/workspace/Astro CS Database/设计大纲"
idx = {}
for l in open(os.path.join(base, "_evidence/commits/index.jsonl"), encoding="utf-8"):
    d = json.loads(l); idx[d["seq"]] = d["sha"][:8]
pat = re.compile(r"seq\s+(\d{1,4})\s*(?:\|\s*([0-9a-f]{7,8}))?")
bad = []; tot = 0; files = sorted(glob.glob(os.path.join(base, "reports/history/STAGE-*.md")) +
                                  glob.glob(os.path.join(base, "reports/history/00_OVERVIEW.md")) +
                                  glob.glob(os.path.join(base, "reports/history/slices/*.md")) +
                                  glob.glob(os.path.join(base, "_evidence/commits/逐条详析/*.md")) +
                                  glob.glob(os.path.join(base, "reports/packs/*.md")) +
                                  glob.glob(os.path.join(base, "大报告_*.md")))
for p in files:
    t = open(p, encoding="utf-8").read()
    for m in pat.finditer(t):
        seq = int(m.group(1)); sha = m.group(2)
        if seq not in idx:
            bad.append((os.path.basename(p), "seq 越界", seq, sha or "")); continue
        tot += 1
        if sha and not idx[seq].startswith(sha[:7]):
            bad.append((os.path.basename(p), "seq 与 sha 不匹配", seq, sha + " 应为 " + idx[seq]))
print("检查文件 %d 份；seq 引用 %d 次；异常 %d 处" % (len(files), tot, len(bad)))
for b in bad[:25]: print("  ", b)
