import glob, re
BT = chr(96)*3
bad = []
for p in glob.glob("/workspace/Astro CS Database/设计大纲/reports/**/*.md", recursive=True) + glob.glob("/workspace/Astro CS Database/设计大纲/大报告_*.md") + glob.glob("/workspace/Astro CS Database/设计大纲/_evidence/commits/逐条详析/*.md"):
    t = open(p, encoding="utf-8").read()
    fences = t.count(BT)
    longb = [b for b in re.findall(BT + r"(.*?)" + BT, t, re.S) if len(b.splitlines()) > 6]
    diffy = len(re.findall(r"^[-+]\s{0,4}(?:#include|if \(|for \(|return |\w+\s*\(\s*\w+\s*\)\s*[;{])", t, re.M))
    if fences or longb or diffy >= 3:
        bad.append((p.split("报告")[-1], fences, len(longb), diffy))
print("报告文件总数:", len(glob.glob("/workspace/Astro CS Database/设计大纲/reports/**/*.md", recursive=True)))
print("疑似夹带代码块/diff 的文件数:", len(bad))
for x in sorted(bad)[:12]: print("   ", x)
