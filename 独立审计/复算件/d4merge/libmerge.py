# -*- coding: utf-8 -*-
"""D4 合并器：机械穷举层 + 四路判读层 + 四链路报告常数条目 → 总台账主表。
只装配、不判读；全部字段取自已落盘成稿，冲突按判读已立案者打红。"""
import csv, io, os, re, collections, json

BASE = r"产出/"
OUT = os.path.join(BASE, "复算/d4merge")

EXT = (r"(?:cpp|hpp|h|json|md|py|sh|in|cmake|yaml|yml|tsv|csv|txt|registry|hpp\.in)")
# 形如 path/to/file.ext:123 或 path/to/file.ext:123-456
POS_RE = re.compile(
    r"[A-Za-z0-9_\-\./\u4e00-\u9fff]*[A-Za-z0-9_\-\u4e00-\u9fff]\." + EXT + r"(?::\d+(?:\s*[-,]\s*\d+)*)?")
LINE_RE = re.compile(r"\.(" + EXT + r"):(\d+(?:\s*[-,]\s*\d+)*)")

SKIP_PREFIX = ("docs/", "run/", "artifacts/", "产出/", "实验/", "standards/", "eng/tools/",
               "eng/ci/", "eng/contracts/", "eng/tests/", "eng/packaging/", "lib/", "根 ",
               "同 ", "该 ", "本 ", "见 ", "在 ")

def extract_paths(*texts):
    """返回规范化 路径:行 集合（仓根相对）。"""
    got = []
    for t in texts:
        if not t:
            continue
        for m in POS_RE.finditer(t):
            s = m.group(0).strip()
            s = s.lstrip("/").replace("\\", "/")
            if s.startswith((".json", ".md", ".h", ".cpp")):
                continue
            got.append(s)
    # 收尾去噪：去掉过短、纯扩展名
    outp = []
    for s in got:
        if len(s) < 5:
            continue
        outp.append(s)
    return outp


if __name__ == "__main__":
    def load(p):
        with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
            return list(csv.reader(f))[1:]
    jud = load("raw/AUD-402-判读-A1.csv") + load("raw/AUD-402-判读-A2.csv") + \
          load("raw/AUD-402-判读-A3.csv") + load("raw/AUD-402-判读-BD1.csv")
    o = io.open(os.path.join(OUT, "paths_probe.txt"), "w", encoding="utf-8")
    cnt = collections.Counter()
    for r in jud:
        ps = extract_paths(r[1], r[8], r[11])
        for p in ps:
            cnt[p] += 1
    o.write("唯一提取到的路径串 %d；总出现 %d\n\n" % (len(cnt), sum(cnt.values())))
    for k, v in cnt.most_common(160):
        o.write("%4d  %s\n" % (v, k))
    o.close()
    print("ok")
